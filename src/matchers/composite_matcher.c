#include <mongory-core.h>
#include "base_matcher.h"
#include "composite_matcher.h"
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "literal_matcher.h"
#include "../foundations/iterable.h"
#include "../foundations/config_private.h"

mongory_composite_matcher* mongory_matcher_composite_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (matcher == NULL) {
    return NULL;
  }
  matcher->name = NULL;
  matcher->match = NULL;
  mongory_composite_matcher *composite = (mongory_composite_matcher *)pool->alloc(pool->ctx, sizeof(mongory_composite_matcher));
  if (composite == NULL) {
    return NULL;
  }
  composite->base = matcher;
  composite->left = NULL;
  composite->right = NULL;
  return composite;
}

bool mongory_matcher_and_match(mongory_matcher *matcher, mongory_value *value) {
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  if (composite->left && !composite->left->match(composite->left, value)) {
    return false;
  }
  if (composite->right && !composite->right->match(composite->right, value)) {
    return false;
  }
  return true;
}

bool mongory_matcher_or_match(mongory_matcher *matcher, mongory_value *value) {
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  if (composite->left && composite->left->match(composite->left, value)) {
    return true;
  }
  if (composite->right && composite->right->match(composite->right, value)) {
    return true;
  }
  return false;
}

bool mongory_matcher_table_cond_validate(mongory_value *condition, void *acc) {
  (void)acc;
  return condition && condition->type == MONGORY_TYPE_TABLE && condition->data.t != NULL;
}

typedef struct mongory_matcher_table_build_sub_matcher_context {
  mongory_memory_pool *pool;
  mongory_matcher **matchers;
  int index;
} mongory_matcher_table_build_sub_matcher_context;

bool mongory_matcher_table_build_sub_matcher(char *key, mongory_value *value, void *acc) {
  mongory_matcher_table_build_sub_matcher_context *ctx = (mongory_matcher_table_build_sub_matcher_context *)acc;
  mongory_memory_pool *pool = ctx->pool;
  mongory_matcher **matchers = ctx->matchers;
  mongory_matcher *matcher = NULL;
  mongory_matcher_build_func func = NULL;
  if (*key == '$') {
    func = mongory_matcher_build_func_get(key);
  }
  if (func != NULL) {
    matcher = func(pool, value);
  } else {
    matcher = mongory_matcher_field_new(pool, key, value);
  }

  if (matcher == NULL) {
    return false;
  }

  matchers[ctx->index++] = matcher;
  return true;
}

typedef mongory_matcher* (*mongory_matchers_construct_func)(mongory_matcher **matchers, int head, int tail);

inline mongory_matcher* mongory_matcher_binary_construct(
  mongory_matcher **matchers,
  int head,
  int tail,
  mongory_matcher_match_func match_func,
  mongory_matchers_construct_func construct_func
) {
  mongory_matcher *sub_matcher = matchers[head];
  int count = tail - head + 1;
  if (count == 1) {
    return sub_matcher;
  }

  int mid = (head + tail) / 2;
  mongory_composite_matcher *composite = mongory_matcher_composite_new(sub_matcher->pool, NULL);
  mongory_matcher *base = (mongory_matcher *)composite;
  base->context = sub_matcher->pool->alloc(sub_matcher->pool->ctx, sizeof(mongory_matcher_context));
  base->match = match_func;
  base->context->original_match = match_func;
  composite->left = construct_func(matchers, head, mid);
  composite->right = construct_func(matchers, mid + 1, tail);

  return (mongory_matcher *)composite;
}

mongory_matcher* mongory_matcher_construct_by_and(mongory_matcher **matchers, int head, int tail) {
  return mongory_matcher_binary_construct(matchers, head, tail, mongory_matcher_and_match, mongory_matcher_construct_by_and);
}

mongory_matcher* mongory_matcher_construct_by_or(mongory_matcher **matchers, int head, int tail) {
  return mongory_matcher_binary_construct(matchers, head, tail, mongory_matcher_or_match, mongory_matcher_construct_by_or);
}

mongory_matcher* mongory_matcher_table_cond_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_table_cond_validate(condition, NULL)) {
    pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
    pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
    pool->error->message = "Condition must be a table.";
    return NULL; // Invalid condition
  }
  mongory_table *table = condition->data.t;
  if (table->count == 0) {
    return mongory_matcher_always_true_new(pool, condition);
  }
  mongory_matcher **sub_matchers = calloc(table->count, sizeof(mongory_matcher *));
  if (sub_matchers == NULL) {
    return NULL;
  }
  mongory_matcher_table_build_sub_matcher_context ctx = { pool, sub_matchers, 0 };
  table->each(table, &ctx, mongory_matcher_table_build_sub_matcher);

  mongory_matcher *matcher = mongory_matcher_construct_by_and(sub_matchers, 0, table->count - 1);
  matcher->condition = condition;
  free(sub_matchers);
  return matcher;
}

bool mongory_matcher_multi_table_cond_validate(mongory_value *condition) {
  if (condition->type != MONGORY_TYPE_ARRAY) return false;

  return condition->data.a->each(condition->data.a, NULL, mongory_matcher_table_cond_validate);
}

bool mongory_matcher_build_and_sub_matcher(mongory_value *condition, void *acc) { 
  return condition->data.t->each(condition->data.t, acc, mongory_matcher_table_build_sub_matcher);
}

