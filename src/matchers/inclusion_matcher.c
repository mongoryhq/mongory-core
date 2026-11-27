/**
 * @file inclusion_matcher.c
 * @brief Implements $in and $nin matchers for checking value inclusion in an
 * array. Uses an immutable, arena-allocated, type-bucketed O(1) hash set for
 * condition arrays with >= 16 elements while falling back to linear scan for
 * small arrays.
 */
#include "inclusion_matcher.h"
#include "base_matcher.h"                   // For mongory_matcher_base_new
#include "../foundations/array_private.h"   // For mongory_array_includes
#include "mongory-core/foundations/array.h" // For mongory_array operations
#include "mongory-core/foundations/error.h" // For mongory_error
#include "mongory-core/foundations/value.h" // For mongory_value
#include <math.h>                           // For floor, isnan, isfinite, fabs
#include <mongory-core.h>                   // General include
#include <stdint.h>                         // For uint64_t, int64_t, INT64_C
#include <string.h>                         // For strlen, memcmp, memset, memcpy
#include "../foundations/utils.h"           // For mongory_string_cpy, mongory_log

#define MONGORY_IN_HASH_THRESHOLD 16
#define MONGORY_IN_SAFE_INT_MAX (INT64_C(1) << 53)

typedef struct mongory_in_str_slot {
  const char *str;
  size_t len;
  uint64_t hash;
} mongory_in_str_slot;

typedef enum mongory_in_num_kind {
  MONGORY_IN_NUM_EMPTY = 0,
  MONGORY_IN_NUM_INT = 1,
  MONGORY_IN_NUM_DOUBLE = 2
} mongory_in_num_kind;

typedef struct mongory_in_num_slot {
  mongory_in_num_kind kind;
  uint64_t key_bits; // int64_t bits (for INT) or IEEE-754 double bits (for DOUBLE)
  uint64_t hash;
  mongory_value *val;
} mongory_in_num_slot;

typedef struct mongory_in_set {
  bool has_null;
  bool has_true;
  bool has_false;

  mongory_in_str_slot *str_slots;
  size_t str_mask;

  mongory_in_num_slot *num_slots;
  size_t num_mask;

  mongory_value **fallback;
  size_t fallback_count;
} mongory_in_set;

typedef struct mongory_inclusion_matcher {
  mongory_matcher base;
  mongory_in_set *set;
} mongory_inclusion_matcher;

static inline uint64_t mongory_fnv1a_64(const char *s, size_t len) {
  uint64_t h = UINT64_C(14695981039346656037);
  for (size_t i = 0; i < len; i++) {
    h ^= (uint64_t)(unsigned char)s[i];
    h *= UINT64_C(1099511628211);
  }
  return h;
}

static inline uint64_t mongory_splitmix64(uint64_t x) {
  x += UINT64_C(0x9e3779b97f4a7c15);
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  return x ^ (x >> 31);
}

static inline size_t mongory_next_pow2(size_t n) {
  size_t cap = 32;
  while (cap < n && cap < ((size_t)1 << (sizeof(size_t) * 8 - 1))) {
    cap <<= 1;
  }
  return cap;
}

/**
 * Classifies a numeric mongory_value into a canonical hashable key:
 * - Returns MONGORY_IN_NUM_INT for exact integers within [-2^53, 2^53] (including
 *   integral doubles like 1.0 and -0.0), so 1 == 1.0 and -0.0 == 0 share the same key.
 * - Returns MONGORY_IN_NUM_DOUBLE for finite non-integer doubles within [-2^53, 2^53]
 *   and +/-Infinity.
 * - Returns MONGORY_IN_NUM_EMPTY if the value is NaN (never matches) or exceeds
 *   safe exact-integer range (caller routes large numbers to fallback).
 */
