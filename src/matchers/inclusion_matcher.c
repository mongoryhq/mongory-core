/**
 * @file inclusion_matcher.c
 * @brief Implements $in and $nin matchers for checking value inclusion in an
 * array. This is an internal implementation file for the matcher module.
 */
#include "inclusion_matcher.h"
#include "base_matcher.h"                   // For mongory_matcher_base_new
#include "mongory-core/foundations/array.h" // For mongory_array operations
#include "mongory-core/foundations/error.h" // For mongory_error
#include "mongory-core/foundations/value.h" // For mongory_value
#include <mongory-core.h>                   // General include

/**
 * @struct mongory_matcher_inclusion_context
 * @brief Context structure used during the inclusion matching process.
 *
 * Stores the result of the comparison and the value being searched for.
 */
typedef struct mongory_matcher_inclusion_context {
  bool result;          /**< Stores the outcome (true if found, false otherwise). */
  mongory_value *value; /**< The value to search for within the condition array or
                           target array. */
} mongory_matcher_inclusion_context;

/**
 * @brief Validates that the condition for an inclusion matcher is a valid
 * array.
 * @param condition The `mongory_value` condition to validate.
 * @return True if `condition` is not NULL, is of type `MONGORY_TYPE_ARRAY`,
 * and its internal array data `condition->data.a` is not NULL. False otherwise.
 */
static inline bool mongory_matcher_validate_array_condition(mongory_value *condition) {
  if (condition == NULL) {
    return false; // Condition must exist.
  }
  if (condition->type != MONGORY_TYPE_ARRAY) {
    return false; // Condition must be an array.
  }
  if (condition->data.a == NULL) {
    return false; // The array data within the condition must be valid.
  }
  return true;
}

/**
 * @brief Callback function for `mongory_array_each` to compare a single value
 * (`b` from context) against an element (`a`) from the condition array.
 *
 * This is used when the value being matched against $in is a scalar.
 *
 * @param item_in_condition_array The current item (`a`) from the condition array.
 * @param acc Pointer to `mongory_matcher_inclusion_context`. `context->value`
 *            is the scalar value (`b`) being searched for.
 * @return `false` (stop iteration) if `item_in_condition_array` equals
 * `context->value`, `true` (continue iteration) otherwise.
 */
static inline bool mongory_matcher_inclusion_value_compare(mongory_value *item_in_condition_array, void *acc) {
  mongory_matcher_inclusion_context *context = (mongory_matcher_inclusion_context *)acc;
  mongory_value *scalar_value_to_find = context->value;

  if (!item_in_condition_array || !scalar_value_to_find || !item_in_condition_array->comp) {
    // Cannot compare if item or value is NULL, or if item has no compare function.
    // Treat as not equal for safety, continue search.
    return true;
  }

  // Check if the item from condition array equals the scalar value we're looking for.
  context->result = (item_in_condition_array->comp(item_in_condition_array, scalar_value_to_find) == 0);

  return !context->result; // Stop if found (result is true), continue if not found.
}

/**
 * @brief Callback function for `mongory_array_each` to compare an array (`a`
 * from context->value) against an element (`b_scalar_in_condition_array`) from
 * the condition array.
 *
 * This is used when the value being matched against $in is an array itself.
 * It checks if `b_scalar_in_condition_array` exists in the input array `a`.
 *
 * @param b_scalar_in_condition_array The current scalar item (`b`) from the condition array.
 * @param acc Pointer to `mongory_matcher_inclusion_context`. `context->value`
 *            is the input array (`a`) being checked.
 * @return `false` (stop iteration) if `b_scalar_in_condition_array` is found
 * in `context->value` (the input array), `true` (continue) otherwise.
 */
