/**
 * @file array_record_matcher.c
 * @brief Implements a versatile matcher for arrays, handling various condition
 * types. This is an internal implementation file for the matcher module.
 *
 * This matcher can interpret conditions as:
 * - A table: Parsed for operators like $elemMatch or implicit field conditions
 *   on array elements.
 * - A regex: Creates an $elemMatch to match array elements against the regex.
 * - A literal: Creates an $elemMatch to check for equality with elements.
 * - Another array: Checks for whole-array equality.
 * It often constructs a composite OR matcher to combine these possibilities.
 */
#include "array_record_matcher.h"
#include "../foundations/config_private.h" // For mongory_try_parse_int
#include "base_matcher.h"                  // For mongory_matcher_base_new, mongory_try_parse_int
#include "compare_matcher.h"               // For mongory_matcher_equal_new
#include "composite_matcher.h"             // For mongory_matcher_composite_new, $elemMatch, table_cond_new, or_match
#include "literal_matcher.h"               // Potentially used by table_cond_new
#include "mongory-core/foundations/table.h"
#include "mongory-core/foundations/value.h"
#include <mongory-core.h> // General include
#include <stdio.h>        // For NULL
#include <string.h>       // For strcmp

/**
 * @struct mongory_matcher_array_record_parse_table_context
 * @brief Context used when parsing a table condition for array record matching.
 *
 * Helps separate parts of the condition table:
 * - `parsed_table`: For explicit operators (e.g., $size, $all if implemented)
 *   or indexed conditions.
 * - `elem_match_table`: For conditions that should apply to individual elements
 *   via an implicit or explicit $elemMatch.
 */
typedef struct mongory_matcher_array_record_parse_table_context {
  mongory_table *parsed_table;     /**< Stores operator or indexed conditions. */
  mongory_table *elem_match_table; /**< Stores conditions for element matching. */
} mongory_matcher_array_record_parse_table_context;

/**
 * @brief Callback to populate the `elem_match_table` during parsing.
 * Used when an explicit $elemMatch object is found within the condition.
 * @param key Key from the $elemMatch object.
 * @param value Value from the $elemMatch object.
 * @param acc Pointer to `mongory_matcher_array_record_parse_table_context`.
 * @return Always true to continue iteration.
 */
static inline bool mongory_matcher_array_record_set_table_elem(char *key, mongory_value *value, void *acc) {
  mongory_matcher_array_record_parse_table_context *context = (mongory_matcher_array_record_parse_table_context *)acc;
  mongory_table *elem_match_table_to_populate = context->elem_match_table;
  elem_match_table_to_populate->set(elem_match_table_to_populate, key, value);
  return true;
}

/**
 * @brief Callback to parse a condition table for array matching.
 *
 * Iterates through the main condition table:
 * - If `key` is "$elemMatch" and `value` is a table, its contents are added to
 *   `context->elem_match_table`.
 * - If `key` starts with '$' (another operator) or is numeric (array index),
 *   it's added to `context->parsed_table`.
 * - Otherwise (plain field name), it's considered part of an implicit element
 *   match and added to `context->elem_match_table`.
 *
 * @param key Current key in the condition table.
 * @param value Current value for the key.
 * @param acc Pointer to `mongory_matcher_array_record_parse_table_context`.
 * @return Always true to continue.
 */
static inline bool mongory_matcher_array_record_parse_table_foreach(char *key, mongory_value *value, void *acc) {
  mongory_matcher_array_record_parse_table_context *context = (mongory_matcher_array_record_parse_table_context *)acc;
  mongory_table *parsed_table_for_operators = context->parsed_table;
  mongory_table *table_for_elem_match_conditions = context->elem_match_table;

  if (strcmp(key, "$elemMatch") == 0 && value->type == MONGORY_TYPE_TABLE && value->data.t != NULL) {
    // Explicit $elemMatch: iterate its sub-table and add to elem_match_table
    mongory_table *elem_match_condition_object = value->data.t;
    elem_match_condition_object->each(elem_match_condition_object, context,
                                      mongory_matcher_array_record_set_table_elem);
  } else if (*key == '$' || mongory_try_parse_int(key, NULL)) {
    // Operator (like $size) or numeric index: goes to parsed_table
    parsed_table_for_operators->set(parsed_table_for_operators, key, value);
  } else {
    // Regular field name: implies a condition on elements, goes to
    // elem_match_table
    table_for_elem_match_conditions->set(table_for_elem_match_conditions, key, value);
  }
  return true;
}

