/**
 * @file composite_matcher.c
 * @brief Implements composite matchers like AND, OR, $elemMatch, and the
 * core table condition parser. This is an internal implementation file for the
 * matcher module.
 */
#include "composite_matcher.h"
#include "../foundations/config_private.h" // For mongory_matcher_build_func_get
#include "base_matcher.h" // For mongory_matcher_always_true_new, etc.
#include "literal_matcher.h" // For mongory_matcher_field_new
#include "mongory-core/foundations/error.h" // For MONGORY_ERROR_INVALID_ARGUMENT
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/table.h" // For mongory_table operations
#include "mongory-core/foundations/value.h"
#include <mongory-core.h> // General include

/**
 * @brief Allocates and initializes a `mongory_composite_matcher` structure.
 *
 * Initializes the base matcher part and sets child pointers (`left`, `right`)
 * to NULL. The specific `match` function and child matchers must be set by the
 * derived composite matcher's constructor.
 *
 * @param pool The memory pool for allocation.
 * @param condition The condition value for this composite matcher.
 * @return mongory_composite_matcher* Pointer to the new matcher, or NULL on
 * failure.
 */
mongory_composite_matcher *
mongory_matcher_composite_new(mongory_memory_pool *pool,
                              mongory_value *condition) {
  if (!pool || !pool->alloc) return NULL;

  mongory_composite_matcher *composite = (mongory_composite_matcher *)pool->alloc(
      pool->ctx, sizeof(mongory_composite_matcher));
  if (composite == NULL) {
    return NULL; // Allocation failed.
  }

  // Initialize base matcher fields
  composite->base.pool = pool;
  composite->base.name = NULL; // Specific name to be set by derived type if any
  composite->base.match = NULL; // Specific match fn to be set by derived type
  composite->base.context.original_match = NULL;
  composite->base.context.trace = NULL;
  composite->base.condition = condition;

  // Initialize composite-specific fields
  composite->left = NULL;
  composite->right = NULL;

  return composite;
}

/**
 * @brief Match function for an AND logical operation.
 *
 * Evaluates to true if both `left` and `right` child matchers (if they exist)
 * evaluate to true. If a child does not exist, it's considered true for this
 * operation.
 *
 * @param matcher Pointer to the composite AND matcher.
 * @param value The value to evaluate.
 * @return True if all child conditions are met, false otherwise.
 */
static inline bool mongory_matcher_and_match(mongory_matcher *matcher,
                                             mongory_value *value) {
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  // If left child exists and doesn't match, the AND fails.
  if (composite->left && !composite->left->match(composite->left, value)) {
    return false;
  }
  // If right child exists and doesn't match, the AND fails.
  if (composite->right && !composite->right->match(composite->right, value)) {
    return false;
  }
  return true; // Both matched (or didn't exist, which is fine for AND).
}

/**
 * @brief Match function for an OR logical operation.
 *
 * Evaluates to true if either the `left` or `right` child matcher (if they
 * exist) evaluates to true. If a child does not exist, it's considered false
 * for this operation.
 *
 * @param matcher Pointer to the composite OR matcher.
 * @param value The value to evaluate.
 * @return True if any child condition is met, false otherwise.
 */
bool mongory_matcher_or_match(mongory_matcher *matcher, mongory_value *value) {
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  // If left child exists and matches, the OR succeeds.
  if (composite->left && composite->left->match(composite->left, value)) {
    return true;
  }
  // If right child exists and matches, the OR succeeds.
  if (composite->right && composite->right->match(composite->right, value)) {
    return true;
  }
  return false; // Neither matched (or children didn't exist).
}

/**
 * @brief Validates if a condition value is a non-null table.
 * Used by table_cond_new and multi_table_cond_validate.
 * @param condition The mongory_value to validate.
 * @param acc Unused accumulator for the array `each` callback.
 * @return True if condition is a valid table, false otherwise.
 */
static inline bool mongory_matcher_table_cond_validate(mongory_value *condition,
                                                       void *acc) {
  (void)acc; // Unused parameter.
  return condition != NULL && condition->type == MONGORY_TYPE_TABLE &&
         condition->data.t != NULL;
}

/**
 * @brief Context structure for building sub-matchers from a table.
 */
