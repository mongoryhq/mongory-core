/**
 * @file value.c
 * @brief Implements the mongory_value generic value type and its operations.
 *
 * This file provides functions for creating (`wrap`), comparing, and
 * converting `mongory_value` instances to strings. Each data type supported by
 * `mongory_value` has a corresponding comparison function and wrapping
 * function.
 */
#include <mongory-core/foundations/array.h>
#include <mongory-core/foundations/config.h> // For mongory_string_cpy
#include <mongory-core/foundations/memory_pool.h>
#include <mongory-core/foundations/table.h>
#include <mongory-core/foundations/value.h>
#include <stddef.h> // For NULL
#include <stdio.h>  // For snprintf
#include <stdlib.h> // For general utilities (not directly used here but common)
#include <string.h> // For strcmp

/**
 * @brief Returns a string literal representing the type of the mongory_value.
 * @param value The mongory_value to inspect.
 * @return A string like "Int", "String", "Array", etc., or "UnknownType".
 */
char *mongory_type_to_string(mongory_value *value) {
  if (!value) return "NullValuePtr"; // Handle null pointer to value itself
  switch (value->type) {
// Use X-Macro to generate cases for each defined type.
#define CASE_GEN(name, num, str, field)                                        \
  case name:                                                                   \
    return str;
    MONGORY_TYPE_MACRO(CASE_GEN)
#undef CASE_GEN
  default:
    return "UnknownType"; // Fallback for unrecognized types.
  }
}

/**
 * @brief Converts a mongory_value to a string representation of its data.
 * For simple types, this formats the data (e.g., integer to string).
 * For complex types (Array, Table), it returns a generic placeholder string.
 * The returned string is allocated from the value's associated memory pool.
 * @param value The mongory_value to convert.
 * @return A string representation, or a placeholder/error string.
 *         The caller should not free this string if it's pool-allocated.
 */
char *mongory_value_to_string(mongory_value *value) {
  if (!value) return "NullValuePtr (toString)";
  if (!value->pool) return "ValueHasNoPool (toString)"; // Cannot allocate buffer

  char *buffer = NULL;
  // Max buffer size for simple types; adjust if necessary.
  // For strings, it will be "<string>", so length + quotes + null.
  // For numbers, 32 chars should be ample.
  const int temp_buffer_size = 64;

  switch (value->type) {
  case MONGORY_TYPE_NULL:
    // Return a string literal for NULL, no allocation needed from pool.
    // Or, if pool allocation is desired for consistency:
    // buffer = value->pool->alloc(value->pool->ctx, strlen("NULL") + 1);
    // if (buffer) strcpy(buffer, "NULL"); else return "AllocError";
    // return buffer;
    return "NULL"; // Simpler to return literal for this case.
  case MONGORY_TYPE_BOOL:
    buffer = value->pool->alloc(value->pool->ctx, strlen("true") + 1); // "false" is longer
    if (!buffer) return "AllocError";
    snprintf(buffer, strlen("true") +1 , "%s", value->data.b ? "true" : "false");
    return buffer;
  case MONGORY_TYPE_INT:
    buffer = value->pool->alloc(value->pool->ctx, temp_buffer_size);
    if (!buffer) return "AllocError";
    snprintf(buffer, temp_buffer_size, "%lld", (long long)value->data.i);
    return buffer;
  case MONGORY_TYPE_DOUBLE:
    buffer = value->pool->alloc(value->pool->ctx, temp_buffer_size);
    if (!buffer) return "AllocError";
    snprintf(buffer, temp_buffer_size, "%f", value->data.d);
    return buffer;
  case MONGORY_TYPE_STRING:
    if (value->data.s == NULL) return "\"\""; // Or "NullString"
    // Allocate enough for quotes, string content, and null terminator.
    buffer = value->pool->alloc(value->pool->ctx, strlen(value->data.s) + 3);
    if (!buffer) return "AllocError";
    snprintf(buffer, strlen(value->data.s) + 3, "\"%s\"", value->data.s);
    return buffer;
  // For complex types, return generic placeholders.
  // Detailed stringification would require recursive processing.
  case MONGORY_TYPE_ARRAY:
    return "ArrayValue"; // Placeholder
  case MONGORY_TYPE_TABLE:
    return "TableValue"; // Placeholder
  case MONGORY_TYPE_REGEX:
    return "RegexValue"; // Placeholder
  case MONGORY_TYPE_POINTER:
    return "PointerValue"; // Placeholder
  case MONGORY_TYPE_UNSUPPORTED:
    return "UnsupportedValue"; // Placeholder
  default:
    return "UnknownType (toString)";
  }
}