mongory_matcher* mongory_matcher_and_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_multi_table_cond_validate(condition)) {
    pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
    pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
    pool->error->message = "And condition must be an array of tables.";
    return NULL; // Invalid condition
  }
  mongory_array *array = condition->data.a;
  mongory_iterable *iterable = (mongory_iterable *)array->base;
  if (iterable->count == 0) {
    return mongory_matcher_always_true_new(pool, condition);
  }
  mongory_matcher **sub_matchers = calloc(iterable->count, sizeof(mongory_matcher *));
  if (sub_matchers == NULL) {
    return NULL;
  }
  mongory_matcher_table_build_sub_matcher_context ctx = { pool, sub_matchers, 0 };
  array->each(array, &ctx, mongory_matcher_build_and_sub_matcher);
  mongory_matcher *matcher = mongory_matcher_construct_by_and(sub_matchers, 0, iterable->count - 1);
  matcher->condition = condition;
  free(sub_matchers);
  return matcher;
}

bool mongory_matcher_build_or_sub_matcher(mongory_value *condition, void *acc) { 
  mongory_matcher_table_build_sub_matcher_context *ctx = (mongory_matcher_table_build_sub_matcher_context *)acc;
  mongory_memory_pool *pool = ctx->pool;
  mongory_matcher **matchers = ctx->matchers;
  mongory_matcher *matcher = mongory_matcher_table_cond_new(pool, condition);
  if (matcher == NULL) {
    return false;
  }
  matchers[ctx->index++] = matcher;
  return true;
}

mongory_matcher* mongory_matcher_or_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_multi_table_cond_validate(condition)) {
    pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
    pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
    pool->error->message = "Or condition must be an array of tables.";
    return NULL; // Invalid condition
  }
  mongory_array *array = condition->data.a;
  mongory_iterable *iterable = (mongory_iterable *)array->base;
  if (iterable->count == 0) {
    return mongory_matcher_always_false_new(pool, condition);
  }
  mongory_matcher **sub_matchers = calloc(iterable->count, sizeof(mongory_matcher *));
  if (sub_matchers == NULL) {
    return NULL;
  }
  mongory_matcher_table_build_sub_matcher_context ctx = { pool, sub_matchers, 0 };
  array->each(array, &ctx, mongory_matcher_build_or_sub_matcher);
  mongory_matcher *matcher = mongory_matcher_construct_by_or(sub_matchers, 0, iterable->count - 1);
  matcher->condition = condition;
  free(sub_matchers);
  return matcher;
}

typedef struct mongory_matcher_array_compare_context {
  mongory_matcher *matcher;
  bool result;
} mongory_matcher_array_compare_context;

bool mongory_matcher_elem_match_unit_compare(mongory_value *value, void *acc) {
  mongory_matcher_array_compare_context *ctx = (mongory_matcher_array_compare_context *)acc;
  if (mongory_matcher_and_match(ctx->matcher, value)) {
    ctx->result = true;
    return false;
  }
  return true;
}

bool mongory_matcher_elem_match_match(mongory_matcher *matcher, mongory_value *value) {
  if (value->type != MONGORY_TYPE_ARRAY) return false;
  mongory_matcher_array_compare_context ctx = { matcher, false };
  mongory_array *array = value->data.a;
  array->each(array, &ctx, mongory_matcher_elem_match_unit_compare);
  return ctx.result;
}

mongory_matcher* mongory_matcher_elem_match_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_table_cond_validate(condition, NULL)) {
    pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
    pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
    pool->error->message = "ElemMatch condition must be a table.";
    return NULL; // Invalid condition
  }
  mongory_table *table = condition->data.t;
  mongory_matcher **sub_matchers = calloc(table->count, sizeof(mongory_matcher *));
  if (sub_matchers == NULL) {
    return NULL;
  }
  mongory_matcher_table_build_sub_matcher_context ctx = { pool, sub_matchers, 0 };
  table->each(table, &ctx, mongory_matcher_table_build_sub_matcher);
  mongory_matcher *matcher = mongory_matcher_construct_by_and(sub_matchers, 0, table->count - 1);
  matcher->context->original_match = mongory_matcher_elem_match_match;
  matcher->match = mongory_matcher_elem_match_match;
  matcher->condition = condition;
  free(sub_matchers);
  return matcher;
}

bool mongory_matcher_every_match_unit_compare(mongory_value *value, void *acc) {
  mongory_matcher_array_compare_context *ctx = (mongory_matcher_array_compare_context *)acc;
  if (!mongory_matcher_and_match(ctx->matcher, value)) {
    ctx->result = false;
    return false;
  }
  return true;
}

bool mongory_matcher_every_match(mongory_matcher *matcher, mongory_value *value) {
  if (value->type != MONGORY_TYPE_ARRAY) return false;
  mongory_matcher_array_compare_context ctx = { matcher, true };
  mongory_array *array = value->data.a;
  array->each(array, &ctx, mongory_matcher_every_match_unit_compare);
  return ctx.result;
}

mongory_matcher* mongory_matcher_every_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_table_cond_validate(condition, NULL)) {
    pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
    pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
    pool->error->message = "Every condition must be a table.";
    return NULL; // Invalid condition
  }
  mongory_table *table = condition->data.t;
  mongory_matcher **sub_matchers = calloc(table->count, sizeof(mongory_matcher *));
  if (sub_matchers == NULL) {
    return NULL;
  }
  mongory_matcher_table_build_sub_matcher_context ctx = { pool, sub_matchers, 0 };
  table->each(table, &ctx, mongory_matcher_table_build_sub_matcher);
  mongory_matcher *matcher = mongory_matcher_construct_by_and(sub_matchers, 0, table->count - 1);
  matcher->context->original_match = mongory_matcher_every_match;
  matcher->match = mongory_matcher_every_match;
  matcher->condition = condition;
  free(sub_matchers);
  return matcher;
}