static inline mongory_in_num_kind mongory_in_classify_num(const mongory_value *v, uint64_t *out_bits, bool *out_is_nan) {
  *out_is_nan = false;
  if (v->type == MONGORY_TYPE_INT) {
    int64_t i = v->data.i;
    if (i >= -MONGORY_IN_SAFE_INT_MAX && i <= MONGORY_IN_SAFE_INT_MAX) {
      *out_bits = (uint64_t)i;
      return MONGORY_IN_NUM_INT;
    }
    return MONGORY_IN_NUM_EMPTY;
  }
  if (v->type == MONGORY_TYPE_DOUBLE) {
    double d = v->data.d;
    if (isnan(d)) {
      *out_is_nan = true;
      return MONGORY_IN_NUM_EMPTY;
    }
    if (d == 0.0) {
      *out_bits = 0;
      return MONGORY_IN_NUM_INT;
    }
    if (isfinite(d)) {
      if (fabs(d) > (double)MONGORY_IN_SAFE_INT_MAX) {
        return MONGORY_IN_NUM_EMPTY;
      }
      if (floor(d) == d) {
        int64_t i = (int64_t)d;
        *out_bits = (uint64_t)i;
        return MONGORY_IN_NUM_INT;
      }
    }
    uint64_t bits = 0;
    memcpy(&bits, &d, sizeof(bits));
    *out_bits = bits;
    return MONGORY_IN_NUM_DOUBLE;
  }
  return MONGORY_IN_NUM_EMPTY;
}

static mongory_in_set *mongory_in_set_build(mongory_memory_pool *pool, mongory_array *arr) {
  size_t count = arr->count;
  size_t str_count = 0;
  size_t num_count = 0;
  size_t fb_count = 0;

  for (size_t i = 0; i < count; i++) {
    mongory_value *elem = arr->get(arr, i);
    if (!elem) {
      continue;
    }
    switch (elem->type) {
    case MONGORY_TYPE_NULL:
    case MONGORY_TYPE_BOOL:
      break;
    case MONGORY_TYPE_STRING:
      if (elem->data.s != NULL) {
        str_count++;
      }
      break;
    case MONGORY_TYPE_INT:
    case MONGORY_TYPE_DOUBLE: {
      uint64_t bits = 0;
      bool is_nan = false;
      if (mongory_in_classify_num(elem, &bits, &is_nan) != MONGORY_IN_NUM_EMPTY) {
        num_count++;
      } else if (!is_nan) {
        fb_count++;
      }
      break;
    }
    default:
      fb_count++;
      break;
    }
  }

  mongory_in_set *set = MG_ALLOC_PTR(pool, mongory_in_set);
  if (!set) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }
  memset(set, 0, sizeof(*set));

  if (str_count > 0) {
    size_t cap = mongory_next_pow2(str_count * 2);
    set->str_slots = (mongory_in_str_slot *)pool->alloc(pool, cap * sizeof(mongory_in_str_slot));
    if (!set->str_slots) {
      pool->error = &MONGORY_ALLOC_ERROR;
      return NULL;
    }
    memset(set->str_slots, 0, cap * sizeof(mongory_in_str_slot));
    set->str_mask = cap - 1;
  }

  if (num_count > 0) {
    size_t cap = mongory_next_pow2(num_count * 2);
    set->num_slots = (mongory_in_num_slot *)pool->alloc(pool, cap * sizeof(mongory_in_num_slot));
    if (!set->num_slots) {
      pool->error = &MONGORY_ALLOC_ERROR;
      return NULL;
    }
    memset(set->num_slots, 0, cap * sizeof(mongory_in_num_slot));
    set->num_mask = cap - 1;
  }

  if (fb_count > 0) {
    set->fallback = (mongory_value **)pool->alloc(pool, fb_count * sizeof(mongory_value *));
    if (!set->fallback) {
      pool->error = &MONGORY_ALLOC_ERROR;
      return NULL;
    }
  }

  for (size_t i = 0; i < count; i++) {
    mongory_value *elem = arr->get(arr, i);
    if (!elem) {
      continue;
    }
    switch (elem->type) {
    case MONGORY_TYPE_NULL:
      set->has_null = true;
      break;
    case MONGORY_TYPE_BOOL:
      if (elem->data.b) {
        set->has_true = true;
      } else {
        set->has_false = true;
      }
      break;
    case MONGORY_TYPE_STRING: {
      const char *s = elem->data.s;
      if (!s) {
        break;
      }
      size_t len = strlen(s);
      uint64_t h = mongory_fnv1a_64(s, len);
      size_t idx = (size_t)(h & set->str_mask);
      while (set->str_slots[idx].str != NULL) {
        if (set->str_slots[idx].hash == h && set->str_slots[idx].len == len &&
            memcmp(set->str_slots[idx].str, s, len) == 0) {
          break;
        }
        idx = (idx + 1) & set->str_mask;
      }
      if (set->str_slots[idx].str == NULL) {
        set->str_slots[idx].str = s;
        set->str_slots[idx].len = len;
        set->str_slots[idx].hash = h;
      }
      break;
    }
    case MONGORY_TYPE_INT:
    case MONGORY_TYPE_DOUBLE: {
      uint64_t bits = 0;
      bool is_nan = false;
      mongory_in_num_kind kind = mongory_in_classify_num(elem, &bits, &is_nan);
      if (kind != MONGORY_IN_NUM_EMPTY) {
        uint64_t h = mongory_splitmix64(bits ^ ((uint64_t)kind << 60));
        size_t idx = (size_t)(h & set->num_mask);
        while (set->num_slots[idx].kind != MONGORY_IN_NUM_EMPTY) {
          if (set->num_slots[idx].kind == kind && set->num_slots[idx].key_bits == bits) {
            break;
          }
          idx = (idx + 1) & set->num_mask;
        }
        if (set->num_slots[idx].kind == MONGORY_IN_NUM_EMPTY) {
          set->num_slots[idx].kind = kind;
          set->num_slots[idx].key_bits = bits;
          set->num_slots[idx].hash = h;
          set->num_slots[idx].val = elem;
        }
      } else if (!is_nan) {
        set->fallback[set->fallback_count++] = elem;
      }
      break;
    }
    default:
      set->fallback[set->fallback_count++] = elem;
      break;
    }
  }

  return set;
}

