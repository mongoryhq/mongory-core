#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mongory-core/foundations/memory_pool.h>
#include <mongory-core/foundations/value.h>
#include <mongory-core/foundations/table.h>
#include <mongory-core/foundations/array.h>
#include <mongory-core/foundations/config.h>

char* mongory_type_to_string(mongory_value *value) {
  switch (value->type) {
#define CASE_GEN(name, num, str, field) case name: return str;
  MONGORY_TYPE_MACRO(CASE_GEN)
#undef CASE_GEN
  default:
      return "UnknownType";
  }
}

char* mongory_value_to_string(mongory_value *value) {
  char *buffer = NULL;
  switch (value->type) {
  case MONGORY_TYPE_INT:
    buffer = value->pool->alloc(value->pool->ctx, 32);
    snprintf(buffer, 32, "%lld", value->data.i);
    return buffer;
  case MONGORY_TYPE_DOUBLE:
    buffer = value->pool->alloc(value->pool->ctx, 32);
    snprintf(buffer, 32, "%f", value->data.d);
    return buffer;
  case MONGORY_TYPE_STRING:
    buffer = value->pool->alloc(value->pool->ctx, 32);
    snprintf(buffer, 32, "\"%s\"", value->data.s);
    return buffer;
  case MONGORY_TYPE_ARRAY:
    return "Array Value";
  case MONGORY_TYPE_TABLE:
    return "Table Value";
  case MONGORY_TYPE_REGEX:
    return "Regex";
  case MONGORY_TYPE_POINTER:
    return "Pointer Value";
  case MONGORY_TYPE_UNSUPPORTED:
    return "Unsupported";
  default:
      return "UnknownType";
  }
}

void* mongory_value_extract(mongory_value *value) {
  switch (value->type) {
#define EXTRACT_CASE(name, num, str, field) case name: return (void *)&value->data.field;
  MONGORY_TYPE_MACRO(EXTRACT_CASE)
#undef EXTRACT_CASE
  default:
      return NULL;
  }
}

static inline mongory_value* mongory_value_new(mongory_memory_pool *pool) {
  mongory_value *value = pool->alloc(pool->ctx, sizeof(mongory_value));
  if (!value) {
    return NULL;
  }
  value->pool = pool;
  return value;
}

static inline int mongory_value_null_compare(mongory_value *a, mongory_value *b) {
  (void)a;
  if (b->type != MONGORY_TYPE_NULL) {
    return mongory_value_compare_fail;
  }

  return 0;
}

mongory_value* mongory_value_wrap_n(mongory_memory_pool *pool, void *n) {
  (void)n;
  mongory_value *value = mongory_value_new(pool);
  if (!value) {
    return NULL;
  }
  value->type = MONGORY_TYPE_NULL;
  value->comp = mongory_value_null_compare;
  return value;
}

static inline int mongory_value_bool_compare(mongory_value *a, mongory_value *b) {
  if (b->type != MONGORY_TYPE_BOOL || a->data.b != b->data.b) {
    return mongory_value_compare_fail;
  }

  return 0;
}

mongory_value* mongory_value_wrap_b(mongory_memory_pool *pool, bool b) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) {
    return NULL;
  }
  value->type = MONGORY_TYPE_BOOL;
  value->data.b = b;
  value->comp = mongory_value_bool_compare;
  return value;
}

static inline int mongory_value_int_compare(mongory_value *a, mongory_value *b) {
  if (b->type == MONGORY_TYPE_DOUBLE) {
    double a_as_double = (double)a->data.i;
    double b_value = b->data.d;
    return (a_as_double > b_value) - (a_as_double < b_value);
  }

  if (b->type == MONGORY_TYPE_INT) {
    int64_t a_value = a->data.i;
    int64_t b_value = b->data.i;
    return (a_value > b_value) - (a_value < b_value);
  }

  return mongory_value_compare_fail;
}

mongory_value* mongory_value_wrap_i(mongory_memory_pool *pool, int i) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) {
    return NULL;
  }
  value->type = MONGORY_TYPE_INT;
  value->data.i = i;
  value->comp = mongory_value_int_compare;
  return value;
}

