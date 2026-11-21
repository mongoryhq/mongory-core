/**
 * @file json_adapter.c
 * @brief Shallow-wrapper raw JSON adapter for mongory-core backed by yyjson.
 */
#include "mongory-core/adapters/json.h"
#include "../foundations/utils.h"
#include <limits.h>
#include <mongory-core.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

#define MONGORY_JSON_STACK_BUF_SIZE (16u * 1024u)
#define MONGORY_JSON_CTX_INIT_BUF_SIZE (64u * 1024u)

// ============================================================================
// Lazy shallow wrappers: yyjson_val* -> mongory_table / mongory_array / mongory_value
// ============================================================================

static mongory_value *yyjson_mongory_value_wrap_shallow(mongory_memory_pool *pool, yyjson_val *val);

typedef struct yyjson_mongory_table_t {
  mongory_table base;
  yyjson_val *obj;
} yyjson_mongory_table_t;

typedef struct yyjson_mongory_array_t {
  mongory_array base;
  yyjson_val *arr;
  yyjson_val **elems; // Lazily populated O(1) index table on first get()
} yyjson_mongory_array_t;

static mongory_value *yyjson_table_get(mongory_table *self, char *key) {
  if (!self || !key) {
    return NULL;
  }
  yyjson_mongory_table_t *tbl = (yyjson_mongory_table_t *)self;
  yyjson_val *val = yyjson_obj_get(tbl->obj, key);
  if (!val) {
    return NULL;
  }
  return yyjson_mongory_value_wrap_shallow(self->pool, val);
}

static bool yyjson_table_each(mongory_table *self, void *acc, mongory_table_each_pair_callback_func callback) {
  if (!self || !callback) {
    return false;
  }
  yyjson_mongory_table_t *tbl = (yyjson_mongory_table_t *)self;
  size_t idx, max;
  yyjson_val *key, *val;
  yyjson_obj_foreach(tbl->obj, idx, max, key, val) {
    const char *k = yyjson_get_str(key);
    mongory_value *wrapped = yyjson_mongory_value_wrap_shallow(self->pool, val);
    if (!callback((char *)k, wrapped, acc)) {
      return false;
    }
  }
  return true;
}

static bool yyjson_table_set_readonly(mongory_table *self, char *key, mongory_value *value) {
  (void)self;
  (void)key;
  (void)value;
  return false;
}

static bool yyjson_table_del_readonly(mongory_table *self, char *key) {
  (void)self;
  (void)key;
  return false;
}

static mongory_table *yyjson_mongory_table_wrap(mongory_memory_pool *pool, yyjson_val *obj) {
  yyjson_mongory_table_t *tbl = MG_ALLOC_PTR(pool, yyjson_mongory_table_t);
  if (!tbl) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }
  tbl->base.pool = pool;
  tbl->base.count = yyjson_obj_size(obj);
  tbl->base.get = yyjson_table_get;
  tbl->base.each = yyjson_table_each;
  tbl->base.set = yyjson_table_set_readonly;
  tbl->base.del = yyjson_table_del_readonly;
  tbl->obj = obj;
  return &tbl->base;
}

static mongory_value *yyjson_array_get(mongory_array *self, size_t index) {
  if (!self || index >= self->count) {
    return NULL;
  }
  yyjson_mongory_array_t *arr = (yyjson_mongory_array_t *)self;
  if (!arr->elems) {
    arr->elems = MG_ALLOC_ARY(self->pool, yyjson_val *, self->count);
    if (!arr->elems) {
      self->pool->error = &MONGORY_ALLOC_ERROR;
      return NULL;
    }
    size_t idx, max;
    yyjson_val *item;
    yyjson_arr_foreach(arr->arr, idx, max, item) {
      arr->elems[idx] = item;
    }
  }
  return yyjson_mongory_value_wrap_shallow(self->pool, arr->elems[index]);
}

static bool yyjson_array_each(mongory_array *self, void *acc, mongory_array_callback_func callback) {
  if (!self || !callback) {
    return false;
  }
  yyjson_mongory_array_t *arr = (yyjson_mongory_array_t *)self;
  size_t idx, max;
  yyjson_val *item;
  yyjson_arr_foreach(arr->arr, idx, max, item) {
    mongory_value *wrapped = yyjson_mongory_value_wrap_shallow(self->pool, item);
    if (!callback(wrapped, acc)) {
      return false;
    }
  }
  return true;
}

static bool yyjson_array_push_readonly(mongory_array *self, mongory_value *value) {
  (void)self;
  (void)value;
  return false;
}

static bool yyjson_array_set_readonly(mongory_array *self, size_t index, mongory_value *value) {
  (void)self;
  (void)index;
  (void)value;
  return false;
}