static inline bool mongory_in_set_contains_scalar(const mongory_in_set *set, mongory_value *v) {
  if (!v) {
    return false;
  }
  switch (v->type) {
  case MONGORY_TYPE_NULL:
    return set->has_null;
  case MONGORY_TYPE_BOOL:
    return v->data.b ? set->has_true : set->has_false;
  case MONGORY_TYPE_STRING: {
    if (!set->str_slots || !v->data.s) {
      return false;
    }
    const char *s = v->data.s;
    size_t len = strlen(s);
    uint64_t h = mongory_fnv1a_64(s, len);
    size_t idx = (size_t)(h & set->str_mask);
    while (set->str_slots[idx].str != NULL) {
      if (set->str_slots[idx].hash == h && set->str_slots[idx].len == len &&
          memcmp(set->str_slots[idx].str, s, len) == 0) {
        return true;
      }
      idx = (idx + 1) & set->str_mask;
    }
    return false;
  }
  case MONGORY_TYPE_INT:
  case MONGORY_TYPE_DOUBLE: {
    uint64_t bits = 0;
    bool is_nan = false;
    mongory_in_num_kind kind = mongory_in_classify_num(v, &bits, &is_nan);
    if (is_nan) {
      return false;
    }
    if (kind != MONGORY_IN_NUM_EMPTY) {
      if (!set->num_slots) {
        return false;
      }
      uint64_t h = mongory_splitmix64(bits ^ ((uint64_t)kind << 60));
      size_t idx = (size_t)(h & set->num_mask);
      while (set->num_slots[idx].kind != MONGORY_IN_NUM_EMPTY) {
        if (set->num_slots[idx].kind == kind && set->num_slots[idx].key_bits == bits &&
            set->num_slots[idx].val->comp(set->num_slots[idx].val, v) == 0) {
          return true;
        }
        idx = (idx + 1) & set->num_mask;
      }
      return false;
    }
    // Large integer/double outside exact 2^53 range -> check fallback
    for (size_t i = 0; i < set->fallback_count; i++) {
      mongory_value *cand = set->fallback[i];
      if (cand->comp(cand, v) == 0) {
        return true;
      }
    }
    return false;
  }
  default:
    for (size_t i = 0; i < set->fallback_count; i++) {
      mongory_value *cand = set->fallback[i];
      if (cand->comp(cand, v) == 0) {
        return true;
      }
    }
    return false;
  }
}

/**
 * @brief Validates that the condition for an inclusion matcher is a valid
 * array.
 */
static inline bool mongory_matcher_validate_array_condition(mongory_value *condition) {
  if (condition == NULL) {
    return false;
  }
  if (condition->type != MONGORY_TYPE_ARRAY) {
    return false;
  }
  if (condition->data.a == NULL) {
    return false;
  }
  return true;
}