static inline bool mongory_matcher_inclusion_array_compare(mongory_value *b_scalar_in_condition_array, void *acc) {
  mongory_matcher_inclusion_context *outer_context = (mongory_matcher_inclusion_context *)acc;
  // outer_context->value is the input array we are checking.
  mongory_array *input_array_a = outer_context->value->data.a;

  // We want to find if b_scalar_in_condition_array is present in input_array_a.
  // Create a new context for searching b_scalar_in_condition_array within input_array_a.
  mongory_matcher_inclusion_context inner_search_ctx = {false, b_scalar_in_condition_array};

  // Iterate through input_array_a to find b_scalar_in_condition_array.
  // mongory_matcher_inclusion_value_compare will set inner_search_ctx.result to true if found.
  // It returns !result, so if found (result=true), it returns false (stop).
  bool iteration_stopped =
      !input_array_a->each(input_array_a, &inner_search_ctx, mongory_matcher_inclusion_value_compare);

  if (iteration_stopped && inner_search_ctx.result) {
    // b_scalar_in_condition_array was found in input_array_a.
    outer_context->result = true; // Mark overall $in as successful.
    return false;                 // Stop iterating the outer (condition) array.
  }

  return true; // b_scalar_in_condition_array was not in input_array_a, continue.
}

/**
 * @brief Match function for the $in matcher.
 *
 * If `value_to_check` is a scalar, it checks if `value_to_check` is present in
 * `matcher->condition` (which must be an array).
 * If `value_to_check` is an array, it checks if any element of `value_to_check`
 * is present in `matcher->condition` (array intersection).
 *
 * @param matcher The $in matcher instance.
 * @param value_to_check The value to check for inclusion.
 * @return True if `value_to_check` (or one of its elements) is found in the
 * condition array, false otherwise.
 */
static inline bool mongory_matcher_in_match(mongory_matcher *matcher, mongory_value *value_to_check) {
  if (!value_to_check || !matcher->condition || !matcher->condition->data.a) {
    // Invalid inputs or condition is not a proper array.
    return false;
  }

  mongory_matcher_inclusion_context ctx = {false, value_to_check};
  mongory_array *condition_array = matcher->condition->data.a;

  if (value_to_check->type == MONGORY_TYPE_ARRAY) {
    if (value_to_check->data.a == NULL)
      return false; // Invalid input array
    // Case: $in: [c1, c2], input: [a1, a2]
    // We need to check if any c_i is in [a1, a2] OR if any a_i is in [c1, c2].
    // The current mongory_matcher_inclusion_array_compare checks if an element from
    // condition_array is present in value_to_check (the input array).
    // So, we iterate the condition_array, and for each element, check if it's in value_to_check.
    condition_array->each(condition_array, &ctx, mongory_matcher_inclusion_array_compare);
  } else {
    // Case: $in: [c1, c2], input: scalar_value
    // Check if scalar_value is any of c_i.
    condition_array->each(condition_array, &ctx, mongory_matcher_inclusion_value_compare);
  }
  return ctx.result; // True if any comparison led to result=true.
}

mongory_matcher *mongory_matcher_in_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_validate_array_condition(condition)) {
    pool->error = MG_ALLOC_PTR(pool, mongory_error);
    if (pool->error) {
      pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
      pool->error->message = "$in condition must be a valid array.";
    }
    return NULL;
  }
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (!matcher) {
    return NULL;
  }
  matcher->match = mongory_matcher_in_match;
  matcher->original_match = mongory_matcher_in_match;
  matcher->name = mongory_string_cpy(pool, "In");
  return matcher;
}

/**
 * @brief Match function for the $nin (not in) matcher.
 * Simply negates the result of the $in logic.
 * @param matcher The $nin matcher instance.
 * @param value The value to check.
 * @return True if the value is NOT found according to $in logic, false
 * otherwise.
 */
static inline bool mongory_matcher_not_in_match(mongory_matcher *matcher, mongory_value *value) {
  // $nin is true if $in is false.
  return !mongory_matcher_in_match(matcher, value);
}

mongory_matcher *mongory_matcher_not_in_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!mongory_matcher_validate_array_condition(condition)) {
    pool->error = MG_ALLOC_PTR(pool, mongory_error);
    if (pool->error) {
      pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
      pool->error->message = "$nin condition must be a valid array.";
    }
    return NULL;
  }
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (!matcher) {
    return NULL;
  }
  matcher->match = mongory_matcher_not_in_match;
  matcher->original_match = mongory_matcher_not_in_match;
  matcher->name = mongory_string_cpy(pool, "Nin");
  return matcher;
}
