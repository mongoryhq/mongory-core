#ifndef MONGORY_MATCHER_H
#define MONGORY_MATCHER_H

/**
 * @file matcher.h
 * @brief Defines the core mongory_matcher structure and related types.
 *
 * This file provides the basic structure for all matchers in the Mongory
 * library. A matcher is responsible for determining if a given `mongory_value`
 * meets certain criteria defined by a `condition`.
 */

#include "mongory-core/foundations/array.h" // For mongory_array (used in context)
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"

// Forward declaration for the main matcher structure.
typedef struct mongory_matcher mongory_matcher;
/**
 * @brief Creates a new generic matcher instance.
 *
 * Expands unescaped dotted field paths (e.g. `"a.b.c": 1` -> `{"a": {"b": {"c": 1}}}`),
 * unescapes `\.` into literal `.`, and preserves leaf collisions via `$and` before
 * building the matcher tree.
 *
 * @param pool The memory pool to use for the matcher's allocations.
 * @param condition A `mongory_value` representing the condition for this
 * matcher.
 * @return mongory_matcher* A pointer to the newly created matcher, or NULL on
 * failure.
 */
mongory_matcher *mongory_matcher_new(mongory_memory_pool *pool, mongory_value *condition, void *extern_ctx);

/**
 * @brief Matches a value against a matcher.
 * @param matcher The matcher to match against.
 * @param value The value to match.
 * @return True if the value matches the matcher, false otherwise.
 */
bool mongory_matcher_match(mongory_matcher *matcher, mongory_value *value);

/**
 * @brief Builds and returns the structured explain tree for a matcher.
 *
 * Each node is a `mongory_value` wrapping a `mongory_table` with keys:
 * `"name"`, `"condition"`, `"priority"`, `"children"` (array of child nodes),
 * and `"field"` (for Field matchers).
 *
 * @param matcher The matcher to explain.
 * @param temp_pool The temporary pool to allocate the tree from.
 * @return A `mongory_value*` table representing the root explain node, or NULL on error.
 */
mongory_value *mongory_matcher_explain_tree(mongory_matcher *matcher, mongory_memory_pool *temp_pool);

/**
 * @brief Explains a matcher by printing `mongory_matcher_explain_tree` to stdout.
 * @param matcher The matcher to explain.
 * @param temp_pool The temporary pool to use for the explanation.
 */
void mongory_matcher_explain(mongory_matcher *matcher, mongory_memory_pool *temp_pool);

/**
 * @brief Traces a matcher against `value` and returns the structured trace tree.
 *
 * Each node is a `mongory_value` wrapping a `mongory_table` with keys:
 * `"name"`, `"condition"`, `"priority"`, `"result"` (`"pass"`, `"fail"`, or `"skip"`),
 * `"children"` (array of child nodes), optional `"field"` (for Field matchers),
 * and optional `"record"` (omitted for skipped nodes or missing values).
 *
 * @param matcher The matcher to trace.
 * @param value The value to trace.
 * @return A `mongory_value*` table representing the root trace node, or NULL on error.
 */
mongory_value *mongory_matcher_trace_tree(mongory_matcher *matcher, mongory_value *value);

/**
 * @brief Traces a matcher, prints the trace tree to stdout, and returns the match result.
 * @param matcher The matcher to trace.
 * @param value The value to trace.
 */
bool mongory_matcher_trace(mongory_matcher *matcher, mongory_value *value);

/**
 * @brief Prints the trace of a matcher by rendering its recorded trace tree to stdout.
 * @param matcher The matcher to print the trace of.
 */
void mongory_matcher_print_trace(mongory_matcher *matcher);

/**
 * @brief Enables the trace of a matcher.
 * @param matcher The matcher to enable the trace of.
 * @param temp_pool The temporary pool to use for the trace.
 */
void mongory_matcher_enable_trace(mongory_matcher *matcher, mongory_memory_pool *temp_pool);

/**
 * @brief Disables the trace of a matcher.
 * @param matcher The matcher to disable the trace of.
 */
void mongory_matcher_disable_trace(mongory_matcher *matcher);

#endif /* MONGORY_MATCHER_H */
