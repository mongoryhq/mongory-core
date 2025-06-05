#include <mongory-core.h>
#include "base_matcher.h"
#include "existance_matcher.h"
#include "../foundations/iterable.h"

bool mongory_matcher_validate_bool_condition(mongory_value *condition) {
  if (condition == NULL) {
    return false; // Condition must not be NULL.
  }
  if (condition->type != MONGORY_TYPE_BOOL) {
    return false; // Condition must be a boolean value.
  }
  return true;
}

bool mongory_matcher_exists_match(mongory_matcher *matcher, mongory_value *value) {
  bool condition = *(bool*)mongory_value_extract(matcher->condition);
  return condition == (value != NULL);
}

mongory_matcher* mongory_matcher_exists_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_validate_bool_condition(condition)) {
    pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
    pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
    pool->error->message = "Condition must be a boolean value.";
    return NULL;
  }
  mongory_matcher *matcher = mongory_matcher_new(pool, condition);
  mongory_matcher_context *context = pool->alloc(pool->ctx, sizeof(mongory_matcher_context));
  context->original_match = mongory_matcher_exists_match;
  matcher->match = mongory_matcher_exists_match;
  matcher->context = context;
  return matcher;
}

bool mongory_matcher_present_match(mongory_matcher *matcher, mongory_value *value) {
  bool condition = *(bool*)mongory_value_extract(matcher->condition);
  if (value == NULL) {
    return !condition; // If value is NULL, it can only match if condition is false.
  }
  // Check the type of the value and determine if it is "present" based on its type.
  switch (value->type) {
    case MONGORY_TYPE_ARRAY: {
      mongory_iterable *iterable = (mongory_iterable *)value->data.a->base;
      return condition == (iterable->count > 0);
    }
    case MONGORY_TYPE_TABLE:
      return condition == (value->data.t->count > 0);
    case MONGORY_TYPE_STRING: {
      char *str = (char *)value->data.s;
      return condition == (str != NULL && str[0] != '\0');
    }
    case MONGORY_TYPE_NULL:
      return condition == false;
    case MONGORY_TYPE_BOOL:
      return condition == value->data.b;
    default:
      return condition; // For other types, we assume the condition is true if the value is not NULL.
  }
}

mongory_matcher* mongory_matcher_present_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_validate_bool_condition(condition)) {
    pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
    pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
    pool->error->message = "Condition must be a boolean value.";
    return NULL;
  }
  mongory_matcher *matcher = mongory_matcher_new(pool, condition);
  mongory_matcher_context *context = pool->alloc(pool->ctx, sizeof(mongory_matcher_context));
  context->original_match = mongory_matcher_present_match;
  matcher->match = mongory_matcher_present_match;
  matcher->context = context;
  return matcher;
}