/**
 * @brief Extracts a void pointer to the raw data within the mongory_value.
 * The caller must know the actual type to cast this pointer correctly.
 * @param value The mongory_value from which to extract data.
 * @return A void pointer to the data, or NULL for unknown types or MONGORY_TYPE_NULL.
 */
void *mongory_value_extract(mongory_value *value) {
  if (!value) return NULL;
  switch (value->type) {
// Use X-Macro to generate cases. For MONGORY_TYPE_NULL, &value->data.i is
// returned but is meaningless as data for NULL.
#define EXTRACT_CASE(name, num, str, field)                                    \
  case name:                                                                   \
    return (void *)&value->data.field;
    MONGORY_TYPE_MACRO(EXTRACT_CASE)
#undef EXTRACT_CASE
  default:
    return NULL; // Unknown type.
  }
}

/**
 * @brief Internal helper to allocate a new mongory_value structure from a pool.
 * @param pool The memory pool to allocate from.
 * @return A pointer to the new mongory_value, or NULL on allocation failure.
 */
static inline mongory_value *mongory_value_new(mongory_memory_pool *pool) {
  if (!pool || !pool->alloc) return NULL; // Invalid pool.
  mongory_value *value = pool->alloc(pool->ctx, sizeof(mongory_value));
  if (!value) {
    // TODO: pool->error could be set by pool->alloc itself.
    return NULL;
  }
  value->pool = pool;
  value->origin = NULL; // Default origin to NULL.
  return value;
}

// --- Comparison Functions ---

/** Compares two MONGORY_TYPE_NULL values. Always equal. */
static inline int mongory_value_null_compare(mongory_value *a,
                                             mongory_value *b) {
  (void)a; // 'a' is implicitly MONGORY_TYPE_NULL by context of call.
  if (b->type != MONGORY_TYPE_NULL) {
    return mongory_value_compare_fail; // Cannot compare NULL with non-NULL.
  }
  return 0; // NULL is equal to NULL.
}

/** Wraps a NULL value. The `n` parameter is ignored. */
mongory_value *mongory_value_wrap_n(mongory_memory_pool *pool, void *n) {
  (void)n; // Parameter 'n' is unused for wrapping NULL.
  mongory_value *value = mongory_value_new(pool);
  if (!value) return NULL;
  value->type = MONGORY_TYPE_NULL;
  value->comp = mongory_value_null_compare;
  // data union is not explicitly set for NULL.
  return value;
}

/** Compares two MONGORY_TYPE_BOOL values. */
static inline int mongory_value_bool_compare(mongory_value *a,
                                             mongory_value *b) {
  if (b->type != MONGORY_TYPE_BOOL) return mongory_value_compare_fail;
  // Standard comparison: (true > false)
  return (a->data.b > b->data.b) - (a->data.b < b->data.b);
}

/** Wraps a boolean value. */
mongory_value *mongory_value_wrap_b(mongory_memory_pool *pool, bool b_val) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) return NULL;
  value->type = MONGORY_TYPE_BOOL;
  value->data.b = b_val;
  value->comp = mongory_value_bool_compare;
  return value;
}