static mongory_array *yyjson_mongory_array_wrap(mongory_memory_pool *pool, yyjson_val *arr_val) {
  yyjson_mongory_array_t *arr = MG_ALLOC_PTR(pool, yyjson_mongory_array_t);
  if (!arr) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }
  arr->base.count = yyjson_arr_size(arr_val);
  arr->base.pool = pool;
  arr->base.each = yyjson_array_each;
  arr->base.push = yyjson_array_push_readonly;
  arr->base.get = yyjson_array_get;
  arr->base.set = yyjson_array_set_readonly;
  arr->arr = arr_val;
  arr->elems = NULL;
  return &arr->base;
}

static mongory_value *yyjson_mongory_value_wrap_shallow(mongory_memory_pool *pool, yyjson_val *val) {
  if (!val) {
    return NULL;
  }
  switch (yyjson_get_type(val)) {
  case YYJSON_TYPE_NULL:
    return mongory_value_wrap_n(pool, NULL);
  case YYJSON_TYPE_BOOL:
    return mongory_value_wrap_b(pool, yyjson_get_bool(val));
  case YYJSON_TYPE_NUM:
    if (yyjson_is_sint(val)) {
      return mongory_value_wrap_i(pool, yyjson_get_sint(val));
    }
    if (yyjson_is_uint(val)) {
      uint64_t u = yyjson_get_uint(val);
      if (u <= (uint64_t)INT64_MAX) {
        return mongory_value_wrap_i(pool, (int64_t)u);
      }
      return mongory_value_wrap_d(pool, (double)u);
    }
    return mongory_value_wrap_d(pool, yyjson_get_real(val));
  case YYJSON_TYPE_STR:
    return mongory_value_wrap_s(pool, (char *)yyjson_get_str(val));
  case YYJSON_TYPE_OBJ: {
    mongory_table *tbl = yyjson_mongory_table_wrap(pool, val);
    return tbl ? mongory_value_wrap_t(pool, tbl) : NULL;
  }
  case YYJSON_TYPE_ARR: {
    mongory_array *arr = yyjson_mongory_array_wrap(pool, val);
    return arr ? mongory_value_wrap_a(pool, arr) : NULL;
  }
  default:
    return mongory_value_wrap_n(pool, NULL);
  }
}

// ============================================================================
// Deep conversion for condition compilation (condition must outlive yyjson_doc)
// ============================================================================

static mongory_value *yyjson_mongory_value_deep_convert(mongory_memory_pool *pool, yyjson_val *val) {
  if (!val || pool->error != NULL) {
    return NULL;
  }
  switch (yyjson_get_type(val)) {
  case YYJSON_TYPE_OBJ: {
    mongory_table *tbl = mongory_table_new(pool);
    if (!tbl) return NULL;
    size_t idx, max;
    yyjson_val *key, *child;
    yyjson_obj_foreach(val, idx, max, key, child) {
      mongory_value *v = yyjson_mongory_value_deep_convert(pool, child);
      if (!v || !tbl->set(tbl, (char *)yyjson_get_str(key), v)) {
        return NULL;
      }
    }
    return mongory_value_wrap_t(pool, tbl);
  }
  case YYJSON_TYPE_ARR: {
    mongory_array *arr = mongory_array_new(pool);
    if (!arr) return NULL;
    size_t idx, max;
    yyjson_val *child;
    yyjson_arr_foreach(val, idx, max, child) {
      mongory_value *v = yyjson_mongory_value_deep_convert(pool, child);
      if (!v || !arr->push(arr, v)) {
        return NULL;
      }
    }
    return mongory_value_wrap_a(pool, arr);
  }
  default:
    return yyjson_mongory_value_wrap_shallow(pool, val);
  }
}

// ============================================================================
// Public API
// ============================================================================

mongory_matcher *mongory_matcher_new_from_json(mongory_memory_pool *pool,
                                               const char *json_str,
                                               size_t len,
                                               void *extern_ctx) {
  if (!pool) {
    return NULL;
  }
  if (!json_str || len == 0) {
    mongory_error_set(pool, MONGORY_ERROR_PARSE, "Empty JSON condition string");
    return NULL;
  }

  yyjson_read_err err;
  yyjson_doc *doc = yyjson_read_opts((char *)json_str, len, 0, NULL, &err);
  if (!doc) {
    char *msg = mongory_string_cpyf(pool, "JSON parse error: %s (at byte %zu)",
                                    err.msg ? err.msg : "invalid JSON", err.pos);
    mongory_error_set(pool, MONGORY_ERROR_PARSE, msg ? msg : "JSON parse error");
    return NULL;
  }

  mongory_value *cond = yyjson_mongory_value_deep_convert(pool, yyjson_doc_get_root(doc));
  yyjson_doc_free(doc);
  if (!cond || pool->error != NULL) {
    return NULL;
  }
  return mongory_matcher_new(pool, cond, extern_ctx);
}

