/**
 * @file literal_matcher.c
 * @brief Implements literal matchers, field matchers, $not, and $size.
 * This is an internal implementation file for the matcher module.
 *
 * - "Literal" matching often involves comparing a value directly or using a
 *   simple condition (like equality).
 * - Field matchers extract a value from a table/array by field name/index and
 *   apply a condition to it.
 * - $not inverts the result of a condition.
 * - $size checks the number of elements in an array against a condition.
 */
#include "literal_matcher.h"
#include "../foundations/config_private.h" // For mongory_internal_value_converter, mongory_string_cpy
#include "array_record_matcher.h" // For handling array-specific matching logic
#include "base_matcher.h"     // For mongory_try_parse_int, mongory_matcher_base_new
#include "compare_matcher.h"  // For mongory_matcher_equal_new
#include "composite_matcher.h" // For mongory_matcher_composite_new, mongory_matcher_table_cond_new
#include "existance_matcher.h" // For mongory_matcher_exists_new (used by null_new)
#include "mongory-core/foundations/array.h" // For mongory_array access
#include "mongory-core/foundations/table.h" // For mongory_table access
#include "mongory-core/foundations/value.h" // For mongory_value types and wrappers
#include "regex_matcher.h"    // For mongory_matcher_regex_new
#include <mongory-core.h>     // General include

/**
 * @brief Core matching logic for literal-based conditions.
 *
 * This function is used by field matchers, $not, and $size.
 * It checks the type of the input `value`.
 * - If `value` is an array, it may delegate to an `array_record_matcher`
 *   (via `composite->right`, if initialized). This path seems intended for
 *   more complex array matching scenarios where the condition itself might
 *   describe how array elements should be matched.
 * - Otherwise (if `value` is not an array, or if array-specific handling is
 *   not triggered), it uses the `composite->left` matcher. `composite->left`
 *   is typically set up by `mongory_matcher_literal_delegate` to handle the
 *   specific type of the literal condition (e.g., equality for simple values,
 *   regex matcher for regex conditions, table condition matcher for table
 *   conditions).
 *
 * @param matcher The `mongory_matcher` (expected to be a
 * `mongory_composite_matcher` where `left` and possibly `right` are set up).
 * @param value The `mongory_value` to evaluate against the matcher's condition.
 * @return True if the value matches, false otherwise.
 */
static inline bool mongory_matcher_literal_match(mongory_matcher *matcher,
                                                 mongory_value *value) {
  mongory_composite_matcher *composite = (mongory_composite_matcher *)matcher;
  if (!composite || !composite->base.pool) return false; // Basic safety check

  if (value != NULL && value->type == MONGORY_TYPE_ARRAY) {
    // If the value being matched is an array, there's special handling.
    // The `right` child of the composite matcher is used for array record matching.
    // It's created on-demand if not already present.
    if (composite->right == NULL) {
      // Lazily create the array_record_matcher if needed.
      // The condition for array_record_new is the original condition of the literal matcher.
      composite->right = mongory_matcher_array_record_new(
          composite->base.pool, composite->base.condition);
    }
    // If right child exists (or was successfully created), use it.
    return composite->right ? composite->right->match(composite->right, value)
                            : false;
  } else {
    // For non-array values, or if array-specific path wasn't taken.
    // The `left` child handles the general literal condition.
    return composite->left ? composite->left->match(composite->left, value)
                           : false;
  }
}

/**
 * @brief Internal helper to create a specialized matcher for a `MONGORY_TYPE_NULL`
 * condition.
 *
 * This creates an OR condition:
 * (`value` IS MONGORY_TYPE_NULL) OR (`value` DOES NOT EXIST (evaluates to
 * false for $exists:true)). This means it matches actual BSON nulls or missing
 * fields.
 *
 * @param pool Memory pool for allocation.
 * @param condition The `mongory_value` which is of `MONGORY_TYPE_NULL`.
 * @return A composite matcher for the NULL condition, or NULL on failure.
 */