/**
 * @brief Parses a table `condition` intended for array matching.
 *
 * Separates the condition into parts for direct array operations (like $size)
 * and parts for matching individual elements (implicit or explicit $elemMatch).
 * If any element-matching conditions are found, they are grouped under an
 * "$elemMatch" key in the returned `parsed_table`.
 *
 * @param condition The `mongory_value` (must be a table) to parse.
 * @return A new `mongory_value` (table) containing the parsed and restructured
 * condition. Returns NULL if input is not a table or on allocation failure.
 */
static inline mongory_value *mongory_matcher_array_record_parse_table(mongory_value *condition) {
  if (!condition || condition->type != MONGORY_TYPE_TABLE || !condition->data.t || !condition->pool) {
    return NULL; // Invalid input
  }
  mongory_memory_pool *pool = condition->pool;
  mongory_table *parsed_table = mongory_table_new(pool);
  mongory_table *elem_match_sub_table = mongory_table_new(pool);

  if (!parsed_table || !elem_match_sub_table) {
    // Cleanup if one allocation succeeded but other failed? Pool should handle.
    return NULL;
  }

  mongory_matcher_array_record_parse_table_context parse_ctx = {parsed_table, elem_match_sub_table};
  mongory_table *original_condition_table = condition->data.t;

  original_condition_table->each(original_condition_table, &parse_ctx,
                                 mongory_matcher_array_record_parse_table_foreach);

  if (elem_match_sub_table->count > 0) {
    // If there were conditions for element matching, add them as an $elemMatch
    // clause to the main parsed_table.
    mongory_value *elem_match_table_value = mongory_value_wrap_t(pool, elem_match_sub_table);
    if (!elem_match_table_value) {
      // Failed to wrap table, cleanup needed or let pool handle.
      return NULL;
    }
    parsed_table->set(parsed_table, "$elemMatch", elem_match_table_value);
  }
  // If elem_match_sub_table is empty, it's not added. The original parsed_table
  // (which might contain $size etc.) is returned.
  return mongory_value_wrap_t(pool, parsed_table);
}

static inline mongory_value *mongory_matcher_array_record_parse_table_wrap(mongory_memory_pool *pool, char *key, mongory_value *value) {
  if (!pool || !key || !value)
    return NULL;
  mongory_table *table = mongory_table_new(pool);
  if (!table)
    return NULL;
  table->set(table, key, value);
  return mongory_value_wrap_t(pool, table);
}

/**
 * @brief Helper to create an $elemMatch matcher with an inner $regex condition.
 * E.g., for matching an array where elements match a regex.
 * @param pool Memory pool.
 * @param regex_condition The `mongory_value` (string or regex type) for the
 * regex.
 * @return A new $elemMatch matcher with a nested $regex, or NULL on failure.
 */
static inline mongory_matcher *mongory_matcher_array_record_elem_match_regex_new(mongory_memory_pool *pool,
                                                                                 mongory_value *regex_condition) {
  mongory_value *wrapped_table = mongory_matcher_array_record_parse_table_wrap(pool, "$regex", regex_condition);
  if (!wrapped_table)
    return NULL;
  return mongory_matcher_elem_match_new(pool, wrapped_table);
}

/**
 * @brief Helper to create an $elemMatch matcher with an inner $eq condition.
 * E.g., for matching an array containing a specific literal value.
 * @param pool Memory pool.
 * @param literal_condition The `mongory_value` literal to find in array
 * elements.
 * @return A new $elemMatch matcher with a nested $eq, or NULL on failure.
 */
static inline mongory_matcher *mongory_matcher_array_record_elem_match_equal_new(mongory_memory_pool *pool,
                                                                                 mongory_value *literal_condition) {
  mongory_value *wrapped_table = mongory_matcher_array_record_parse_table_wrap(pool, "$eq", literal_condition);
  if (!wrapped_table)
    return NULL;
  return mongory_matcher_elem_match_new(pool, wrapped_table);
}

/**
 * @brief Delegates creation of the "left" part of an array_record_matcher.
 *
 * This part typically handles conditions related to array elements (e.g.,
 * $elemMatch logic).
 * - If `condition` is a table, it's parsed by
 * `mongory_matcher_array_record_parse_table` and then a
 * `mongory_matcher_table_cond_new` is created.
 * - If `condition` is regex, creates an $elemMatch with inner $regex.
 * - Otherwise (literal), creates an $elemMatch with inner $eq.
 *
 * @param pool Memory pool.
 * @param condition The original condition for the array_record_matcher.
 * @return The "left" child matcher, or NULL on failure.
 */