typedef struct mongory_matcher_table_build_sub_matcher_context {
  mongory_memory_pool *pool; /**< Main pool for allocating created matchers. */
  mongory_array *matchers;   /**< Array to store the created sub-matchers. */
} mongory_matcher_table_build_sub_matcher_context;

/**
 * @brief Callback for iterating over a condition table's key-value pairs.
 *
 * For each pair, it creates an appropriate sub-matcher:
 * - If key starts with '$', it looks up a registered matcher builder.
 * - Otherwise, it creates a field matcher (`mongory_matcher_field_new`).
 * The created sub-matcher is added to the `matchers` array in the context.
 *
 * @param key The key from the condition table.
 * @param value The value associated with the key.
 * @param acc Pointer to `mongory_matcher_table_build_sub_matcher_context`.
 * @return True to continue iteration, false if a sub-matcher creation fails.
 */
static inline bool mongory_matcher_table_build_sub_matcher(char *key,
                                                           mongory_value *value,
                                                           void *acc) {
  mongory_matcher_table_build_sub_matcher_context *ctx =
      (mongory_matcher_table_build_sub_matcher_context *)acc;
  mongory_memory_pool *pool = ctx->pool;
  mongory_array *matchers_array = ctx->matchers;
  mongory_matcher *sub_matcher = NULL;
  mongory_matcher_build_func build_func = NULL;

  if (key[0] == '$') { // Operator key (e.g., "$eq", "$in")
    build_func = mongory_matcher_build_func_get(key);
  }

  if (build_func != NULL) {
    sub_matcher = build_func(pool, value);
  } else {
    // Not an operator or unknown operator, treat as a field name.
    sub_matcher = mongory_matcher_field_new(pool, key, value);
  }

  if (sub_matcher == NULL) {
    // Failed to create sub-matcher (e.g., allocation error, invalid condition
    // for sub-matcher)
    // TODO: Propagate error from sub-matcher creation if possible.
    return false;
  }

  matchers_array->push(matchers_array, (mongory_value *)sub_matcher);
  return true;
}

/**
 * @brief Recursively constructs a binary tree of composite matchers from an
 * array of sub-matchers.
 *
 * This is used to combine multiple conditions (e.g., from an AND or OR list,
 * or fields in a table condition) into a single effective matcher.
 *
 * @param matchers_array Array of pre-built sub-matchers.
 * @param head Start index in `matchers_array`.
 * @param tail End index in `matchers_array`.
 * @param match_func The match function for the composite nodes (e.g.,
 * `mongory_matcher_and_match`).
 * @param constructor_func Recursive call to this function itself (or a similar
 * one for different logic).
 * @return The root mongory_matcher of the constructed binary tree.
 */
static inline mongory_matcher *mongory_matcher_binary_construct(
    mongory_array *matchers_array, int head, int tail,
    mongory_matcher_match_func match_func,
    mongory_matcher *(*constructor_func)(mongory_array *matchers_array, int head,
                                         int tail)) {
  // Base case: single matcher in the current range.
  mongory_matcher *first_matcher_in_range =
      (mongory_matcher *)matchers_array->get(matchers_array, head);
  if (first_matcher_in_range == NULL) return NULL; // Should not happen if array is populated correctly

  int count = tail - head + 1;
  if (count == 1) {
    return first_matcher_in_range;
  }

  // Recursive step: split the range and combine.
  int mid = head + (tail - head) / 2; // Avoid overflow for large head/tail
  mongory_composite_matcher *composite = mongory_matcher_composite_new(
      first_matcher_in_range->pool, NULL); // Condition is implicit from children
  if (!composite) return NULL;

  mongory_matcher *base_composite_matcher = (mongory_matcher *)composite;
  base_composite_matcher->match = match_func;
  base_composite_matcher->context.original_match = match_func;

  composite->left = constructor_func(matchers_array, head, mid);
  composite->right = constructor_func(matchers_array, mid + 1, tail);

  if (!composite->left || !composite->right) {
      // If children construction failed, this composite matcher is invalid.
      // The memory for 'composite' itself is from the pool and will be handled
      // if the whole operation fails and the pool is cleaned up.
      // However, this indicates a deeper issue.
      // TODO: Proper error handling/cleanup for partial tree construction.
      return NULL;
  }

  return base_composite_matcher;
}

