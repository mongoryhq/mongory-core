/**
 * @file external_matcher.c
 * @brief Implements the $regex matcher.
 * This is an internal implementation file for the matcher module.
 */
#include "external_matcher.h"
#include "../foundations/config_private.h"   // For mongory_internal_regex_adapter
#include "base_matcher.h"                    // For mongory_matcher_base_new
#include "mongory-core/foundations/config.h" // For mongory_string_cpy
#include "mongory-core/foundations/error.h"  // For MONGORY_ERROR_INVALID_ARGUMENT
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"

/**
 * @brief Match function for the $regex matcher.
 *
 * Checks if the input `value` (which must be a string) matches the regex
 * pattern stored in `matcher->condition`. The actual regex matching is
 * performed by the function pointed to by
 * `mongory_internal_regex_adapter->match_func`.
 *
 * @param matcher The $regex matcher instance.
 * @param value The `mongory_value` to test; must be of type
 * `MONGORY_TYPE_STRING`.
 * @return True if the string value matches the regex, false otherwise or if
 * input is not a string.
 */
static inline bool mongory_matcher_regex_match(mongory_matcher *matcher, mongory_value *value) {
  if (!value || value->type != MONGORY_TYPE_STRING) {
    return false; // Regex matching applies only to strings.
  }
  if (!mongory_internal_regex_adapter || !mongory_internal_regex_adapter->match_func) {
    return false; // Regex adapter or function not configured.
  }

  // Delegate to the configured regex function.
  return mongory_internal_regex_adapter->match_func(matcher->pool, matcher->condition, value);
}

/**
 * @brief Validates that the condition for a $regex matcher is valid.
 * The condition must be non-NULL and either a `MONGORY_TYPE_STRING` (pattern)
 * or `MONGORY_TYPE_REGEX` (pre-compiled regex object).
 * @param condition The `mongory_value` condition to validate.
 * @return True if the condition is valid for a regex matcher, false otherwise.
 */
static inline bool mongory_matcher_regex_condition_validate(mongory_value *condition) {
  return condition != NULL && (condition->type == MONGORY_TYPE_STRING || condition->type == MONGORY_TYPE_REGEX);
}

mongory_matcher *mongory_matcher_regex_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_regex_condition_validate(condition)) {
    if (pool && pool->alloc) {
      pool->error = MG_ALLOC_PTR(pool, mongory_error);
      if (pool->error) {
        pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
        pool->error->message = "$regex condition must be a string or a regex object.";
      }
    }
    return NULL;
  }

  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (matcher) {
    matcher->match = mongory_matcher_regex_match;
    matcher->context.original_match = mongory_matcher_regex_match;
    matcher->name = mongory_string_cpy(pool, "Regex");
  }
  return matcher;
}

/**
 * @brief The match function for a custom matcher.
 * @param matcher The matcher to match against.
 * @param value The value to match.
 * @return True if the value matches the matcher, false otherwise.
 */
bool mongory_matcher_custom_match(mongory_matcher *matcher, mongory_value *value) {
  if (mongory_custom_matcher_adapter == NULL || mongory_custom_matcher_adapter->match == NULL) {
    return false; // Custom matcher adapter not initialized.
  }
  return mongory_custom_matcher_adapter->match(matcher->context.ref, value); // TODO: Check if this is correct.
}

/**
 * @brief Creates a new custom matcher instance.
 *
 * @param pool The memory pool to use for the matcher's allocations.
 * @param key The key for the custom matcher.
 * @param condition The `mongory_value` representing the condition for this
 * matcher.
 * @return mongory_matcher* A pointer to the newly created custom matcher, or
 * NULL on failure.
 */
mongory_matcher *mongory_matcher_custom_new(mongory_memory_pool *pool, char *key, mongory_value *condition) {
  if (mongory_custom_matcher_adapter == NULL || mongory_custom_matcher_adapter->build == NULL) {
    return NULL; // Custom matcher adapter not initialized.
  }
  mongory_matcher_custom_context *context = mongory_custom_matcher_adapter->build(key, condition);
  if (context == NULL) {
    return NULL; // Custom matcher build failed.
  }
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (matcher == NULL) {
    return NULL; // Base matcher allocation failed.
  }
  matcher->context.ref = context->external_ref;
  matcher->name = context->name;
  matcher->match = mongory_matcher_custom_match;
  matcher->context.original_match = mongory_matcher_custom_match;
  return matcher;
}