static inline mongory_matcher *mongory_matcher_array_record_left_delegate(mongory_memory_pool *pool,
                                                                          mongory_value *condition) {
  if (!condition)
    return NULL;
  switch (condition->type) {
  case MONGORY_TYPE_TABLE: {
    mongory_value *parsed_table_condition = mongory_matcher_array_record_parse_table(condition);
    if (!parsed_table_condition)
      return NULL;
    return mongory_matcher_table_cond_new(pool, parsed_table_condition);
  }
  case MONGORY_TYPE_REGEX:
    return mongory_matcher_array_record_elem_match_regex_new(pool, condition);
  default: // Literals (string, int, bool, etc.)
    return mongory_matcher_array_record_elem_match_equal_new(pool, condition);
  }
}

/**
 * @brief Delegates creation of the "right" part of an array_record_matcher.
 *
 * This part handles conditions where the array_record_matcher's `condition`
 * is itself an array, implying a whole-array equality check.
 *
 * @param pool Memory pool.
 * @param condition The original condition for the array_record_matcher.
 * @return An equality matcher if `condition` is an array, otherwise NULL.
 */
static inline mongory_matcher *mongory_matcher_array_record_right_delegate(mongory_memory_pool *pool,
                                                                           mongory_value *condition) {
  if (!condition)
    return NULL;
  switch (condition->type) {
  case MONGORY_TYPE_ARRAY:
    // If original condition is an array, right delegate is for direct equality.
    return mongory_matcher_equal_new(pool, condition);
  default:
    return NULL; // No whole-array equality check needed for other types.
  }
}

/**
 * @brief Match function for the array_record_matcher.
 *
 * It expects the `matcher` to be a composite OR matcher.
 * It checks if the input `value` is an array, then applies the OR logic.
 * This OR logic typically combines:
 * 1. Matching based on element conditions (via `composite->left` from
 * `left_delegate`).
 * 2. Matching based on whole-array equality (via `composite->right` from
 * `right_delegate`, if applicable).
 *
 * @param matcher The array_record_matcher instance (cast as base `mongory_matcher`).
 * @param value The `mongory_value` to check, expected to be an array.
 * @return True if the array matches according to the ORed conditions, false
 * otherwise.
 */
static inline bool mongory_matcher_array_record_match(mongory_matcher *matcher, mongory_value *value) {
  if (value == NULL || value->type != MONGORY_TYPE_ARRAY) {
    return false; // Only applies to arrays.
  }
  // This matcher is constructed as a composite OR internally.
  return mongory_matcher_or_match(matcher, value);
}

/**
 * @brief Main constructor for `mongory_matcher_array_record_new`.
 *
 * Constructs a two-part matcher (often combined with OR):
 * - `left`: Handles element-wise conditions (like $elemMatch from various
 *   condition types).
 * - `right`: Handles whole-array equality if the original `condition` was an
 *   array.
 *
 * If `right` is NULL (i.e., original `condition` was not an array), only the
 * `left` matcher is returned. Otherwise, a composite OR matcher is created
 * with `left` and `right` as children.
 *
 * @param pool Memory pool.
 * @param condition The condition to apply to arrays.
 * @return The constructed array_record_matcher, or NULL on failure.
 */
mongory_matcher *mongory_matcher_array_record_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!pool || !condition)
    return NULL;

  mongory_matcher *left_child = mongory_matcher_array_record_left_delegate(pool, condition);
  mongory_matcher *right_child = mongory_matcher_array_record_right_delegate(pool, condition);

  if (!(left_child && right_child)) {
    return left_child ? left_child : right_child;
  }

  // Both left (element-wise) and right (whole-array equality) are possible.
  // Combine them with an OR.
  mongory_composite_matcher *final_or_composite = mongory_matcher_composite_new(pool, condition);
  if (!final_or_composite) {
    // Cleanup left_child, right_child? Pool should handle.
    return NULL;
  }

  final_or_composite->left = left_child;
  final_or_composite->right = right_child;
  final_or_composite->base.match = mongory_matcher_array_record_match; // Uses or_match
  final_or_composite->base.original_match = mongory_matcher_array_record_match;

  final_or_composite->base.name = mongory_string_cpy(pool, "ArrayRecord");
  return (mongory_matcher *)final_or_composite;
}