/** Helper to construct AND tree */
static mongory_matcher *mongory_matcher_construct_by_and(
    mongory_array *matchers_array, int head, int tail) {
  return mongory_matcher_binary_construct(matchers_array, head, tail,
                                          mongory_matcher_and_match,
                                          mongory_matcher_construct_by_and);
}

/** Helper to construct OR tree */
static mongory_matcher *
mongory_matcher_construct_by_or(mongory_array *matchers_array, int head,
                                int tail) {
  return mongory_matcher_binary_construct(matchers_array, head, tail,
                                          mongory_matcher_or_match,
                                          mongory_matcher_construct_by_or);
}

/**
 * @brief Creates a matcher from a table-based condition.
 *
 * Parses the `condition` table, creating sub-matchers for each key-value pair.
 * These sub-matchers are then combined using an AND logic.
 *
 * @param pool Memory pool for allocations.
 * @param condition A `mongory_value` of type `MONGORY_TYPE_TABLE`.
 * @return A `mongory_matcher` representing the table condition, or NULL on
 * failure.
 */
mongory_matcher *mongory_matcher_table_cond_new(mongory_memory_pool *pool,
                                                mongory_value *condition) {
  if (!mongory_matcher_table_cond_validate(condition, NULL)) {
    if (pool && pool->alloc) { // Check if pool is usable for error allocation
        pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
        if (pool->error) {
            pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
            pool->error->message = "Condition for table_cond_new must be a valid table.";
        }
    }
    return NULL;
  }

  mongory_table *table = condition->data.t;
  if (table->count == 0) {
    // Empty table condition matches everything.
    return mongory_matcher_always_true_new(pool, condition);
  }

  // Use a temporary pool for the sub_matchers array itself.
  // The actual sub-matchers will be allocated from the main 'pool'.
  mongory_memory_pool *temp_pool = mongory_memory_pool_new();
  if (!temp_pool) return NULL; // Failed to create temp pool

  mongory_array *sub_matchers_array = mongory_array_new(temp_pool);
  if (sub_matchers_array == NULL) {
    temp_pool->free(temp_pool);
    return NULL; // Failed to create array for sub-matchers.
  }

  mongory_matcher_table_build_sub_matcher_context build_ctx = {pool,
                                                              sub_matchers_array};
  // Iterate over the condition table, building sub-matchers.
  if (!table->each(table, &build_ctx,
                   mongory_matcher_table_build_sub_matcher)) {
      // Building one of the sub-matchers failed.
      temp_pool->free(temp_pool); // Clean up temp_pool and sub_matchers_array
      return NULL;
  }

  if (sub_matchers_array->count == 0) { // Should not happen if table->count > 0 and each() succeeded
      temp_pool->free(temp_pool);
      return mongory_matcher_always_true_new(pool, condition); // Or an error
  }

  // Combine sub-matchers using AND logic.
  mongory_matcher *final_matcher = mongory_matcher_construct_by_and(
      sub_matchers_array, 0, sub_matchers_array->count - 1);

  if (final_matcher && final_matcher->condition == NULL) {
    final_matcher->condition = condition; // Assign original condition if not set by construct
  }

  temp_pool->free(temp_pool); // Free the temporary pool and the sub_matchers_array.
  return final_matcher;
}

/**
 * @brief Validates if a condition is an array of valid tables.
 * Used by $and and $or matcher constructors.
 * @param condition The mongory_value to validate.
 * @return True if valid, false otherwise.
 */
static inline bool
mongory_matcher_multi_table_cond_validate(mongory_value *condition) {
  if (!condition || condition->type != MONGORY_TYPE_ARRAY || !condition->data.a) {
    return false; // Must be a non-null array.
  }
  // Check each element of the array.
  return condition->data.a->each(condition->data.a, NULL,
                                 mongory_matcher_table_cond_validate);
}

/**
 * @brief Callback for $and constructor to build sub-matchers from each table
 * in the condition array. This is a bit complex: each element of the $and array
 * is a table, and each key-value in THAT table becomes a sub-matcher. These
 * are then ANDed together.
 * @param condition_table A `mongory_value` (table) from the $and array.
 * @param acc Pointer to `mongory_matcher_table_build_sub_matcher_context`.
 * @return Result of iterating through `condition_table`.
 */
