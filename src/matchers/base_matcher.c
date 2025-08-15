/**
 * @file base_matcher.c
 * @brief Implements the base matcher constructor and related utility functions.
 * This is an internal implementation file for the matcher module.
 */
#include "base_matcher.h"
#include "../foundations/string_buffer.h"
#include <errno.h>        // For errno, ERANGE
#include <limits.h>       // For INT_MIN, INT_MAX
#include <mongory-core.h> // General include, for mongory_matcher types
#include <stdbool.h>
#include <stdio.h>  // For printf
#include <stdlib.h> // For strtol
#include "matcher_explainable.h"

/**
 * @brief Allocates and initializes common fields of a `mongory_matcher`.
 *
 * This function serves as a common initializer for all specific matcher types.
 * It allocates memory for the `mongory_matcher` structure itself from the
 * provided `pool`, sets the `pool` and `condition` members.
 * The `matcher->match` function pointer specific to the matcher type must be
 * set by the caller. `original_match` and `trace` in the context are set to
 * NULL.
 *
 * @param pool The memory pool to use for allocation. Must be non-NULL and
 * valid.
 * @param condition The condition value for this matcher.
 * @return mongory_matcher* Pointer to the newly allocated and partially
 * initialized matcher, or NULL if allocation fails.
 */
mongory_matcher *mongory_matcher_base_new(mongory_memory_pool *pool, mongory_value *condition) {
  if (!pool || !pool->alloc) {
    return NULL; // Invalid memory pool.
  }
  mongory_matcher *matcher = MG_ALLOC_PTR(pool, mongory_matcher);
  if (matcher == NULL) {
    // Allocation failed, pool->alloc might set pool->error.
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }

  // Initialize common fields
  matcher->original_match = NULL;
  matcher->sub_count = 0;
  matcher->external_matcher = NULL;
  matcher->pool = pool;
  matcher->condition = condition;
  matcher->name = NULL;                            // Name is not set by base_new.
  matcher->match = NULL;                           // Specific match function must be set by derived type.
  matcher->explain = mongory_matcher_base_explain; // Specific explain function must be set by derived type.

  return matcher;
}

/**
 * @brief The match function for a matcher that always returns true.
 * @param matcher Unused.
 * @param value Unused.
 * @return Always true.
 */
static inline bool mongory_matcher_always_true_match(mongory_matcher *matcher, mongory_value *value) {
  (void)matcher; // Mark as unused to prevent compiler warnings.
  (void)value;   // Mark as unused.
  return true;   // This matcher always indicates a match.
}

/**
 * @brief Creates a matcher instance that will always evaluate to true.
 * Useful as a placeholder or for default cases.
 * @param pool Memory pool for allocation.
 * @param condition Condition (typically ignored by this matcher).
 * @return A new `mongory_matcher` or NULL on failure.
 */
mongory_matcher *mongory_matcher_always_true_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (!matcher) {
    return NULL;
  }
  matcher->match = mongory_matcher_always_true_match;
  matcher->name = mongory_string_cpy(pool, "Always True");
  matcher->explain = mongory_matcher_base_explain;
  // Optionally set original_match as well if it's a strict policy
  // matcher->context.original_match = mongory_matcher_always_true_match;
  return matcher;
}

/**
 * @brief The match function for a matcher that always returns false.
 * @param matcher Unused.
 * @param value Unused.
 * @return Always false.
 */
static inline bool mongory_matcher_always_false_match(mongory_matcher *matcher, mongory_value *value) {
  (void)matcher; // Mark as unused.
  (void)value;   // Mark as unused.
  return false;  // This matcher never indicates a match.
}

/**
 * @brief Creates a matcher instance that will always evaluate to false.
 * Useful for conditions that should never match.
 * @param pool Memory pool for allocation.
 * @param condition Condition (typically ignored by this matcher).
 * @return A new `mongory_matcher` or NULL on failure.
 */
mongory_matcher *mongory_matcher_always_false_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (!matcher) {
    return NULL;
  }
  matcher->match = mongory_matcher_always_false_match;
  matcher->name = mongory_string_cpy(pool, "Always False");
  matcher->explain = mongory_matcher_base_explain;
  // matcher->context.original_match = mongory_matcher_always_false_match;
  return matcher;
}

/**
 * @brief Attempts to parse a string `key` into an integer `out`.
 *
 * Uses `strtol` for conversion and checks for common parsing errors:
 * - Input string is NULL or empty.
 * - The string contains non-numeric characters after the number.
 * - The parsed number is out of the range of `int` (`INT_MIN`, `INT_MAX`).
 *
 * @param key The null-terminated string to parse.
 * @param out Pointer to an integer where the result is stored.
 * @return `true` if parsing is successful and the value fits in an `int`.
 *         `false` otherwise. `errno` may be set by `strtol`.
 */
bool mongory_try_parse_int(const char *key, int *out) {
  if (key == NULL || *key == '\0') {
    return false; // Invalid input string.
  }
  if (out == NULL) {
    return false; // Output pointer must be valid.
  }

  char *endptr = NULL;
  errno = 0;                           // Clear errno before calling strtol.
  long val = strtol(key, &endptr, 10); // Base 10 conversion.

  // Check for parsing errors reported by strtol.
  if (endptr == key) {
    return false; // No digits were found.
  }
  if (*endptr != '\0') {
    return false; // Additional characters after the number.
  }
  if (errno == ERANGE || val < INT_MIN || val > INT_MAX) {
    // Value out of range for int. ERANGE is set by strtol for overflow/underflow.
    return false;
  }

  *out = (int)val; // Successfully parsed and within int range.
  return true;
}
