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
 * This is the primary public function for creating a matcher.
 * Currently, it delegates the creation to `mongory_matcher_table_cond_new`,
 * which is specialized for handling conditions structured as tables (similar
 * to JSON objects or MongoDB query documents). This implies that the most
 * common entry point for matching involves a table-like condition.
 *
 * @param pool The memory pool to be used for allocating the matcher and its
 * components.
 * @param condition A `mongory_value` that defines the criteria for the matcher.
 *                  This is often expected to be a `mongory_table`.
 * @return mongory_matcher* A pointer to the newly constructed matcher. Returns
 * NULL if allocation fails or the condition is invalid for
 * `mongory_matcher_table_cond_new`.
 */
mongory_matcher *mongory_matcher_new(mongory_memory_pool *pool, mongory_value *condition) {
  // Delegate to table_cond_new, assuming conditions are typically tables.
  mongory_matcher *matcher = mongory_matcher_table_cond_new(pool, condition);
  matcher->name = mongory_string_cpy(pool, "Intro");
  return matcher;
}

void mongory_matcher_explain(mongory_matcher *matcher, mongory_memory_pool *temp_pool) {
  mongory_matcher_explain_context ctx = {
      .pool = temp_pool,
      .count = 0,
      .total = 1,
      .prefix = "",
  };
  matcher->explain(matcher, &ctx);
}