static inline mongory_matcher *
mongory_matcher_null_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_composite_matcher *composite =
      mongory_matcher_composite_new(pool, condition);
  if (!composite) return NULL;

  // Left branch: checks for actual MONGORY_TYPE_NULL
  mongory_value *null_val = mongory_value_wrap_n(pool, NULL);
  if (!null_val) return NULL; // Failed to wrap null value
  composite->left = mongory_matcher_equal_new(pool, null_val);

  // Right branch: checks if the field does not exist (value is NULL from get)
  // $exists: false
  mongory_value *exists_false_cond = mongory_value_wrap_b(pool, false);
  if (!exists_false_cond) return NULL; // Failed to wrap bool
  composite->right = mongory_matcher_exists_new(pool, exists_false_cond);

  if (!composite->left || !composite->right) {
      // Cleanup if one part failed? Pool should handle.
      return NULL;
  }

  composite->base.match = mongory_matcher_or_match; // OR logic
  composite->base.context.original_match = mongory_matcher_or_match;
  // composite->base.condition is already set by mongory_matcher_composite_new
  return (mongory_matcher *)composite;
}

/**
 * @brief Delegates creation of a simple literal matcher based on the
 * `condition`'s type.
 *
 * - Table condition: creates a `mongory_matcher_table_cond_new`.
 * - Regex condition: creates a `mongory_matcher_regex_new`.
 * - Null condition: creates a specialized `mongory_matcher_null_new`.
 * - Other types: creates an equality matcher (`mongory_matcher_equal_new`).
 *
 * @param pool Memory pool for allocation.
 * @param condition The literal value or condition table.
 * @return The appropriate simple matcher for the condition, or NULL on failure.
 */
static inline mongory_matcher *
mongory_matcher_literal_delegate(mongory_memory_pool *pool,
                                 mongory_value *condition) {
  if (!condition) return mongory_matcher_equal_new(pool, NULL); // Or specific null matcher

  switch (condition->type) {
  case MONGORY_TYPE_TABLE:
    return mongory_matcher_table_cond_new(pool, condition);
  case MONGORY_TYPE_REGEX:
    return mongory_matcher_regex_new(pool, condition);
  case MONGORY_TYPE_NULL:
    // When the condition is explicitly `null`, e.g. `{ field: null }`
    return mongory_matcher_null_new(pool, condition);
  default:
    // For boolean, int, double, string, array (equality), pointer, unsupported.
    return mongory_matcher_equal_new(pool, condition);
  }
}

/**
 * @struct mongory_field_matcher
 * @brief Specialized composite matcher for matching a specific field.
 * Stores the field name/index.
 */
typedef struct mongory_field_matcher {
  mongory_composite_matcher composite; /**< Base composite matcher structure. */
  char *field; /**< Name/index of the field to match. Copied string. */
} mongory_field_matcher;

/**
 * @brief Match function for a field matcher.
 *
 * Extracts the value of `field_matcher->field` from the input `value`
 * (table or array). Then, applies the literal matching logic
 * (`mongory_matcher_literal_match`) using the sub-matcher stored in
 * `composite.left` (which was set up by `literal_delegate` based on the
 * original condition for this field).
 *
 * @param matcher Pointer to the `mongory_matcher` (a `mongory_field_matcher`).
 * @param value The input table or array to extract the field from.
 * @return True if the field's value matches the condition, false otherwise.
 */