static inline bool
mongory_matcher_build_and_sub_matcher(mongory_value *condition_table, void *acc) {
  // The 'condition_table' is one of the tables in the $and:[{}, {}, ...] array.
  // We need to build all matchers from this table and add them to the list.
  // The list in 'acc' (ctx->matchers) will then be ANDed together.
  if (!condition_table || condition_table->type != MONGORY_TYPE_TABLE || !condition_table->data.t) {
      return false; // Element in $and array is not a table.
  }
  return condition_table->data.t->each(
      condition_table->data.t, acc, mongory_matcher_table_build_sub_matcher);
}

/**
 * @brief Creates an "AND" ($and) matcher from an array of condition tables.
 * @param pool Memory pool for allocations.
 * @param condition A `mongory_value` array of table conditions.
 * @return A new $and matcher, or NULL on failure.
 */
mongory_matcher *mongory_matcher_and_new(mongory_memory_pool *pool,
                                         mongory_value *condition) {
  if (!mongory_matcher_multi_table_cond_validate(condition)) {
    if (pool && pool->alloc) {
        pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
        if (pool->error) {
            pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
            pool->error->message = "$and condition must be an array of tables.";
        }
    }
    return NULL;
  }

  mongory_array *array_of_tables = condition->data.a;
  if (array_of_tables->count == 0) {
    return mongory_matcher_always_true_new(pool, condition); // $and:[] is true
  }

  mongory_memory_pool *temp_pool = mongory_memory_pool_new();
  if(!temp_pool) return NULL;
  mongory_array *all_sub_matchers = mongory_array_new(temp_pool);
  if (all_sub_matchers == NULL) {
    temp_pool->free(temp_pool);
    return NULL;
  }

  // Context for building matchers from EACH table within the $and array.
  mongory_matcher_table_build_sub_matcher_context build_ctx = {pool,
                                                              all_sub_matchers};
  // Iterate through the array of tables provided in the $and condition.
  // mongory_matcher_build_and_sub_matcher will then iterate keys of EACH table.
  if (!array_of_tables->each(array_of_tables, &build_ctx,
                           mongory_matcher_build_and_sub_matcher)) {
      temp_pool->free(temp_pool);
      return NULL; // Failure during sub-matcher construction.
  }

  if(all_sub_matchers->count == 0) { // Should not happen if validation passed and array_of_tables->count > 0
      temp_pool->free(temp_pool);
      return mongory_matcher_always_true_new(pool, condition);
  }

  mongory_matcher *final_matcher = mongory_matcher_construct_by_and(
      all_sub_matchers, 0, all_sub_matchers->count - 1);

  if (final_matcher && final_matcher->condition == NULL) {
    final_matcher->condition = condition; // Original $and condition
  }
  temp_pool->free(temp_pool);
  return final_matcher;
}

/**
 * @brief Callback for $or constructor. Each element in the $or array is a
 * complete table condition, which is turned into a single matcher. These
 * "table condition matchers" are then ORed.
 * @param condition_table A `mongory_value` (table) from the $or array.
 * @param acc Pointer to `mongory_matcher_table_build_sub_matcher_context`.
 * The `matchers` array in context will store the result of
 * `mongory_matcher_table_cond_new`.
 * @return True if successful, false otherwise.
 */
static inline bool
mongory_matcher_build_or_sub_matcher(mongory_value *condition_table, void *acc) {
  mongory_matcher_table_build_sub_matcher_context *ctx =
      (mongory_matcher_table_build_sub_matcher_context *)acc;
  mongory_memory_pool *pool_for_new_matchers = ctx->pool;
  mongory_array *array_to_store_table_matchers = ctx->matchers;

  // Each 'condition_table' is a complete query object for one branch of the OR.
  // So, create a full table_cond_new matcher for it.
  mongory_matcher *table_level_matcher =
      mongory_matcher_table_cond_new(pool_for_new_matchers, condition_table);
  if (table_level_matcher == NULL) {
    return false; // Failed to create a matcher for this OR branch.
  }
  array_to_store_table_matchers->push(array_to_store_table_matchers,
                                    (mongory_value *)table_level_matcher);
  return true;
}

