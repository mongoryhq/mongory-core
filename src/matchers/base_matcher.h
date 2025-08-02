#ifndef MONGORY_MATCHER_BASE_H
#define MONGORY_MATCHER_BASE_H

/**
 * @file base_matcher.h
 * @brief Defines the base matcher constructor and utility functions for
 * matchers. This is an internal header for the matcher module.
 *
 * This includes a constructor for the fundamental `mongory_matcher` structure,
 * constructors for trivial matchers (always true/false), and a utility
 * for parsing integers from strings.
 */

#include "mongory-core/foundations/array.h" // For mongory_array (context trace)
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "mongory-core/matchers/matcher.h" // For mongory_matcher structure
#include <stdbool.h>

/**
 * @brief Function pointer type for a matcher's core matching logic.
 *
 * @param matcher A pointer to the `mongory_matcher` instance itself.
 * @param value A pointer to the `mongory_value` to be evaluated against the
 * matcher's condition.
 * @return bool True if the `value` matches the condition, false otherwise.
 */
typedef bool (*mongory_matcher_match_func)(mongory_matcher *matcher, mongory_value *value);

/**
 * @brief Context for explaining a matcher.
 *
 * @param pool The pool to use for the explanation.
 * @param count The count of the matcher.
 * @param total The total number of matchers.
 * @param prefix The prefix to use for the explanation.
 */
typedef struct mongory_matcher_explain_context {
  mongory_memory_pool *pool;
  int count;
  int total;
  char *prefix;
} mongory_matcher_explain_context;

/**
 * @brief Function pointer type for a matcher's explanation logic.
 *
 * @param matcher The matcher to explain.
 * @param ctx The context to use for the explanation.
 */
typedef void (*mongory_matcher_explain_func)(mongory_matcher *matcher, mongory_matcher_explain_context *ctx);

/**
 * @struct mongory_matcher_context
 * @brief Context associated with a matcher instance.
 *
 * This can store the original match function (useful if the `match` function
 * pointer in `mongory_matcher` is dynamically changed, e.g., for decorators)
 * and a trace array for debugging purposes (currently not extensively used).
 */
typedef struct mongory_matcher_context {
  mongory_matcher_match_func original_match; /**< Stores the original match function, potentially for
                                                restoration or delegation. */
  mongory_array *trace;                      /**< An array that can be used for tracing matcher
                                                execution (for debugging). */
  size_t sub_count;                          /**< The number of sub-matchers. */
} mongory_matcher_context;

/**
 * @struct mongory_matcher
 * @brief Represents a generic matcher in the Mongory system.
 *
 * Each matcher has a name (optional, for identification), a condition value
 * that defines its criteria, a function pointer to its matching logic,
 * a memory pool for its allocations, and a context.
 */
struct mongory_matcher {
  char *name;                           /**< Optional name for the matcher (e.g., "$eq").
                                           String is typically allocated from the pool. */
  mongory_value *condition;             /**< The condition (a `mongory_value`) that this
                                           matcher evaluates against. */
  mongory_matcher_match_func match;     /**< Function pointer to the specific matching
                                           logic for this matcher type. */
  mongory_memory_pool *pool;            /**< The memory pool used for allocations related
                                           to this matcher instance. */
  mongory_matcher_context context;      /**< Additional context for the matcher, like
                                           tracing or original function pointers. */
  mongory_matcher_explain_func explain; /**< Function pointer to the explanation
                                          logic for this matcher type. */
};

/**
 * @brief Creates a new base `mongory_matcher` instance and initializes its
 * common fields.
 *
 * This function allocates a `mongory_matcher` structure from the provided pool
 * and sets its `pool` and `condition` fields. The `match` function and other
 * specific fields must be set by the caller or derived matcher constructors.
 * The context's original_match and trace are initialized to NULL.
 *
 * @param pool The memory pool to use for allocating the matcher.
 * @param condition The `mongory_value` representing the condition for this
 * matcher.
 * @return mongory_matcher* A pointer to the newly allocated base matcher, or
 * NULL on allocation failure.
 */
mongory_matcher *mongory_matcher_base_new(mongory_memory_pool *pool, mongory_value *condition);

/**
 * @brief Creates a new matcher that always evaluates to true.
 *
 * @param pool The memory pool for allocation.
 * @param condition The condition value (often unused by this matcher but stored
 * for consistency).
 * @return mongory_matcher* A pointer to the "always true" matcher, or NULL on
 * failure.
 */
mongory_matcher *mongory_matcher_always_true_new(mongory_memory_pool *pool, mongory_value *condition);

/**
 * @brief Creates a new matcher that always evaluates to false.
 *
 * @param pool The memory pool for allocation.
 * @param condition The condition value (often unused by this matcher but stored
 * for consistency).
 * @return mongory_matcher* A pointer to the "always false" matcher, or NULL on
 * failure.
 */
mongory_matcher *mongory_matcher_always_false_new(mongory_memory_pool *pool, mongory_value *condition);

/**
 * @brief Attempts to parse an integer from a string.
 *
 * This utility function is useful for matchers that might operate on array
 * indices or numeric string fields. It checks for valid integer formats and
 * range (`INT_MIN`, `INT_MAX`).
 *
 * @param key The null-terminated string to parse. Must not be NULL or empty.
 * @param out A pointer to an integer where the parsed value will be stored if
 * successful. Must not be NULL.
 * @return bool True if the string was successfully parsed as an integer and is
 * within `int` range, false otherwise. `errno` might be set by `strtol` on
 * failure (e.g. `ERANGE`).
 */
bool mongory_try_parse_int(const char *key, int *out);

/**
 * @brief Explains a matcher.
 * @param matcher The matcher to explain.
 * @param ctx The context to use for the explanation.
 */
void mongory_matcher_base_explain(mongory_matcher *matcher, mongory_matcher_explain_context *ctx);

#endif /* MONGORY_MATCHER_BASE_H */
