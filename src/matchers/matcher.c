/**
 * @file matcher.c
 * @brief Implements the generic mongory_matcher constructor.
 *
 * This file provides the implementation for the top-level matcher creation
 * function.
 */
#include "mongory-core/matchers/matcher.h" // Public API

// Required internal headers for delegation
#include "../foundations/config_private.h" // Potentially for global settings
#include "base_matcher.h"                  // For mongory_matcher_base_new if used directly
#include "composite_matcher.h"             // For mongory_matcher_table_cond_new
#include "literal_matcher.h"               // Potentially for other default constructions

#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include <mongory-core.h> // General include, might not be strictly necessary here

/**
 * @brief Creates a new matcher based on the provided condition.
 *
 * This is the primary public entry point for creating a matcher. The library
 * uses a factory pattern where this function determines the appropriate
 * specific matcher to create based on the structure of the `condition` value.
 *
 * Currently, it always delegates to `mongory_matcher_table_cond_new`,
 * which handles query documents (tables). This is the most common use case,
 * where the condition is a table like `{ "field": { "$op": "value" } }`.
 *
 * @param pool The memory pool to be used for allocating the matcher.
 * @param condition A `mongory_value` defining the matching criteria. This is
 *                  typically a `mongory_table`.
 * @return mongory_matcher* A pointer to the newly constructed matcher, or NULL
 * if allocation fails or the condition is invalid.
 */
mongory_matcher *mongory_matcher_new(mongory_memory_pool *pool, mongory_value *condition) {
  // The core logic is delegated to a more specific constructor.
  // This design allows for easy extension; for example, a different constructor
  // could be chosen here based on the `condition->type`.
  mongory_matcher *matcher = mongory_matcher_table_cond_new(pool, condition);
  if (matcher == NULL) {
    return NULL;
  }

  return matcher;
}

/**
 * @brief Executes the matching logic for the given matcher.
 *
 * This function is a polymorphic wrapper. It invokes the `match` function
 * pointer on the specific `mongory_matcher` instance, which will be one of
 * the internal matching functions (e.g., from a compare_matcher or
 * composite_matcher).
 *
 * @param matcher The matcher to use.
 * @param value The value to check against the matcher's condition.
 * @return True if the value satisfies the matcher's condition, false otherwise.
 */
bool mongory_matcher_match(mongory_matcher *matcher, mongory_value *value) { return matcher->match(matcher, value); }

/**
 * @brief Generates a human-readable explanation of the matcher's criteria.
 *
 * This function is a polymorphic wrapper around the `explain` function pointer,
 * allowing different matcher types to provide their own specific explanations.
 *
 * @param matcher The matcher to explain.
 * @param temp_pool A temporary memory pool for allocating the explanation string(s).
 */
void mongory_matcher_explain(mongory_matcher *matcher, mongory_memory_pool *temp_pool) {
  mongory_matcher_explain_context ctx = {
      .pool = temp_pool,
      .count = 0,
      .total = 0,
      .prefix = "",
  };
  matcher->explain(matcher, &ctx);
}