/**
 * @brief Creates an "OR" ($or) matcher from an array of condition tables.
 * @param pool Memory pool for allocations.
 * @param condition A `mongory_value` array of table conditions.
 * @return A new $or matcher, or NULL on failure.
 */
mongory_matcher *mongory_matcher_or_new(mongory_memory_pool *pool,
                                        mongory_value *condition) {
  if (!mongory_matcher_multi_table_cond_validate(condition)) {
    if (pool && pool->alloc) {
        pool->error = pool->alloc(pool->ctx, sizeof(mongory_error));
        if (pool->error) {
            pool->error->type = MONGORY_ERROR_INVALID_ARGUMENT;
            pool->error->message = "$or condition must be an array of tables.";
        }
    }
    return NULL;
  }
  mongory_array *array_of_tables = condition->data.a;
  if (array_of_tables->count == 0) {
    return mongory_matcher_always_false_new(pool, condition); // $or:[] is false
  }

  mongory_memory_pool *temp_pool = mongory_memory_pool_new();
  if(!temp_pool) return NULL;
  mongory_array *or_branch_matchers = mongory_array_new(temp_pool);
  if (or_branch_matchers == NULL) {
    temp_pool->free(temp_pool);
    return NULL;
  }

  mongory_matcher_table_build_sub_matcher_context build_ctx = {
      pool, or_branch_matchers};
  if (!array_of_tables->each(array_of_tables, &build_ctx,
                           mongory_matcher_build_or_sub_matcher)) {
      temp_pool->free(temp_pool);
      return NULL; // Failure building one of the OR branches
  }

  if(or_branch_matchers->count == 0) { // Should not happen
      temp_pool->free(temp_pool);
      return mongory_matcher_always_false_new(pool, condition);
  }

  mongory_matcher *final_matcher = mongory_matcher_construct_by_or(
      or_branch_matchers, 0, or_branch_matchers->count - 1);

  if (final_matcher && final_matcher->condition == NULL) {
    final_matcher->condition = condition; // Original $or condition
  }
  temp_pool->free(temp_pool);
  return final_matcher;
}

/**
 * @brief Callback for $elemMatch: checks if a single array element matches.
 * The `acc` is the sub-matcher (from $elemMatch's condition).
 * This callback is used with `array->each`. `each` stops if callback returns
 * false. So, for $elemMatch (find AT LEAST ONE), this should return `false`
 * (stop) upon first match.
 * @param value_in_array An element from the array being checked.
 * @param sub_matcher_for_element The matcher derived from $elemMatch's condition.
 * @return `false` if `value_in_array` matches `sub_matcher_for_element` (to
 * stop iteration), `true` otherwise (to continue).
 */
static inline bool mongory_matcher_elem_match_unit_compare(
    mongory_value *value_in_array, void *sub_matcher_for_element) {
  mongory_matcher *matcher = (mongory_matcher *)sub_matcher_for_element;
  // If it matches, we found one, so stop iterating (return false for each).
  return !matcher->match(matcher, value_in_array);
}

/**
 * @brief Match function for $elemMatch.
 * Checks if any element in the input array `value` matches the condition
 * stored in `composite->left`.
 * @param matcher The $elemMatch composite matcher.
 * @param value_to_check The input value, expected to be an array.
 * @return True if `value_to_check` is an array and at least one of its elements
 * matches.
 */
static inline bool mongory_matcher_elem_match_match(mongory_matcher *matcher,
                                                    mongory_value *value_to_check) {
  if (!value_to_check || value_to_check->type != MONGORY_TYPE_ARRAY || !value_to_check->data.a) {
    return false; // $elemMatch applies to arrays.
  }
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  if (!composite->left) return false; // No sub-matcher to apply.

  mongory_array *target_array = value_to_check->data.a;
  if (target_array->count == 0) return false; // Empty array cannot have a matching element.

  // `array->each` returns true if fully iterated, false if callback stopped it.
  // `elem_match_unit_compare` returns false to stop if a match is found.
  // So, if `each` returns false, it means a match was found.
  return !target_array->each(target_array, composite->left,
                             mongory_matcher_elem_match_unit_compare);
}

