/**
 * @file composite_matcher.c
 * @brief Implements composite matchers like AND, OR, $elemMatch, and the
 * core table condition parser. This is an internal implementation file for the
 * matcher module.
 */
#include "composite_matcher.h"
#include "external_matcher.h"
#include "../foundations/config_private.h"  // For mongory_matcher_build_func_get
#include "../foundations/array_private.h"   // For mongory_array_sort_by
#include "../foundations/memory_pool_private.h" // For atomic children load/store
#include "../foundations/string_buffer.h"   // For mongory_string_buffer_new
#include "base_matcher.h"                   // For mongory_matcher_always_true_new, etc.
#include "value_matcher.h"                // For mongory_matcher_field_new
#include "mongory-core/foundations/error.h" // For MONGORY_ERROR_INVALID_ARGUMENT
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/table.h" // For mongory_table operations
#include "mongory-core/foundations/value.h"
#include "matcher_traversable.h"
#include "../foundations/utils.h"
#include <mongory-core.h> // General include
#include <stdio.h>        // For sprintf
#include <string.h>       // For strcmp

// Forward declaration.
double mongory_matcher_calculate_priority(mongory_array *sub_matchers);
mongory_array *mongory_matcher_sort_matchers(mongory_array *sub_matchers);

static inline void mongory_matcher_attach_children_parent(mongory_composite_matcher *parent, mongory_array *children) {
  if (!parent || !children) {
    return;
  }
  for (size_t i = 0; i < children->count; i++) {
    mongory_matcher *child = (mongory_matcher *)children->get(children, i);
    if (child != NULL) {
      child->parent = (mongory_matcher *)parent;
    }
  }
}

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
// ============================================================================
// Core Composite Matcher Functions
// ============================================================================
mongory_composite_matcher *mongory_matcher_composite_new(mongory_memory_pool *pool, mongory_value *condition, void *extern_ctx) {
  if (!mongory_validate_pool(pool))
    return NULL;
  if (!mongory_compile_budget_count_node(pool))
    return NULL;

  mongory_composite_matcher *composite = MG_ALLOC_PTR(pool, mongory_composite_matcher);
  if (composite == NULL) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL; // Allocation failed.
  }
  // Initialize base matcher fields
  composite->base.pool = pool;
  composite->base.name = NULL;                                 // Specific name to be set by derived type if any
  composite->base.match = NULL;                                // Specific match fn to be set by derived type
  composite->base.original_match = NULL;
  composite->base.condition = condition;
  composite->base.traverse = mongory_matcher_composite_traverse;
  composite->base.extern_ctx = extern_ctx;
  composite->base.priority = 2.0;
  composite->base.parent = NULL;
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
// ============================================================================
// Logical Operator Match Functions (AND, OR)
// ============================================================================
static inline bool mongory_matcher_and_match(mongory_matcher *matcher, mongory_value *value) {
  if (!matcher) {
    return false;
  }
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  mongory_array *children = mongory_atomic_load_ptr_acquire(&composite->children);
  if (!children) {
    return false;
  }
  int total = (int)children->count;
  for (int i = 0; i < total; i++) {
    mongory_matcher *child = (mongory_matcher *)children->get(children, i);
    if (!child || !child->match || !child->match(child, value)) {
      return false;
    }
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
  if (!matcher) {
    return false;
  }
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  mongory_array *children = mongory_atomic_load_ptr_acquire(&composite->children);
  if (!children) {
    return false;
  }
  int total = (int)children->count;
  for (int i = 0; i < total; i++) {
    mongory_matcher *child = (mongory_matcher *)children->get(children, i);
    if (child && child->match && child->match(child, value)) {
      return true;
    }
  }
  return false; // Neither matched (or children didn't exist).
}

static inline bool mongory_matcher_nor_match(mongory_matcher *matcher, mongory_value *value) {
  if (!matcher) {
    return false;
  }
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  mongory_array *children = mongory_atomic_load_ptr_acquire(&composite->children);
  if (!children) {
    return false;
  }
  int total = (int)children->count;
  for (int i = 0; i < total; i++) {
    mongory_matcher *child = (mongory_matcher *)children->get(children, i);
    if (child && child->match && child->match(child, value)) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Context structure for building sub-matchers from a table.
 */
typedef struct mongory_matcher_table_build_sub_matcher_context {
  mongory_memory_pool *pool; /**< Main pool for allocating created matchers. */
  mongory_array *matchers;   /**< Array to store the created sub-matchers. */
  void *extern_ctx;          /**< External context for the matcher. */
  mongory_table *table;      /**< Table being iterated, for sibling lookups ($regex/$options). */
} mongory_matcher_table_build_sub_matcher_context;

static inline mongory_matcher *mongory_matcher_build_sub_matcher(char *key, mongory_value *value, mongory_matcher_table_build_sub_matcher_context *ctx);

static inline void mongory_matcher_build_error_set(mongory_memory_pool *pool, const char *message) {
  mongory_error_set(pool, MONGORY_ERROR_INVALID_ARGUMENT, message);
}

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
static inline bool mongory_matcher_table_build_sub_matcher(char *key, mongory_value *value, void *acc) {
  mongory_matcher_table_build_sub_matcher_context *ctx = (mongory_matcher_table_build_sub_matcher_context *)acc;
  mongory_array *matchers_array = ctx->matchers;
  if (strcmp(key, "$options") == 0) {
    // Consumed by the sibling $regex; only its presence is checked here.
    if (ctx->table == NULL || ctx->table->get(ctx->table, "$regex") == NULL) {
      mongory_matcher_build_error_set(ctx->pool, "$options requires a sibling $regex.");
      return false;
    }
    return true;
  }
  mongory_matcher *sub_matcher = mongory_matcher_build_sub_matcher(key, value, ctx);
  if (sub_matcher == NULL) {
    // Failed to create sub-matcher (e.g., allocation error, invalid condition
    // for sub-matcher)
    return false;
  }

  if (!matchers_array->push(matchers_array, (mongory_value *)sub_matcher)) {
    return false;
  }
  return true;
}

/**
 * @brief Builds a sub-matcher from a key-value pair.
 *
 * This function determines the appropriate sub-matcher type based on the key:
 * - If the key starts with '$', it looks up a registered matcher builder
 *   (`$regex` also takes its sibling `$options`). Unknown operators fail.
 * - Otherwise, it creates a field matcher.
 *
 * @param key The key from the condition table.
 * @param value The value associated with the key.
 * @param ctx Pointer to `mongory_matcher_table_build_sub_matcher_context`.
 * @return A new sub-matcher, or NULL on failure.
 */
// ============================================================================
// Matcher Construction from Conditions
// ============================================================================
static inline mongory_matcher *mongory_matcher_build_sub_matcher(char *key, mongory_value *value, mongory_matcher_table_build_sub_matcher_context *ctx) {
  mongory_memory_pool *pool = ctx->pool;
  mongory_matcher_build_func build_func = NULL;

  if (key[0] == '$') { // Operator key (e.g., "$eq", "$in")
    if (strcmp(key, "$regex") == 0) {
      mongory_value *options = ctx->table ? ctx->table->get(ctx->table, "$options") : NULL;
      return mongory_matcher_regex_with_options_new(pool, value, options, ctx->extern_ctx);
    }
    build_func = mongory_matcher_build_func_get(key);
    if (build_func != NULL) {
      return build_func(pool, value, ctx->extern_ctx);
    } else if (mongory_custom_matcher_adapter.lookup != NULL && mongory_custom_matcher_adapter.lookup(key, ctx->extern_ctx)) {
      return mongory_matcher_custom_new(pool, key, value, ctx->extern_ctx);
    }
    mongory_matcher_build_error_set(pool, mongory_string_cpyf(pool, "Unknown operator: %s", key));
    return NULL;
  }

  return mongory_matcher_field_new(pool, key, value, ctx->extern_ctx);
}

/**
 * @brief Creates a matcher from a table-based condition.
 *
 * Parses the `condition` table, creating sub-matchers for each key-value pair.
 *
 * This is a core function of the query engine. It takes a query document (a
 * table) and builds a tree of matchers that represents the logic of that query.
 *
 * The process is as follows:
 * 1. Iterate through each key-value pair in the `condition` table.
 * 2. For each pair, create a specific sub-matcher (e.g., a `field_matcher` for
 *    a field name, or a `$gt` matcher for a `"$gt"` operator).
 * 3. Store all these sub-matchers in a temporary array.
 * 4. Use `mongory_matcher_binary_construct` to combine all the sub-matchers
 *    into a single matcher tree using AND logic.
 *
 * @param pool Memory pool for allocations.
 * @param condition A `mongory_value` of type `MONGORY_TYPE_TABLE`.
 * @return A `mongory_matcher` representing the combined logic of the table, or NULL on failure.
 */
mongory_matcher *mongory_matcher_condition_new(mongory_memory_pool *pool, mongory_value *table_condition, void *extern_ctx) {
  if (!MONGORY_VALIDATE_TABLE(pool, table_condition)) {
    return NULL;
  }
  if (!mongory_compile_budget_enter_depth(pool)) {
    return NULL;
  }

  mongory_table *table = table_condition->data.t;
  if (table->count == 0) {
    // Empty table condition matches everything.
    mongory_matcher *res = mongory_matcher_always_true_new(pool, table_condition, extern_ctx);
    mongory_compile_budget_leave_depth();
    return res;
  }

  mongory_array *sub_matchers = mongory_array_new(pool);
  if (sub_matchers == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL; // Failed to create array for sub-matchers.
  }

  mongory_matcher_table_build_sub_matcher_context build_ctx = {pool, sub_matchers, extern_ctx, table};
  // Iterate over the condition table, building sub-matchers.
  if (!table->each(table, &build_ctx, mongory_matcher_table_build_sub_matcher)) {
    // Building one of the sub-matchers failed.
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  if (sub_matchers->count == 1) {
    mongory_matcher *res = (mongory_matcher *)sub_matchers->get(sub_matchers, 0);
    mongory_compile_budget_leave_depth();
    return res;
  }

  // Combine sub-matchers using AND logic.
  mongory_composite_matcher *final_matcher = mongory_matcher_composite_new(pool, table_condition, extern_ctx);
  if (final_matcher == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  final_matcher->children = mongory_matcher_sort_matchers(sub_matchers);
  if (final_matcher->children == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  mongory_matcher_attach_children_parent(final_matcher, final_matcher->children);
  final_matcher->base.match = mongory_matcher_and_match;
  final_matcher->base.original_match = mongory_matcher_and_match;
  final_matcher->base.name = mongory_string_cpy(pool, "And");
  if (final_matcher->base.name == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  final_matcher->base.priority += mongory_matcher_calculate_priority(sub_matchers);
  mongory_compile_budget_leave_depth();
  return (mongory_matcher *)final_matcher;
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
static inline bool mongory_matcher_build_and_sub_matcher(mongory_value *and_sub_condition, void *acc) {
  // The 'and_sub_condition' is one of the tables in the $and:[{}, {}, ...] array.
  // We need to build all matchers from this table and add them to the list.
  // The list in 'acc' (ctx->matchers) will then be ANDed together.
  mongory_matcher_table_build_sub_matcher_context *ctx = (mongory_matcher_table_build_sub_matcher_context *)acc;
  if (!MONGORY_VALIDATE_TABLE(ctx->pool, and_sub_condition)) {
    return false; // Element in $and array is not a table.
  }
  ctx->table = and_sub_condition->data.t;
  return and_sub_condition->data.t->each(and_sub_condition->data.t, acc, mongory_matcher_table_build_sub_matcher);
}

/**
 * @brief Creates an "AND" ($and) matcher from an array of condition tables.
 * @param pool Memory pool for allocations.
 *
 * The `$and` operator takes an array of query documents. This function builds
 * a single, flat list of all the sub-matchers from all the query documents,
 * and then combines them into one large AND-connected matcher tree.
 *
 * @param pool Memory pool for allocations.
 * @param condition A `mongory_value` array of table conditions.
 * @return A new $and matcher, or NULL on failure.
 */
mongory_matcher *mongory_matcher_and_new(mongory_memory_pool *pool, mongory_value *and_condition, void *extern_ctx) {
  if (!MONGORY_VALIDATE_ARRAY(pool, and_condition)) {
    return NULL;
  }
  if (!mongory_compile_budget_enter_depth(pool)) {
    return NULL;
  }

  mongory_array *array_of_tables = and_condition->data.a;
  if (array_of_tables->count == 0) {
    mongory_matcher *res = mongory_matcher_always_true_new(pool, and_condition, extern_ctx); // $and:[] is true
    mongory_compile_budget_leave_depth();
    return res;
  }
  mongory_value *sub_condition_of_and_condition = array_of_tables->get(array_of_tables, 0);

  if (!MONGORY_VALIDATE_TABLE(pool, sub_condition_of_and_condition)) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  mongory_array *sub_matchers = mongory_array_new(pool);
  if (sub_matchers == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  // Context for building matchers from EACH table within the $and array.
  mongory_matcher_table_build_sub_matcher_context build_ctx = {pool, sub_matchers, extern_ctx, NULL};
  // Iterate through the array of tables provided in the $and condition.
  // mongory_matcher_build_and_sub_matcher will then iterate keys of EACH table.
  int total = (int)array_of_tables->count;
  for (int i = 0; i < total; i++) {
    mongory_value *table = array_of_tables->get(array_of_tables, i);
    if (!mongory_matcher_build_and_sub_matcher(table, &build_ctx)) {
      mongory_compile_budget_leave_depth();
      return NULL; // Failure during sub-matcher construction.
    }
  }

  if (sub_matchers->count == 0) {
    mongory_matcher *res = mongory_matcher_always_true_new(pool, and_condition, extern_ctx);
    mongory_compile_budget_leave_depth();
    return res;
  }

  if (sub_matchers->count == 1) {
    mongory_matcher *res = (mongory_matcher *)sub_matchers->get(sub_matchers, 0);
    mongory_compile_budget_leave_depth();
    return res;
  }

  mongory_composite_matcher *final_matcher = mongory_matcher_composite_new(pool, and_condition, extern_ctx);
  if (final_matcher == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  final_matcher->children = mongory_matcher_sort_matchers(sub_matchers);
  if (final_matcher->children == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  mongory_matcher_attach_children_parent(final_matcher, final_matcher->children);
  final_matcher->base.match = mongory_matcher_and_match;
  final_matcher->base.original_match = mongory_matcher_and_match;
  final_matcher->base.name = mongory_string_cpy(pool, "And");
  if (final_matcher->base.name == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  final_matcher->base.priority += mongory_matcher_calculate_priority(sub_matchers);
  mongory_compile_budget_leave_depth();
  return (mongory_matcher *)final_matcher;
}

/**
 * @brief Callback for $or constructor. Each element in the $or array is a
 * complete table condition, which is turned into a single matcher. These
 * "table condition matchers" are then ORed.
 * @param condition_table A `mongory_value` (table) from the $or array.
 * @param acc Pointer to `mongory_matcher_table_build_sub_matcher_context`.
 * The `matchers` array in context will store the result of
 * `mongory_matcher_condition_new`.
 * @return True if successful, false otherwise.
 */
static inline bool mongory_matcher_build_or_sub_matcher(mongory_value *condition_table, void *acc) {
  mongory_matcher_table_build_sub_matcher_context *ctx = (mongory_matcher_table_build_sub_matcher_context *)acc;
  mongory_memory_pool *pool_for_new_matchers = ctx->pool;
  mongory_array *array_to_store_table_matchers = ctx->matchers;

  // Each 'condition_table' is a complete query object for one branch of the OR.
  // So, create a full table_cond_new matcher for it.
  mongory_matcher *table_level_matcher = mongory_matcher_condition_new(pool_for_new_matchers, condition_table, ctx->extern_ctx);
  if (table_level_matcher == NULL) {
    return false; // Failed to create a matcher for this OR branch.
  }
  if (!array_to_store_table_matchers->push(array_to_store_table_matchers, (mongory_value *)table_level_matcher)) {
    return false;
  }
  return true;
}

/**
 * @brief Creates an "OR" ($or) matcher from an array of condition tables.
 * @param pool Memory pool for allocations.
 *
 * The `$or` operator takes an array of query documents. For each document in
 * the array, this function creates a complete sub-matcher (using
 * `table_cond_new`). It then combines these top-level sub-matchers into an
 * OR-connected tree. This is different from `$and`, which flattens the list.
 *
 * @param pool Memory pool for allocations.
 * @param condition A `mongory_value` array of table conditions.
 * @return A new $or matcher, or NULL on failure.
 */
mongory_matcher *mongory_matcher_or_new(mongory_memory_pool *pool, mongory_value *or_condition, void *extern_ctx) {
  if (!MONGORY_VALIDATE_ARRAY(pool, or_condition)) {
    return NULL;
  }
  if (!mongory_compile_budget_enter_depth(pool)) {
    return NULL;
  }
  mongory_array *array_of_tables = or_condition->data.a;
  if (array_of_tables->count == 0) {
    mongory_matcher *res = mongory_matcher_always_false_new(pool, or_condition, extern_ctx); // $or:[] is false
    mongory_compile_budget_leave_depth();
    return res;
  }
  mongory_value *sub_condition_of_or_condition = array_of_tables->get(array_of_tables, 0);
  if (!MONGORY_VALIDATE_TABLE(pool, sub_condition_of_or_condition)) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  mongory_array *sub_matchers = mongory_array_new(pool);
  if (sub_matchers == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  mongory_matcher_table_build_sub_matcher_context build_ctx = {pool, sub_matchers, extern_ctx, NULL};
  int total = (int)array_of_tables->count;
  for (int i = 0; i < total; i++) {
    mongory_value *table = array_of_tables->get(array_of_tables, i);
    if (!mongory_matcher_build_or_sub_matcher(table, &build_ctx)) {
      mongory_compile_budget_leave_depth();
      return NULL; // Failure building one of the OR branches
    }
  }

  if (sub_matchers->count == 1) {
    mongory_matcher *res = (mongory_matcher *)sub_matchers->get(sub_matchers, 0);
    mongory_compile_budget_leave_depth();
    return res;
  }

  mongory_composite_matcher *final_matcher = mongory_matcher_composite_new(pool, or_condition, extern_ctx);
  if (final_matcher == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  final_matcher->children = mongory_matcher_sort_matchers(sub_matchers);
  if (final_matcher->children == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  mongory_matcher_attach_children_parent(final_matcher, final_matcher->children);
  final_matcher->base.match = mongory_matcher_or_match;
  final_matcher->base.original_match = mongory_matcher_or_match;
  final_matcher->base.name = mongory_string_cpy(pool, "Or");
  if (final_matcher->base.name == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  final_matcher->base.priority += mongory_matcher_calculate_priority(sub_matchers);
  mongory_compile_budget_leave_depth();
  return (mongory_matcher *)final_matcher;
}

mongory_matcher *mongory_matcher_nor_new(mongory_memory_pool *pool, mongory_value *nor_condition, void *extern_ctx) {
  if (!MONGORY_VALIDATE_ARRAY(pool, nor_condition)) {
    return NULL;
  }
  if (!mongory_compile_budget_enter_depth(pool)) {
    return NULL;
  }
  mongory_array *array_of_tables = nor_condition->data.a;
  if (array_of_tables->count == 0) {
    mongory_matcher *res = mongory_matcher_always_true_new(pool, nor_condition, extern_ctx); // $nor:[] is true
    mongory_compile_budget_leave_depth();
    return res;
  }
  mongory_value *first = array_of_tables->get(array_of_tables, 0);
  if (!MONGORY_VALIDATE_TABLE(pool, first)) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  mongory_array *sub_matchers = mongory_array_new(pool);
  if (sub_matchers == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  mongory_matcher_table_build_sub_matcher_context build_ctx = {pool, sub_matchers, extern_ctx, NULL};
  int total = (int)array_of_tables->count;
  for (int i = 0; i < total; i++) {
    mongory_value *table = array_of_tables->get(array_of_tables, i);
    if (!mongory_matcher_build_or_sub_matcher(table, &build_ctx)) {
      mongory_compile_budget_leave_depth();
      return NULL;
    }
  }

  mongory_composite_matcher *final_matcher = mongory_matcher_composite_new(pool, nor_condition, extern_ctx);
  if (final_matcher == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  final_matcher->children = mongory_matcher_sort_matchers(sub_matchers);
  if (final_matcher->children == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  mongory_matcher_attach_children_parent(final_matcher, final_matcher->children);
  final_matcher->base.match = mongory_matcher_nor_match;
  final_matcher->base.original_match = mongory_matcher_nor_match;
  final_matcher->base.name = mongory_string_cpy(pool, "Nor");
  if (final_matcher->base.name == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  final_matcher->base.priority += mongory_matcher_calculate_priority(sub_matchers);
  mongory_compile_budget_leave_depth();
  return (mongory_matcher *)final_matcher;
}

/**
 * @brief Match function for $elemMatch.
 * Checks if any element in the input array `value` matches the condition
 * stored in `composite->children`.
 * @param matcher The $elemMatch composite matcher.
 * @param value_to_check The input value, expected to be an array.
 * @return True if `value_to_check` is an array and at least one of its elements
 * matches.
 */
// ============================================================================
// Array-based Match Functions ($elemMatch, $every)
// ============================================================================
static inline bool mongory_matcher_elem_match_match(mongory_matcher *matcher, mongory_value *elem_match_target) {
  if (!matcher || elem_match_target == NULL || elem_match_target->type != MONGORY_TYPE_ARRAY ||
      elem_match_target->data.a == NULL) {
    return false; // $elemMatch applies to valid arrays.
  }
  mongory_array *target_array = elem_match_target->data.a;
  if (target_array->count == 0)
    return false; // Empty array cannot have a matching element.

  int total = (int)target_array->count;
  for (int i = 0; i < total; i++) {
    mongory_value *value = target_array->get(target_array, i);
    if (mongory_matcher_and_match(matcher, value)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Creates an $elemMatch matcher.
 * The `condition` (a table) is used to create a sub-matcher
 * (`composite->left`) which is then applied to each element of an input array.
 * @param pool Memory pool for allocations.
 * @param condition The table condition for matching array elements.
 * @return A new $elemMatch matcher, or NULL on failure.
 */
mongory_matcher *mongory_matcher_elem_match_new(mongory_memory_pool *pool, mongory_value *elem_match_condition, void *extern_ctx) {
  if (!MONGORY_VALIDATE_TABLE(pool, elem_match_condition)) {
    return NULL;
  }
  if (!mongory_compile_budget_enter_depth(pool)) {
    return NULL;
  }
  mongory_array *sub_matchers = mongory_array_new(pool);
  if (sub_matchers == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  mongory_matcher_table_build_sub_matcher_context build_ctx = {pool, sub_matchers, extern_ctx, NULL};
  if (!mongory_matcher_build_and_sub_matcher(elem_match_condition, &build_ctx)) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  mongory_composite_matcher *composite = mongory_matcher_composite_new(pool, elem_match_condition, extern_ctx);
  if (composite == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  composite->children = mongory_matcher_sort_matchers(sub_matchers);
  if (composite->children == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  mongory_matcher_attach_children_parent(composite, composite->children);
  composite->base.match = mongory_matcher_elem_match_match;
  composite->base.original_match = mongory_matcher_elem_match_match;
  composite->base.name = mongory_string_cpy(pool, "ElemMatch");
  if (composite->base.name == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  composite->base.priority = 8.0 + 2.0 * mongory_matcher_calculate_priority(sub_matchers);
  mongory_compile_budget_leave_depth();
  return (mongory_matcher *)composite;
}

/**
 * @brief Match function for $every.
 * Checks if all elements in the input array `value` match the condition
 * stored in `composite->children`.
 * @param matcher The $every composite matcher.
 * @param value_to_check The input value, expected to be an array.
 * @return True if `value_to_check` is an array and all its elements match (or
 * if array is empty).
 */
static inline bool mongory_matcher_every_match(mongory_matcher *matcher, mongory_value *every_match_target) {
  if (!matcher || every_match_target == NULL || every_match_target->type != MONGORY_TYPE_ARRAY ||
      every_match_target->data.a == NULL) {
    return false;
  }
  mongory_array *target_array = every_match_target->data.a;
  if (target_array->count == 0)
    return false; // Non-empty array must have at least one element.

  int total = (int)target_array->count;
  for (int i = 0; i < total; i++) {
    mongory_value *value = target_array->get(target_array, i);
    if (!mongory_matcher_and_match(matcher, value)) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Creates an $every matcher.
 * The `condition` (a table) is used to create a sub-matcher
 * (`composite->left`) which is then applied to each element of an input array.
 * @param pool Memory pool for allocations.
 * @param condition The table condition for matching array elements.
 * @return A new $every matcher, or NULL on failure.
 */
mongory_matcher *mongory_matcher_every_new(mongory_memory_pool *pool, mongory_value *every_condition, void *extern_ctx) {
  if (!MONGORY_VALIDATE_TABLE(pool, every_condition)) {
    return NULL;
  }
  if (!mongory_compile_budget_enter_depth(pool)) {
    return NULL;
  }
  mongory_array *sub_matchers = mongory_array_new(pool);
  if (sub_matchers == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  mongory_matcher_table_build_sub_matcher_context build_ctx = {pool, sub_matchers, extern_ctx, NULL};
  if (!mongory_matcher_build_and_sub_matcher(every_condition, &build_ctx)) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  mongory_composite_matcher *composite = mongory_matcher_composite_new(pool, every_condition, extern_ctx);
  if (composite == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }

  composite->children = mongory_matcher_sort_matchers(sub_matchers);
  if (composite->children == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  mongory_matcher_attach_children_parent(composite, composite->children);
  composite->base.match = mongory_matcher_every_match;
  composite->base.original_match = mongory_matcher_every_match;
  composite->base.name = mongory_string_cpy(pool, "Every");
  if (composite->base.name == NULL) {
    mongory_compile_budget_leave_depth();
    return NULL;
  }
  composite->base.priority = 8.0 + 2.0 * mongory_matcher_calculate_priority(sub_matchers);
  mongory_compile_budget_leave_depth();
  return (mongory_matcher *)composite;
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Calculates the priority of a composite matcher.
 * @param sub_matchers The array of sub-matchers.
 * @return The priority of the composite matcher.
 */
double mongory_matcher_calculate_priority(mongory_array *sub_matchers) {
  if (!sub_matchers) {
    return 0.0;
  }
  double priority = 0.0;
  for (size_t i = 0; i < sub_matchers->count; i++) {
    priority += ((mongory_matcher *)sub_matchers->get(sub_matchers, i))->priority;
  }
  return priority;
}

static inline size_t mongory_matcher_calc_priority(mongory_value *matcher_value, void *ctx) {
  (void)ctx;
  mongory_matcher *matcher = (mongory_matcher *)matcher_value;
  return (size_t)(matcher->priority * 10000);
}

/**
 * @brief Sorts the sub-matchers by priority.
 * @param sub_matchers The array of sub-matchers.
 * @return The sorted array of sub-matchers.
 */
mongory_array *mongory_matcher_sort_matchers(mongory_array *sub_matchers) {
  if (!sub_matchers || !sub_matchers->pool) {
    return NULL;
  }
  mongory_memory_pool *temp_pool = mongory_memory_pool_new();
  if (temp_pool == NULL) {
    sub_matchers->pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }
  mongory_array *sorted_matchers = mongory_array_sort_by(sub_matchers, temp_pool, NULL, mongory_matcher_calc_priority);
  temp_pool->free(temp_pool);
  return sorted_matchers;
}

void mongory_matcher_resort_composite(mongory_composite_matcher *composite) {
  if (!composite) {
    return;
  }
  mongory_array *cur = mongory_atomic_load_ptr_acquire(&composite->children);
  if (!cur || cur->count <= 1) {
    return;
  }
  mongory_array *sorted = mongory_matcher_sort_matchers(cur);
  if (sorted != NULL) {
    mongory_atomic_store_ptr_release(&composite->children, sorted);
  }
}
