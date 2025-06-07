#include <mongory-core.h>
#include "base_matcher.h"
#include "inclusion_matcher.h"

typedef struct mongory_matcher_inclusion_context {
  bool result;
  mongory_value *value;
} mongory_matcher_inclusion_context;

bool mongory_matcher_validate_array_condition(mongory_value *condition) {
  if (condition == NULL) {
    return false; // Condition must not be NULL.
  }
  if (condition->type != MONGORY_TYPE_ARRAY) {
    return false; // Condition must be an array.
  }
  return true;
}

bool mongory_matcher_inclusion_value_compare(mongory_value *a, void *ctx) {
  mongory_matcher_inclusion_context *context = (mongory_matcher_inclusion_context *)ctx;
  mongory_value *b = context->value;
  
  context->result = a->comp(a, b) == 0;
  return !context->result; // Continue iteration if the values are not equal.
}

bool mongory_matcher_inclusion_array_compare(mongory_value *a, void *ctx) {
  mongory_matcher_inclusion_context *context = (mongory_matcher_inclusion_context *)ctx;
  mongory_array *b = context->value->data.a;
  mongory_matcher_inclusion_context ctx_b = { false, a };

  bool is_continue = b->each(b, &ctx_b, mongory_matcher_inclusion_value_compare);
  context->result = ctx_b.result;
  return is_continue;
}

bool mongory_matcher_in_match(mongory_matcher *matcher, mongory_value *value) {
  mongory_matcher_inclusion_context ctx = { false, value };
  mongory_array *condition_array = (mongory_array *)matcher->condition->data.a;
  if (value->type == MONGORY_TYPE_ARRAY) {
    condition_array->each(condition_array, &ctx, mongory_matcher_inclusion_array_compare);
    return ctx.result;
  } else {
    condition_array->each(condition_array, &ctx, mongory_matcher_inclusion_value_compare); 
    return ctx.result;
  }
}

mongory_matcher* mongory_matcher_in_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_validate_array_condition(condition)) {
    pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
    pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
    pool->error->message = "Condition must be an array.";
    return NULL;
  }
  mongory_matcher *matcher = mongory_matcher_new(pool, condition);
  mongory_matcher_context *context = pool->alloc(pool->ctx, sizeof(mongory_matcher_context));
  context->original_match = mongory_matcher_in_match;
  matcher->match = mongory_matcher_in_match;
  matcher->context = context;
  return matcher;
}

bool mongory_matcher_not_in_match(mongory_matcher *matcher, mongory_value *value) {
  return !mongory_matcher_in_match(matcher, value);
}

mongory_matcher* mongory_matcher_not_in_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_validate_array_condition(condition)) {
    pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
    pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
    pool->error->message = "Condition must be an array.";
    return NULL;
  }
  mongory_matcher *matcher = mongory_matcher_new(pool, condition);
  mongory_matcher_context *context = pool->alloc(pool->ctx, sizeof(mongory_matcher_context));
  context->original_match = mongory_matcher_not_in_match;
  matcher->match = mongory_matcher_not_in_match;
  matcher->context = context;
  return matcher;
}