/** Compares MONGORY_TYPE_INT with INT or DOUBLE. */
static inline int mongory_value_int_compare(mongory_value *a,
                                            mongory_value *b) {
  if (b->type == MONGORY_TYPE_DOUBLE) {
    // Compare int (a) as double with double (b).
    double a_as_double = (double)a->data.i;
    double b_value = b->data.d;
    return (a_as_double > b_value) - (a_as_double < b_value);
  }
  if (b->type == MONGORY_TYPE_INT) {
    // Compare int (a) with int (b).
    int64_t a_value = a->data.i;
    int64_t b_value = b->data.i;
    return (a_value > b_value) - (a_value < b_value);
  }
  return mongory_value_compare_fail; // Incompatible type for comparison.
}

/** Wraps an integer (promoted to int64_t). */
mongory_value *mongory_value_wrap_i(mongory_memory_pool *pool, int i_val) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) return NULL;
  value->type = MONGORY_TYPE_INT;
  value->data.i = (int64_t)i_val; // Store as int64_t.
  value->comp = mongory_value_int_compare;
  return value;
}

/** Compares MONGORY_TYPE_DOUBLE with DOUBLE or INT. */
static inline int mongory_value_double_compare(mongory_value *a,
                                               mongory_value *b) {
  if (b->type == MONGORY_TYPE_DOUBLE) {
    double a_value = a->data.d;
    double b_value = b->data.d;
    return (a_value > b_value) - (a_value < b_value);
  }
  if (b->type == MONGORY_TYPE_INT) {
    // Compare double (a) with int (b) as double.
    double a_value = a->data.d;
    double b_as_double = (double)b->data.i;
    return (a_value > b_as_double) - (a_value < b_as_double);
  }
  return mongory_value_compare_fail; // Incompatible type.
}

/** Wraps a double value. */
mongory_value *mongory_value_wrap_d(mongory_memory_pool *pool, double d_val) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) return NULL;
  value->type = MONGORY_TYPE_DOUBLE;
  value->data.d = d_val;
  value->comp = mongory_value_double_compare;
  return value;
}

/** Compares two MONGORY_TYPE_STRING values lexicographically. */
static inline int mongory_value_string_compare(mongory_value *a,
                                               mongory_value *b) {
  // Ensure both are strings and not NULL pointers.
  if (b->type != MONGORY_TYPE_STRING || a->data.s == NULL ||
      b->data.s == NULL) {
    // If one is NULL string and other is not, how to compare?
    // For now, strict: both must be valid strings.
    // Or, define NULL string < non-NULL string.
    // Current: fail if not string or if actual char* is NULL.
    return mongory_value_compare_fail;
  }
  int cmp_result = strcmp(a->data.s, b->data.s);
  return (cmp_result > 0) - (cmp_result < 0); // Normalize to -1, 0, 1
}

/** Wraps a string. Makes a copy of the string using the pool. */
mongory_value *mongory_value_wrap_s(mongory_memory_pool *pool, char *s_val) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) return NULL;
  value->type = MONGORY_TYPE_STRING;
  // mongory_string_cpy handles NULL s_val and allocation.
  value->data.s = mongory_string_cpy(pool, s_val);
  if (s_val != NULL && value->data.s == NULL) {
      // String copy failed (likely pool allocation error).
      // `value` itself is allocated, but its data is incomplete.
      // Depending on pool strategy, `value` might need to be "unallocated"
      // or this error is indicated by `pool->error`.
      // For now, we return the value, but its string is NULL.
      // This might be problematic. A robust solution would be to free `value` if
      // `mongory_string_cpy` fails AND `s_val` was not NULL.
      // However, `mongory_value_new` doesn't offer a paired free.
      // So, the string field remains NULL.
  }
  value->comp = mongory_value_string_compare;
  return value;
}