static inline int mongory_value_double_compare(mongory_value *a, mongory_value *b) {
  if (b->type == MONGORY_TYPE_DOUBLE) {
    double a_value = a->data.d;
    double b_value = b->data.d;
    return (a_value > b_value) - (a_value < b_value);
  }

  if (b->type == MONGORY_TYPE_INT) {
    double a_value = a->data.d;
    double b_as_double = (double)b->data.i;
    return (a_value > b_as_double) - (a_value < b_as_double);
  }

  return mongory_value_compare_fail;
}

mongory_value* mongory_value_wrap_d(mongory_memory_pool *pool, double d) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) {
    return NULL;
  }
  value->type = MONGORY_TYPE_DOUBLE;
  value->data.d = d;
  value->comp = mongory_value_double_compare;
  return value;
}

static inline int mongory_value_string_compare(mongory_value *a, mongory_value *b) {
  if (b->type != MONGORY_TYPE_STRING || a->data.s == NULL || b->data.s == NULL) {
    return mongory_value_compare_fail;
  }

  int cmp_result = strcmp(a->data.s, b->data.s);
  return (cmp_result > 0) - (cmp_result < 0);
}

mongory_value* mongory_value_wrap_s(mongory_memory_pool *pool, char *s) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) {
    return NULL;
  }
  value->type = MONGORY_TYPE_STRING;
  value->data.s = mongory_string_cpy(pool, s);
  value->comp = mongory_value_string_compare;
  return value;
}

static inline int mongory_value_array_compare(mongory_value *a, mongory_value *b) {
  if (b->type != MONGORY_TYPE_ARRAY || a->data.a == NULL || b->data.a == NULL) {
    return mongory_value_compare_fail;
  }
  mongory_array *array_a = a->data.a;
  mongory_array *array_b = b->data.a;
  if (array_a->count != array_b->count) {
    return mongory_value_compare_fail;
  }
  for (size_t i = 0; i < array_a->count; i++) {
    mongory_value *item_a = array_a->get(array_a, i);
    mongory_value *item_b = array_b->get(array_b, i);
    bool a_is_null = item_a == NULL;
    bool b_is_null = item_b == NULL;
    if (a_is_null != b_is_null) {
      return mongory_value_compare_fail;
    }
    if (a_is_null && b_is_null) {
      continue;
    }
    int cmp_result = item_a->comp(item_a, item_b);
    if (cmp_result != 0) {
      return cmp_result;
    }
  }
  return 0;
}

mongory_value* mongory_value_wrap_a(mongory_memory_pool *pool, struct mongory_array *a) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) {
    return NULL;
  }
  value->type = MONGORY_TYPE_ARRAY;
  value->data.a = a;
  value->comp = mongory_value_array_compare;
  return value;
}

static inline int mongory_value_table_compare(mongory_value *a, mongory_value *b) {
  (void)a;
  (void)b;
  return mongory_value_compare_fail;
}

mongory_value* mongory_value_wrap_t(mongory_memory_pool *pool, struct mongory_table *t) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) {
    return NULL;
  }
  value->type = MONGORY_TYPE_TABLE;
  value->data.t = t;
  value->comp = mongory_value_table_compare;
  return value;
}

static inline int mongory_value_unknown_compare(mongory_value *a, mongory_value *b) {
  (void)a;
  (void)b;
  return mongory_value_compare_fail;
}

mongory_value* mongory_value_wrap_u(mongory_memory_pool *pool, void *unknown_value) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) {
    return NULL;
  }
  value->type = MONGORY_TYPE_UNSUPPORTED;
  value->data.u = unknown_value;
  value->comp = mongory_value_unknown_compare;
  return value;
}

static inline int mongory_value_regex_compare(mongory_value *a, mongory_value *b) {
  (void)a;
  (void)b;
  return mongory_value_compare_fail;
}

mongory_value* mongory_value_wrap_regex(mongory_memory_pool *pool, void *regex) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) {
    return NULL;
  }
  value->type = MONGORY_TYPE_REGEX;
  value->data.regex = regex;
  value->comp = mongory_value_regex_compare;
  return value;
}

static inline int mongory_value_ptr_compare(mongory_value *a, mongory_value *b) {
  (void)a;
  (void)b;
  return mongory_value_compare_fail;
}

mongory_value* mongory_value_wrap_ptr(mongory_memory_pool *pool, void *ptr) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) {
    return NULL;
  }
  value->type = MONGORY_TYPE_POINTER;
  value->data.ptr = ptr;
  value->comp = mongory_value_ptr_compare;
  return value;
}