/**
 * @brief Creates an $elemMatch matcher.
 * The `condition` (a table) is used to create a sub-matcher
 * (`composite->left`) which is then applied to each element of an input array.
 * @param pool Memory pool for allocations.
 * @param condition The table condition for matching array elements.
 * @return A new $elemMatch matcher, or NULL on failure.
 */
mongory_matcher *mongory_matcher_elem_match_new(mongory_memory_pool *pool,
                                                mongory_value *condition) {
  mongory_composite_matcher *composite =
      mongory_matcher_composite_new(pool, condition);
  if (composite == NULL) {
    return NULL;
  }
  // The 'left' child will be the matcher for individual elements.
  composite->left = mongory_matcher_table_cond_new(pool, condition);
  if (composite->left == NULL) {
    // If pool is not self-cleaning for composite on child failure:
    // mongory_matcher_composite_destroy(composite); // or similar
    return NULL; // Failed to create sub-matcher for elements.
  }
  composite->base.match = mongory_matcher_elem_match_match;
  composite->base.context.original_match = mongory_matcher_elem_match_match;

  return (mongory_matcher *)composite;
}

/**
 * @brief Callback for $every: checks if a single array element matches.
 * The `acc` is the sub-matcher. This callback is used with `array->each`.
 * `each` stops if callback returns false. For $every (ALL must match), this
 * should return `true` (continue) if element matches, and `false` (stop) if
 * element does NOT match.
 * @param value_in_array An element from the array being checked.
 * @param sub_matcher_for_element The matcher derived from $every's condition.
 * @return `true` if `value_in_array` matches (continue), `false` if it
 * doesn't (stop).
 */
static inline bool
mongory_matcher_every_match_unit_compare(mongory_value *value_in_array,
                                         void *sub_matcher_for_element) {
  mongory_matcher *matcher = (mongory_matcher *)sub_matcher_for_element;
  // If it matches, continue. If it doesn't match, stop.
  return matcher->match(matcher, value_in_array);
}

/**
 * @brief Match function for $every.
 * Checks if all elements in the input array `value` match the condition
 * stored in `composite->left`.
 * @param matcher The $every composite matcher.
 * @param value_to_check The input value, expected to be an array.
 * @return True if `value_to_check` is an array and all its elements match (or
 * if array is empty).
 */
static inline bool mongory_matcher_every_match(mongory_matcher *matcher,
                                               mongory_value *value_to_check) {
  if (!value_to_check || value_to_check->type != MONGORY_TYPE_ARRAY || !value_to_check->data.a) {
    // Or, should an $every condition on a non-array be false or an error?
    // Current: false, as no elements (all zero of them) satisfy it.
    // If an empty array should match $every, this needs adjustment.
    // MongoDB's $all on empty query array or empty target array has specific rules.
    // This $every is simpler: "do all present elements match?".
    return false;
  }
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
   if (!composite->left) return true; // No sub-matcher means condition is vacuously true for all elements.

  mongory_array *target_array = value_to_check->data.a;
  if (target_array->count == 0) return true; // All (zero) elements match.

  // `array->each` returns true if fully iterated (all elements matched),
  // false if callback stopped it (an element did NOT match).
  return target_array->each(target_array, composite->left,
                            mongory_matcher_every_match_unit_compare);
}

/**
 * @brief Creates an $every matcher.
 * The `condition` (a table) is used to create a sub-matcher
 * (`composite->left`) which is then applied to each element of an input array.
 * @param pool Memory pool for allocations.
 * @param condition The table condition for matching array elements.
 * @return A new $every matcher, or NULL on failure.
 */
mongory_matcher *mongory_matcher_every_new(mongory_memory_pool *pool,
                                           mongory_value *condition) {
  mongory_composite_matcher *composite =
      mongory_matcher_composite_new(pool, condition);
  if (composite == NULL) {
    return NULL;
  }
  composite->left = mongory_matcher_table_cond_new(pool, condition);
  if (composite->left == NULL) {
    return NULL;
  }
  composite->base.match = mongory_matcher_every_match;
  composite->base.context.original_match = mongory_matcher_every_match;

  return (mongory_matcher *)composite;
}