static inline bool mongory_matcher_field_match(mongory_matcher *matcher,
                                               mongory_value *value) {
  if (value == NULL) { // Cannot extract field from NULL.
    return false;
  }
  mongory_field_matcher *field_matcher = (mongory_field_matcher *)matcher;
  if (!field_matcher->field) return false; // No field specified.

  mongory_value *field_value = NULL;
  char *field_key = field_matcher->field;

  if (value->type == MONGORY_TYPE_TABLE) {
    if (value->data.t) {
      field_value = value->data.t->get(value->data.t, field_key);
    }
  } else if (value->type == MONGORY_TYPE_ARRAY) {
    if (value->data.a) {
      int index;
      if (!mongory_try_parse_int(field_key, &index)) {
        return false; // Field key is not a valid integer index for an array.
      }
      if (index < 0) { // Handle negative indexing (from end of array)
        if ( (size_t)(-index) > value->data.a->count) return false; // Out of bounds
        index = value->data.a->count + index;
      }
      if ( (size_t)index >= value->data.a->count) return false; // Out of bounds
      field_value = value->data.a->get(value->data.a, (size_t)index);
    }
  } else {
    return false; // Can only extract fields from tables or arrays.
  }

  // If the extracted field value is a pointer type that needs conversion
  // (e.g., from a language binding), convert it.
  if (field_value && field_value->type == MONGORY_TYPE_POINTER &&
      mongory_internal_value_converter &&
      mongory_internal_value_converter->shallow_convert) {
    // The pool for the converted value should ideally be the field_value's pool
    // or the matcher's pool.
    mongory_memory_pool* conversion_pool = field_value->pool ? field_value->pool : matcher->pool;
    field_value = mongory_internal_value_converter->shallow_convert(
        conversion_pool, field_value->data.ptr);
  }
  // Now, use the literal_match logic (which uses composite.left primarily)
  // to match the extracted field_value.
  return mongory_matcher_literal_match(matcher, field_value);
}

mongory_matcher *mongory_matcher_field_new(mongory_memory_pool *pool,
                                           char *field_name,
                                           mongory_value *condition_for_field) {
  if(!pool || !pool->alloc || !field_name) return NULL;

  mongory_field_matcher *field_m =
      pool->alloc(pool->ctx, sizeof(mongory_field_matcher));
  if (field_m == NULL) {
    return NULL;
  }
  field_m->field = mongory_string_cpy(pool, field_name);
  if (field_m->field == NULL) {
      // pool->free(field_m) is not standard; pool manages its own memory.
      // This indicates an error in string copy, likely pool allocation.
      return NULL;
  }

  // Initialize the base composite matcher part
  field_m->composite.base.pool = pool;
  field_m->composite.base.name = NULL; // Can be set if needed, e.g. to field_name
  field_m->composite.base.match = mongory_matcher_field_match;
  field_m->composite.base.context.original_match = mongory_matcher_field_match;
  field_m->composite.base.context.trace = NULL;
  field_m->composite.base.condition = condition_for_field; // Original condition for the field

  // The 'left' child of the composite is the actual matcher for the field's value,
  // determined by the type of 'condition_for_field'.
  field_m->composite.left =
      mongory_matcher_literal_delegate(pool, condition_for_field);
  field_m->composite.right = NULL; // Not typically used by field_match directly,
                                 // but literal_match might use it for arrays.

  if (field_m->composite.left == NULL) {
      // Failed to create the delegate matcher for the condition.
      return NULL;
  }

  return (mongory_matcher *)field_m;
}

/**
 * @brief Match function for a NOT matcher.
 * Negates the result of `mongory_matcher_literal_match`.
 * @param matcher The $not matcher.
 * @param value The value to evaluate.
 * @return True if the literal match is false, false if it's true.
 */
static inline bool mongory_matcher_not_match(mongory_matcher *matcher,
                                             mongory_value *value) {
  // literal_match uses composite.left (and sometimes .right for arrays)
  return !mongory_matcher_literal_match(matcher, value);
}

mongory_matcher *mongory_matcher_not_new(mongory_memory_pool *pool,
                                         mongory_value *condition_to_negate) {
  if (!pool) return NULL;
  mongory_composite_matcher *composite =
      mongory_matcher_composite_new(pool, condition_to_negate);
  if (!composite) return NULL;

  // The 'left' child is the matcher for the condition being negated.
  composite->left = mongory_matcher_literal_delegate(pool, condition_to_negate);
  if (!composite->left) {
      return NULL; // Failed to create delegate for the condition.
  }
  // composite->right remains NULL for $not, as literal_match's array path
  // via composite->right will use condition_to_negate if right is NULL.

  composite->base.match = mongory_matcher_not_match;
  composite->base.context.original_match = mongory_matcher_not_match;
  return (mongory_matcher *)composite;
}