static bool match_json_doc_with_pool(mongory_matcher *matcher, mongory_memory_pool *scratch_pool, yyjson_doc *doc) {
  yyjson_val *root = yyjson_doc_get_root(doc);
  if (!root) {
    return false;
  }
  mongory_value *wrapped = yyjson_mongory_value_wrap_shallow(scratch_pool, root);
  if (!wrapped || scratch_pool->error != NULL) {
    return false;
  }
  return mongory_matcher_match(matcher, wrapped);
}

bool mongory_matcher_match_json(mongory_matcher *matcher, const char *json_str, size_t len) {
  if (!matcher || !json_str || len == 0) {
    return false;
  }

  char stack_buf[MONGORY_JSON_STACK_BUF_SIZE];
  yyjson_alc alc;
  yyjson_read_err err;
  yyjson_doc *doc = NULL;
  bool used_stack_alc = false;

  if (yyjson_alc_pool_init(&alc, stack_buf, sizeof(stack_buf))) {
    doc = yyjson_read_opts((char *)json_str, len, 0, &alc, &err);
    used_stack_alc = (doc != NULL);
  }
  if (!doc && err.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION) {
    doc = yyjson_read_opts((char *)json_str, len, 0, NULL, &err);
    used_stack_alc = false;
  }
  if (!doc) {
    return false;
  }

  mongory_memory_pool *scratch_pool = mongory_memory_pool_new();
  if (!scratch_pool) {
    if (!used_stack_alc) yyjson_doc_free(doc);
    return false;
  }

  bool matched = match_json_doc_with_pool(matcher, scratch_pool, doc);
  scratch_pool->free(scratch_pool);
  if (!used_stack_alc) {
    yyjson_doc_free(doc);
  }
  return matched;
}

struct mongory_json_match_ctx {
  mongory_memory_pool *scratch_pool;
  void *yyjson_buf;
  size_t yyjson_buf_size;
};

mongory_json_match_ctx *mongory_json_match_ctx_new(void) {
  mongory_json_match_ctx *ctx = (mongory_json_match_ctx *)calloc(1, sizeof(mongory_json_match_ctx));
  if (!ctx) {
    return NULL;
  }
  ctx->scratch_pool = mongory_memory_pool_new();
  if (!ctx->scratch_pool) {
    free(ctx);
    return NULL;
  }
  ctx->yyjson_buf = malloc(MONGORY_JSON_CTX_INIT_BUF_SIZE);
  if (!ctx->yyjson_buf) {
    ctx->scratch_pool->free(ctx->scratch_pool);
    free(ctx);
    return NULL;
  }
  ctx->yyjson_buf_size = MONGORY_JSON_CTX_INIT_BUF_SIZE;
  return ctx;
}

void mongory_json_match_ctx_free(mongory_json_match_ctx *ctx) {
  if (!ctx) {
    return;
  }
  if (ctx->scratch_pool) {
    ctx->scratch_pool->free(ctx->scratch_pool);
  }
  free(ctx->yyjson_buf);
  free(ctx);
}

bool mongory_matcher_match_json_with_ctx(mongory_matcher *matcher,
                                         mongory_json_match_ctx *ctx,
                                         const char *json_str,
                                         size_t len) {
  if (!matcher || !ctx || !json_str || len == 0) {
    return false;
  }

  yyjson_alc alc;
  yyjson_read_err err;
  yyjson_doc *doc = NULL;
  bool used_ctx_alc = false;

  if (ctx->yyjson_buf && yyjson_alc_pool_init(&alc, ctx->yyjson_buf, ctx->yyjson_buf_size)) {
    doc = yyjson_read_opts((char *)json_str, len, 0, &alc, &err);
    used_ctx_alc = (doc != NULL);
  }
  if (!doc && err.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION) {
    // Grow reusable buffer if payload needs more room, or fall back to heap for extreme inputs.
    size_t needed = yyjson_read_max_memory_usage(len, 0);
    if (needed > ctx->yyjson_buf_size && needed <= (16u * 1024u * 1024u)) {
      void *grown = malloc(needed);
      if (grown) {
        free(ctx->yyjson_buf);
        ctx->yyjson_buf = grown;
        ctx->yyjson_buf_size = needed;
        if (yyjson_alc_pool_init(&alc, ctx->yyjson_buf, ctx->yyjson_buf_size)) {
          doc = yyjson_read_opts((char *)json_str, len, 0, &alc, &err);
          used_ctx_alc = (doc != NULL);
        }
      }
    }
    if (!doc) {
      doc = yyjson_read_opts((char *)json_str, len, 0, NULL, &err);
      used_ctx_alc = false;
    }
  }
  if (!doc) {
    return false;
  }

  ctx->scratch_pool->reset(ctx->scratch_pool);
  ctx->scratch_pool->error = NULL;
  bool matched = match_json_doc_with_pool(matcher, ctx->scratch_pool, doc);
  if (!used_ctx_alc) {
    yyjson_doc_free(doc);
  }
  return matched;
}
