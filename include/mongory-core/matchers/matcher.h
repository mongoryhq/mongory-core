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
 * @brief Function pointer type for a matcher's core matching logic.
 *
 * @param matcher A pointer to the `mongory_matcher` instance itself.
 * @param value A pointer to the `mongory_value` to be evaluated against the
 * matcher's condition.
 * @return bool True if the `value` matches the condition, false otherwise.
 */
typedef bool (*mongory_matcher_match_func)(mongory_matcher *matcher,
                                           mongory_value *value);

/**
 * @struct mongory_matcher_context
 * @brief Context associated with a matcher instance.
 *
 * This can store the original match function (useful if the `match` function
 * pointer in `mongory_matcher` is dynamically changed, e.g., for decorators)
 * and a trace array for debugging purposes (currently not extensively used).
 */
typedef struct mongory_matcher_context {
  mongory_matcher_match_func
      original_match; /**< Stores the original match function, potentially for
                         restoration or delegation. */
  mongory_array *trace; /**< An array that can be used for tracing matcher
                           execution (for debugging). */
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
  char *name;                 /**< Optional name for the matcher (e.g., "$eq").
                                 String is typically allocated from the pool. */
  mongory_value *condition;   /**< The condition (a `mongory_value`) that this
                                 matcher evaluates against. */
  mongory_matcher_match_func match; /**< Function pointer to the specific matching
                                       logic for this matcher type. */
  mongory_memory_pool *pool;  /**< The memory pool used for allocations related
                                 to this matcher instance. */
  mongory_matcher_context context; /**< Additional context for the matcher, like
                                      tracing or original function pointers. */
};

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
mongory_matcher *
mongory_matcher_new(mongory_memory_pool *pool, mongory_value *condition);

#endif /* MONGORY_MATCHER_H */
