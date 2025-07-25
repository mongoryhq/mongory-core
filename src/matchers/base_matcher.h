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
mongory_matcher *
mongory_matcher_base_new(mongory_memory_pool *pool, mongory_value *condition);

/**
 * @brief Creates a new matcher that always evaluates to true.
 *
 * @param pool The memory pool for allocation.
 * @param condition The condition value (often unused by this matcher but stored
 * for consistency).
 * @return mongory_matcher* A pointer to the "always true" matcher, or NULL on
 * failure.
 */
mongory_matcher *mongory_matcher_always_true_new(mongory_memory_pool *pool,
                                                 mongory_value *condition);

/**
 * @brief Creates a new matcher that always evaluates to false.
 *
 * @param pool The memory pool for allocation.
 * @param condition The condition value (often unused by this matcher but stored
 * for consistency).
 * @return mongory_matcher* A pointer to the "always false" matcher, or NULL on
 * failure.
 */
mongory_matcher *mongory_matcher_always_false_new(mongory_memory_pool *pool,
                                                  mongory_value *condition);

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

#endif /* MONGORY_MATCHER_BASE_H */
