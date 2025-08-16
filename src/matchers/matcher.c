/**
 * @file matcher.c
 * @brief Implements the generic mongory_matcher constructor.
 *
 * This file provides the implementation for the top-level matcher creation
 * function.
 */
#include <stdio.h>
#include <string.h>
#include "mongory-core/matchers/matcher.h" // Public API

// Required internal headers for delegation
#include "../foundations/config_private.h" // Potentially for global settings
#include "base_matcher.h"                  // For mongory_matcher_base_new if used directly
#include "composite_matcher.h"             // For mongory_matcher_table_cond_new
#include "literal_matcher.h"               // Potentially for other default constructions
#include "mongory-core/foundations/array.h"
#include "../foundations/string_buffer.h"
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

typedef struct mongory_matcher_traced_match_context {
  char *message;
  int level;
} mongory_matcher_traced_match_context;

static bool mongory_matcher_traced_match(mongory_matcher *matcher, mongory_value *value) {
  bool matched = matcher->original_match(matcher, value);
  mongory_memory_pool *export_pool = value == NULL ? matcher->pool : value->pool;
  mongory_string_buffer *buffer = mongory_string_buffer_new(export_pool);
  char *res = matched ? "\e[30;42mMatched\e[0m" : "\e[30;41mDismatch\e[0m";
  char *condition = matcher->condition->to_str(matcher->condition, export_pool);
  char *record = value == NULL ? "Nothing" : value->to_str(value, export_pool);
  if (strcmp(matcher->name, "Field") == 0) {
    mongory_field_matcher *field_matcher = (mongory_field_matcher *)matcher;
    char *field_name = field_matcher->field;
    mongory_string_buffer_appendf(buffer, "%s: %s, field: \"%s\", condition: %s, record: %s\n", matcher->name, res, field_name, condition, record);
  } else {
    mongory_string_buffer_appendf(buffer, "%s: %s, condition: %s, record: %s\n", matcher->name, res, condition, record);
  }

  mongory_matcher_traced_match_context *trace_result = MG_ALLOC_PTR(export_pool, mongory_matcher_traced_match_context);
  trace_result->message = mongory_string_buffer_cstr(buffer);
  trace_result->level = matcher->trace_level;
  mongory_value *wrapped = mongory_value_wrap_ptr(export_pool, (void *)trace_result);
  matcher->trace_stack->push(matcher->trace_stack, wrapped);
  return matched;
}

static bool mongory_matcher_enable_trace(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  matcher->trace_stack = (mongory_array *)ctx->acc;
  matcher->trace_level = ctx->level;
  matcher->match = mongory_matcher_traced_match;
  return true;
}

static bool mongory_matcher_disable_trace(mongory_matcher *matcher, mongory_matcher_traverse_context *ctx) {
  (void)ctx;
  matcher->match = matcher->original_match;
  matcher->trace_stack = NULL;
  return true;
}

static mongory_array *mongory_matcher_traces_sort(mongory_array *self, int level) {
  mongory_array *sorted_array = mongory_array_new(self->pool);
  mongory_array *group = mongory_array_new(self->pool);
  int total = (int)self->count;
  for (int i = 0; i < total; i++) {
    mongory_value *item = self->get(self, i);
    mongory_matcher_traced_match_context *trace = (mongory_matcher_traced_match_context *)item->data.ptr;
    if (trace->level == level) {
      sorted_array->push(sorted_array, item);
      mongory_array *sorted_group = mongory_matcher_traces_sort(group, level + 1);
      int sorted_group_total = (int)sorted_group->count;
      for (int j = 0; j < sorted_group_total; j++) {
        sorted_array->push(sorted_array, sorted_group->get(sorted_group, j));
      }
      group = mongory_array_new(self->pool);
    } else {
      group->push(group, item);
    }
  }
  return sorted_array;
}

void mongory_matcher_trace(mongory_matcher *matcher, mongory_value *value) {
  mongory_array *trace_stack = mongory_array_new(value->pool);
  mongory_matcher_traverse_context ctx = {
      .pool = value->pool,
      .level = 0,
      .count = 0,
      .total = 0,
      .acc = (void *)trace_stack,
      .callback = mongory_matcher_enable_trace,
  };
  matcher->traverse(matcher, &ctx);
  matcher->match(matcher, value);
  ctx.callback = mongory_matcher_disable_trace;
  matcher->traverse(matcher, &ctx);
  mongory_array *sorted_trace_stack = mongory_matcher_traces_sort(trace_stack, 0);
  int total = (int)sorted_trace_stack->count;
  for (int i = 0; i < total; i++) {
    mongory_string_buffer *buffer = mongory_string_buffer_new(value->pool);
    mongory_value *item = sorted_trace_stack->get(sorted_trace_stack, i);
    mongory_matcher_traced_match_context *trace = (mongory_matcher_traced_match_context *)item->data.ptr;
    for (int k = 0; k < trace->level; k++) {
      mongory_string_buffer_append(buffer, "  ");
    }
    mongory_string_buffer_append(buffer, trace->message);
    printf("%s", mongory_string_buffer_cstr(buffer));
  }
}
