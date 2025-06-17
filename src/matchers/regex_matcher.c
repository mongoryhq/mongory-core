#include "mongory-core/foundations/value.h"
#include "mongory-core/foundations/memory_pool.h"
#include "../foundations/config_private.h"
#include "base_matcher.h"
#include "regex_matcher.h"

static inline bool mongory_matcher_regex_match(mongory_matcher *matcher, mongory_value *value) {
  if (value->type != MONGORY_TYPE_STRING) {
    return false;
  }

  return mongory_internal_regex_adapter->func(matcher->pool, matcher->condition, value);
}

static inline bool mongory_matcher_regex_validation(mongory_value *condition) {
  return condition != NULL && 
         (condition->type == MONGORY_TYPE_STRING || condition->type == MONGORY_TYPE_REGEX);
}

mongory_matcher *mongory_matcher_regex_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_regex_validation(condition)) {
    pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
    pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
    pool->error->message = "Condition must be a string or regex.";
    return NULL;
  }
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  matcher->context.original_match = mongory_matcher_regex_match;
  matcher->match = mongory_matcher_regex_match;
  return matcher;
}
