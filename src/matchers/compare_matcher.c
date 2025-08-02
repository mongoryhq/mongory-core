/**
 * @file compare_matcher.c
 * @brief Implements various comparison matchers (e.g., $eq, $gt, $lt).
 * This is an internal implementation file for the matcher module.
 */
#include "compare_matcher.h"
#include "base_matcher.h" // For mongory_matcher_base_new
#include <mongory-core.h> // For mongory_value, mongory_matcher types

/**
 * @brief Generic constructor for comparison matchers.
 *
 * Initializes a base matcher and sets its `match` function and
 * `original_match` context field to the provided `match_func`.
 *
 * @param pool The memory pool for allocation.
 * @param condition The `mongory_value` to be stored as the comparison target.
 * @param match_func The specific comparison logic function (e.g., for equality,
 * greater than).
 * @return mongory_matcher* A pointer to the newly created comparison matcher,
 * or NULL on failure.
 */
static inline mongory_matcher *mongory_matcher_compare_new(mongory_memory_pool *pool, mongory_value *condition,
                                                           mongory_matcher_match_func match_func) {
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (matcher == NULL) {
    return NULL; // Base matcher allocation failed.
  }
  matcher->match = match_func;
  matcher->context.original_match = match_func; // Store original for context
  return matcher;
}

/**
 * @brief Match function for equality ($eq).
 * Compares `value` with `matcher->condition` using `value->comp`.
 * @param matcher The equality matcher instance.
 * @param value The value to check.
 * @return True if `value` is equal to `matcher->condition`, false otherwise or
 * if comparison fails.
 */
static inline bool mongory_matcher_equal_match(mongory_matcher *matcher, mongory_value *value) {
  if (!value || !value->comp || !matcher->condition)
    return false; // Invalid inputs
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return false; // Types are not comparable or other comparison error.
  }
  return result == 0; // 0 indicates equality.
}

mongory_matcher *mongory_matcher_equal_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_compare_new(pool, condition, mongory_matcher_equal_match);
  if (matcher) {
    matcher->name = mongory_string_cpy(pool, "Eq");
  }
  return matcher;
}

/**
 * @brief Match function for inequality ($ne).
 * Compares `value` with `matcher->condition`.
 * @param matcher The inequality matcher instance.
 * @param value The value to check.
 * @return True if `value` is not equal to `matcher->condition`. If comparison
 * itself fails (types incompatible), it's also considered "not equal".
 */
static inline bool mongory_matcher_not_equal_match(mongory_matcher *matcher, mongory_value *value) {
  if (!value || !value->comp || !matcher->condition)
    return true; // Invalid inputs, treat as "not equal"
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return true; // Incomparable types are considered "not equal".
  }
  return result != 0; // Non-zero indicates inequality.
}

mongory_matcher *mongory_matcher_not_equal_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_compare_new(pool, condition, mongory_matcher_not_equal_match);
  if (matcher) {
    matcher->name = mongory_string_cpy(pool, "Ne");
  }
  return matcher;
}

/**
 * @brief Match function for "greater than" ($gt).
 * @param matcher The $gt matcher instance.
 * @param value The value to check.
 * @return True if `value` is greater than `matcher->condition`, false
 * otherwise or on comparison failure.
 */
static inline bool mongory_matcher_greater_than_match(mongory_matcher *matcher, mongory_value *value) {
  if (!value || !value->comp || !matcher->condition)
    return false;
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return false;
  }
  return result == 1; // 1 indicates value > condition.
}

mongory_matcher *mongory_matcher_greater_than_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_compare_new(pool, condition, mongory_matcher_greater_than_match);
  if (matcher) {
    matcher->name = mongory_string_cpy(pool, "Gt");
  }
  return matcher;
}

/**
 * @brief Match function for "less than" ($lt).
 * @param matcher The $lt matcher instance.
 * @param value The value to check.
 * @return True if `value` is less than `matcher->condition`, false otherwise or
 * on comparison failure.
 */
static inline bool mongory_matcher_less_than_match(mongory_matcher *matcher, mongory_value *value) {
  if (!value || !value->comp || !matcher->condition)
    return false;
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return false;
  }
  return result == -1; // -1 indicates value < condition.
}

mongory_matcher *mongory_matcher_less_than_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_compare_new(pool, condition, mongory_matcher_less_than_match);
  if (matcher) {
    matcher->name = mongory_string_cpy(pool, "Lt");
  }
  return matcher;
}

/**
 * @brief Match function for "greater than or equal" ($gte).
 * @param matcher The $gte matcher instance.
 * @param value The value to check.
 * @return True if `value` is >= `matcher->condition`, false otherwise or on
 * comparison failure.
 */
static inline bool mongory_matcher_greater_than_or_equal_match(mongory_matcher *matcher, mongory_value *value) {
  if (!value || !value->comp || !matcher->condition)
    return false;
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return false;
  }
  return result >= 0; // 0 or 1 indicates value >= condition.
}

mongory_matcher *mongory_matcher_greater_than_or_equal_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_compare_new(pool, condition, mongory_matcher_greater_than_or_equal_match);
  if (matcher) {
    matcher->name = mongory_string_cpy(pool, "Gte");
  }
  return matcher;
}

/**
 * @brief Match function for "less than or equal" ($lte).
 * @param matcher The $lte matcher instance.
 * @param value The value to check.
 * @return True if `value` is <= `matcher->condition`, false otherwise or on
 * comparison failure.
 */
static inline bool mongory_matcher_less_than_or_equal_match(mongory_matcher *matcher, mongory_value *value) {
  if (!value || !value->comp || !matcher->condition)
    return false;
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return false;
  }
  return result <= 0; // 0 or -1 indicates value <= condition.
}

mongory_matcher *mongory_matcher_less_than_or_equal_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_compare_new(pool, condition, mongory_matcher_less_than_or_equal_match);
  if (matcher) {
    matcher->name = mongory_string_cpy(pool, "Lte");
  }
  return matcher;
}
