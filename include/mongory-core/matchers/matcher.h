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
 * This function typically serves as a high-level entry point for creating
 * matchers. In the current implementation, it delegates to
 * `mongory_matcher_table_cond_new`, implying that conditions are often
 * table-based (similar to query documents).
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
 * @brief Builds and returns the explain tree string for a matcher.
 * @param matcher The matcher to explain.
 * @param temp_pool The temporary pool to allocate the formatted tree string from.
 * @return A NUL-terminated explain tree string allocated in `temp_pool`, or NULL on error.
 */
char *mongory_matcher_explain_tree(mongory_matcher *matcher, mongory_memory_pool *temp_pool);

/**
 * @brief Explains a matcher by printing `mongory_matcher_explain_tree` to stdout.
 * @param matcher The matcher to explain.
 * @param temp_pool The temporary pool to use for the explanation.
 */
void mongory_matcher_explain(mongory_matcher *matcher, mongory_memory_pool *temp_pool);

/**
 * @brief Traces a matcher against `value` and returns the formatted trace tree string.
 * @param matcher The matcher to trace.
 * @param value The value to trace.
 * @return A NUL-terminated trace tree string allocated in `value->pool` (or `matcher->pool` if `value` is NULL), or NULL on error.
 */
char *mongory_matcher_trace_tree(mongory_matcher *matcher, mongory_value *value);

/**
 * @brief Traces a matcher, prints the trace tree to stdout, and returns the match result.
 * @param matcher The matcher to trace.
 * @param value The value to trace.
 */
bool mongory_matcher_trace(mongory_matcher *matcher, mongory_value *value);

/**
 * @brief Formats the currently recorded trace stack into a string.
 * @param matcher The matcher whose trace stack should be formatted.
 * @return A NUL-terminated trace string allocated in the trace stack pool, or NULL if no trace stack is active.
 */
char *mongory_matcher_format_trace(mongory_matcher *matcher);

/**
 * @brief Prints the trace of a matcher by printing `mongory_matcher_format_trace` to stdout.
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