/**
 * @brief Match function for a $size matcher.
 * Checks if the input `value` (must be an array) has a size that matches
 * the condition associated with the $size matcher.
 * @param matcher The $size matcher.
 * @param value The value to evaluate (must be an array).
 * @return True if array size matches condition, false otherwise.
 */
static inline bool mongory_matcher_size_match(mongory_matcher *matcher,
                                              mongory_value *value) {
  if (!value || value->type != MONGORY_TYPE_ARRAY || !value->data.a) {
    return false; // $size only applies to valid arrays.
  }
  mongory_array *array = value->data.a;
  // Wrap the array's count as a mongory_value (integer) to be matched.
  // The pool for this temporary size value should be from the input value or matcher.
  mongory_memory_pool* temp_val_pool = value->pool ? value->pool : matcher->pool;
  if (!temp_val_pool) return false; // Cannot create temp value without a pool

  mongory_value *size_value = mongory_value_wrap_i(temp_val_pool, array->count);
  if (!size_value) return false; // Failed to wrap size.

  // Use literal_match with the matcher's original condition against the size_value.
  // The $size matcher's `composite.left` was set up by literal_delegate
  // based on the condition provided to $size (e.g., if {$size: 5}, left is an
  // equality matcher for 5).
  bool result = mongory_matcher_literal_match(matcher, size_value);
  // Note: size_value is temporary and allocated from a pool. It will be cleaned
  // up when that pool is freed. No manual free here.
  return result;
}

mongory_matcher *mongory_matcher_size_new(mongory_memory_pool *pool,
                                          mongory_value *size_condition) {
  if (!pool) return NULL;
  mongory_composite_matcher *composite =
      mongory_matcher_composite_new(pool, size_condition);
  if (!composite) return NULL;

  // The 'left' child is the matcher for the size_condition itself.
  // E.g., if {$size: {$gt: 5}}, size_condition is {$gt: 5}, and
  // composite.left becomes a "greater than 5" matcher.
  composite->left = mongory_matcher_literal_delegate(pool, size_condition);
  if (!composite->left) {
      return NULL;
  }
  // composite->right typically NULL for $size, array path of literal_match not primary.

  composite->base.match = mongory_matcher_size_match;
  composite->base.context.original_match = mongory_matcher_size_match;
  return (mongory_matcher *)composite;
}

/**
 * @brief Creates a "literal" matcher (constructor for `mongory_matcher_literal_new`).
 *
 * This appears to be an internal helper or a way to directly use the
 * `mongory_matcher_literal_match` logic. It sets up a composite matcher
 * where the `left` child is determined by `mongory_matcher_literal_delegate`
 * applied to the `condition`. The `right` child is initially NULL but can be
 * lazily initialized by `mongory_matcher_literal_match` if the value being
 * matched is an array.
 *
 * @param pool Memory pool for allocation.
 * @param condition The literal `mongory_value` or condition table.
 * @return A new literal matcher, or NULL on failure.
 */
mongory_matcher *mongory_matcher_literal_new(mongory_memory_pool *pool,
                                             mongory_value *condition) {
  if (!pool) return NULL;
  mongory_composite_matcher *composite =
      mongory_matcher_composite_new(pool, condition);
  if (!composite) return NULL;

  composite->left = mongory_matcher_literal_delegate(pool, condition);
  if (!composite->left) {
    return NULL;
  }
  // composite->right is NULL initially. mongory_matcher_literal_match will
  // populate it with an array_record_matcher if it encounters an array value.
  composite->base.match = mongory_matcher_literal_match;
  composite->base.context.original_match = mongory_matcher_literal_match;
  return (mongory_matcher *)composite;
}