static inline bool mongory_matcher_in_match(mongory_matcher *matcher, mongory_value *value_to_check) {
  if (!value_to_check || !matcher->condition || !matcher->condition->data.a) {
    return false;
  }

  mongory_inclusion_matcher *inc = (mongory_inclusion_matcher *)matcher;
  mongory_in_set *set = inc->set;

  if (set != NULL) {
    if (value_to_check->type != MONGORY_TYPE_ARRAY) {
      return mongory_in_set_contains_scalar(set, value_to_check);
    }
    mongory_array *input_array = value_to_check->data.a;
    if (!input_array) {
      return false;
    }
    for (size_t i = 0; i < input_array->count; i++) {
      mongory_value *input_item = input_array->get(input_array, i);
      if (mongory_in_set_contains_scalar(set, input_item)) {
        return true;
      }
    }
    return false;
  }

  mongory_array *condition_array = matcher->condition->data.a;

  if (value_to_check->type != MONGORY_TYPE_ARRAY) {
    return mongory_array_includes(condition_array, value_to_check);
  }

  mongory_array *input_array = value_to_check->data.a;
  if (!input_array) {
    return false;
  }

  for (size_t i = 0; i < input_array->count; i++) {
    mongory_value *input_item = input_array->get(input_array, i);
    if (mongory_array_includes(condition_array, input_item)) {
      return true;
    }
  }

  return false;
}

static mongory_inclusion_matcher *mongory_inclusion_matcher_alloc(mongory_memory_pool *pool, mongory_value *condition,
                                                                  void *extern_ctx, const char *name,
                                                                  mongory_matcher_match_func match_fn) {
  if (!pool || !pool->alloc) {
    return NULL;
  }
  if (!mongory_compile_budget_count_node(pool)) {
    return NULL;
  }
  mongory_inclusion_matcher *inc = MG_ALLOC_PTR(pool, mongory_inclusion_matcher);
  if (!inc) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }

  inc->base.original_match = match_fn;
  inc->base.sub_count = 0;
  inc->base.pool = pool;
  inc->base.condition = condition;
  inc->base.name = mongory_string_cpy(pool, (char *)name);
  if (!inc->base.name) {
    return NULL;
  }
  inc->base.match = match_fn;
  inc->base.explain = mongory_matcher_base_explain;
  inc->base.traverse = mongory_matcher_leaf_traverse;
  inc->base.trace_stack = NULL;
  inc->base.trace_level = 0;
  inc->base.extern_ctx = extern_ctx;
  inc->base.parent = NULL;
  inc->set = NULL;

  mongory_array *cond_arr = condition->data.a;
  if (cond_arr->count >= MONGORY_IN_HASH_THRESHOLD) {
    inc->set = mongory_in_set_build(pool, cond_arr);
    if (!inc->set) {
      return NULL;
    }
    inc->base.priority = 3.0 + (double)(inc->set->fallback_count / 8);
  } else {
    inc->base.priority = 2.0 + (double)(cond_arr->count / 8);
  }

  return inc;
}

mongory_matcher *mongory_matcher_in_new(mongory_memory_pool *pool, mongory_value *condition, void *extern_ctx) {
  if (!mongory_matcher_validate_array_condition(condition)) {
    mongory_error_set(pool, MONGORY_ERROR_INVALID_ARGUMENT, "$in condition must be a valid array.");
    return NULL;
  }
  mongory_inclusion_matcher *inc =
      mongory_inclusion_matcher_alloc(pool, condition, extern_ctx, "In", mongory_matcher_in_match);
  return inc ? &inc->base : NULL;
}

static inline bool mongory_matcher_not_in_match(mongory_matcher *matcher, mongory_value *value) {
  return !mongory_matcher_in_match(matcher, value);
}

mongory_matcher *mongory_matcher_not_in_new(mongory_memory_pool *pool, mongory_value *condition, void *extern_ctx) {
  if (!mongory_matcher_validate_array_condition(condition)) {
    mongory_error_set(pool, MONGORY_ERROR_INVALID_ARGUMENT, "$nin condition must be a valid array.");
    return NULL;
  }
  mongory_inclusion_matcher *inc =
      mongory_inclusion_matcher_alloc(pool, condition, extern_ctx, "Nin", mongory_matcher_not_in_match);
  if (inc) {
    inc->base.priority += 1.0;
  }
  return inc ? &inc->base : NULL;
}