/** Compares two MONGORY_TYPE_ARRAY values element by element. */
static inline int mongory_value_array_compare(mongory_value *a,
                                              mongory_value *b) {
  if (b->type != MONGORY_TYPE_ARRAY || a->data.a == NULL || b->data.a == NULL) {
    return mongory_value_compare_fail; // Must be two valid arrays.
  }
  struct mongory_array *array_a = a->data.a;
  struct mongory_array *array_b = b->data.a;

  // Different counts: shorter array is "less".
  if (array_a->count != array_b->count) {
    return (array_a->count > array_b->count) -
           (array_a->count < array_b->count);
  }

  // Same count, compare element by element.
  for (size_t i = 0; i < array_a->count; i++) {
    mongory_value *item_a = array_a->get(array_a, i);
    mongory_value *item_b = array_b->get(array_b, i);

    // Handle NULL elements within arrays carefully.
    bool a_item_is_null = (item_a == NULL || item_a->type == MONGORY_TYPE_NULL);
    bool b_item_is_null = (item_b == NULL || item_b->type == MONGORY_TYPE_NULL);

    if (a_item_is_null && b_item_is_null) continue; // Both null, considered equal here.
    if (a_item_is_null) return -1; // Null is less than non-null.
    if (b_item_is_null) return 1;  // Non-null is greater than null.

    // Both items are non-null, compare them using their own comp functions.
    if (!item_a->comp || !item_b->comp) return mongory_value_compare_fail; // Should not happen for valid values

    int cmp_result = item_a->comp(item_a, item_b);
    if (cmp_result == mongory_value_compare_fail) return mongory_value_compare_fail; // Incomparable elements
    if (cmp_result != 0) {
      return cmp_result; // First differing element determines array order.
    }
  }
  return 0; // All elements are equal.
}

/** Wraps a mongory_array. */
mongory_value *mongory_value_wrap_a(mongory_memory_pool *pool,
                                    struct mongory_array *a_val) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) return NULL;
  value->type = MONGORY_TYPE_ARRAY;
  value->data.a = a_val;
  value->comp = mongory_value_array_compare;
  return value;
}

/** Compares two MONGORY_TYPE_TABLE values. Currently unsupported (always fails). */
static inline int mongory_value_table_compare(mongory_value *a,
                                              mongory_value *b) {
  (void)a; // Unused.
  (void)b; // Unused.
  // Table comparison is complex (order of keys doesn't matter).
  // For now, consider them incomparable or equal only if identical pointers.
  return mongory_value_compare_fail;
}

/** Wraps a mongory_table. */
mongory_value *mongory_value_wrap_t(mongory_memory_pool *pool,
                                    struct mongory_table *t_val) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) return NULL;
  value->type = MONGORY_TYPE_TABLE;
  value->data.t = t_val;
  value->comp = mongory_value_table_compare;
  return value;
}

/** Generic comparison for types not otherwise handled (e.g. POINTER, REGEX, UNSUPPORTED). Always fails. */
static inline int mongory_value_generic_ptr_compare(mongory_value *a,
                                                mongory_value *b) {
  (void)a; // Unused.
  (void)b; // Unused.
  // Generic pointers are generally not comparable in a meaningful way beyond identity.
  return mongory_value_compare_fail;
}

/** Wraps an unsupported/unknown type pointer. */
mongory_value *mongory_value_wrap_u(mongory_memory_pool *pool,
                                    void *u_val) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) return NULL;
  value->type = MONGORY_TYPE_UNSUPPORTED;
  value->data.u = u_val;
  value->comp = mongory_value_generic_ptr_compare; // Unsupported types are not comparable.
  return value;
}


/** Wraps a regex type pointer. */
mongory_value *mongory_value_wrap_regex(mongory_memory_pool *pool,
                                        void *regex_val) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) return NULL;
  value->type = MONGORY_TYPE_REGEX;
  value->data.regex = regex_val;
  value->comp = mongory_value_generic_ptr_compare; // Regex values are not directly comparable.
  return value;
}

/** Wraps a generic void pointer. */
mongory_value *mongory_value_wrap_ptr(mongory_memory_pool *pool, void *ptr_val) {
  mongory_value *value = mongory_value_new(pool);
  if (!value) return NULL;
  value->type = MONGORY_TYPE_POINTER;
  value->data.ptr = ptr_val;
  value->comp = mongory_value_generic_ptr_compare; // Generic pointers are not comparable.
  return value;
}
