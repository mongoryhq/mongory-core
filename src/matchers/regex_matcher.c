/**
 * @file regex_matcher.c
 * @brief Implements the $regex matcher.
 * This is an internal implementation file for the matcher module.
 */
#include "regex_matcher.h"
#include "../foundations/config_private.h" // For mongory_internal_regex_adapter
#include "base_matcher.h"                  // For mongory_matcher_base_new
#include "mongory-core/foundations/error.h" // For MONGORY_ERROR_INVALID_ARGUMENT
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"

/**
 * @brief Match function for the $regex matcher.
 *
 * Checks if the input `value` (which must be a string) matches the regex
 * pattern stored in `matcher->condition`. The actual regex matching is
 * performed by the function pointed to by
 * `mongory_internal_regex_adapter->func`.
 *
 * @param matcher The $regex matcher instance.
 * @param value The `mongory_value` to test; must be of type
 * `MONGORY_TYPE_STRING`.
 * @return True if the string value matches the regex, false otherwise or if
 * input is not a string.
 */
static inline bool mongory_matcher_regex_match(mongory_matcher *matcher,
                                               mongory_value *value) {
  if (!value || value->type != MONGORY_TYPE_STRING) {
    return false; // Regex matching applies only to strings.
  }
  if (!mongory_internal_regex_adapter ||
      !mongory_internal_regex_adapter->func) {
    return false; // Regex adapter or function not configured.
  }

  // Delegate to the configured regex function.
  return mongory_internal_regex_adapter->func(matcher->pool, matcher->condition,
                                              value);
}

/**
 * @brief Validates that the condition for a $regex matcher is valid.
 * The condition must be non-NULL and either a `MONGORY_TYPE_STRING` (pattern)
 * or `MONGORY_TYPE_REGEX` (pre-compiled regex object).
 * @param condition The `mongory_value` condition to validate.
 * @return True if the condition is valid for a regex matcher, false otherwise.
 */
static inline bool
mongory_matcher_regex_condition_validate(mongory_value *condition) {
  return condition != NULL && (condition->type == MONGORY_TYPE_STRING ||
                               condition->type == MONGORY_TYPE_REGEX);
}

mongory_matcher *mongory_matcher_regex_new(mongory_memory_pool *pool,
                                           mongory_value *condition) {
  if (!mongory_matcher_regex_condition_validate(condition)) {
    if (pool && pool->alloc) {
        pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
        if (pool->error) {
            pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
            pool->error->message =
                "$regex condition must be a string or a regex object.";
        }
    }
    return NULL;
  }

  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (matcher) {
    matcher->match = mongory_matcher_regex_match;
    matcher->context.original_match = mongory_matcher_regex_match;
  }
  return matcher;
}
