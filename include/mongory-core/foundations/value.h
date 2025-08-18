#ifndef MONGORY_VALUE
#define MONGORY_VALUE

/**
 * @file value.h
 * @brief Defines the mongory_value structure, a generic value type for the
 * Mongory library.
 *
 * `mongory_value` is a tagged union that can represent various data types such
 * as null, boolean, integer, double, string, array, table, regex, pointers,
 * and an unsupported type. It includes functions for wrapping C types into
 * `mongory_value` objects, comparing values, and converting them to strings.
 */

#include "mongory-core/foundations/memory_pool.h"
#include <stdbool.h>
#include <stdint.h> // For int64_t

// Forward declarations for complex types that can be stored in mongory_value.
struct mongory_array;
struct mongory_table;

// Forward declaration of mongory_value itself and its type enum.
struct mongory_value;
/**
 * @brief Alias for `struct mongory_value`.
 */
typedef struct mongory_value mongory_value;

enum mongory_type;
/**
 * @brief Alias for `enum mongory_type`.
 */
typedef enum mongory_type mongory_type;

/**
 * @brief Function pointer type for comparing two mongory_value instances.
 *
 * @param a The first mongory_value.
 * @param b The second mongory_value.
 * @return int Returns:
 *         - 0 if a is equal to b.
 *         - A negative value if a is less than b.
 *         - A positive value if a is greater than b.
 *         - `mongory_value_compare_fail` (a specific constant) if the types
 *           are incompatible for comparison or an error occurs.
 */
typedef int (*mongory_value_compare_func)(mongory_value *a, mongory_value *b);

/**
 * @brief Function pointer type for converting a mongory_value to a string representation.
 * @param value The mongory_value to convert.
 * @param pool The memory pool to allocate from.
 * @return char* A string literal representing the value, or NULL if allocation
 * fails. The string is a literal and should not be freed.
 */
typedef char *(*mongory_value_to_str_func)(mongory_value *value, mongory_memory_pool *pool);

/**
 * @brief Converts the type of a mongory_value to its string representation.
 * @param value A pointer to the mongory_value.
 * @return char* A string literal representing the type (e.g., "Int", "String").
 * Returns "UnknownType" if the type is not recognized. This string is a
 * literal and should not be freed.
 */
char *mongory_type_to_string(mongory_value *value);

/**
 * @brief Extracts a pointer to the raw data stored within a mongory_value.
 * The type of the returned pointer depends on the `mongory_value`'s type.
 * For example, for an MONGORY_TYPE_INT, it returns `int64_t*`.
 * @param value A pointer to the mongory_value.
 * @return void* A pointer to the internal data, or NULL if the type is unknown
 * or has no direct data pointer (e.g. MONGORY_TYPE_NULL).
 */
void *mongory_value_extract(mongory_value *value);

/** @name Mongory Value Wrapper Functions
 *  @brief Functions to create `mongory_value` instances from basic C types.
 *  These functions allocate a new `mongory_value` from the provided pool.
 *  For string values, the input string is also copied into the pool.
 *  @{
 */
/**
 * @brief Wraps a null value.
 * @param pool The memory pool for allocation.
 * @param n A null pointer.
 * @return A new `mongory_value` of type `MONGORY_TYPE_NULL`.
 */
mongory_value *mongory_value_wrap_n(mongory_memory_pool *pool, void *n);
/**
 * @brief Wraps a boolean value.
 * @param pool The memory pool for allocation.
 * @param b The boolean value (`true` or `false`).
 * @return A new `mongory_value` of type `MONGORY_TYPE_BOOL`.
 */
mongory_value *mongory_value_wrap_b(mongory_memory_pool *pool, bool b);
/**
 * @brief Wraps an integer value.
 * @param pool The memory pool for allocation.
 * @param i The integer value.
 * @return A new `mongory_value` of type `MONGORY_TYPE_INT`.
 */
mongory_value *mongory_value_wrap_i(mongory_memory_pool *pool, int i);
/**
 * @brief Wraps a double value.
 * @param pool The memory pool for allocation.
 * @param d The double value.
 * @return A new `mongory_value` of type `MONGORY_TYPE_DOUBLE`.
 */
mongory_value *mongory_value_wrap_d(mongory_memory_pool *pool, double d);
/**
 * @brief Wraps a string value. The string is copied into the pool.
 * @param pool The memory pool for allocation.
 * @param s The null-terminated string to wrap.
 * @return A new `mongory_value` of type `MONGORY_TYPE_STRING`.
 */
mongory_value *mongory_value_wrap_s(mongory_memory_pool *pool, char *s);
/**
 * @brief Wraps a `mongory_array`.
 * @param pool The memory pool for allocation.
 * @param a A pointer to the `mongory_array`.
 * @return A new `mongory_value` of type `MONGORY_TYPE_ARRAY`.
 */
mongory_value *mongory_value_wrap_a(mongory_memory_pool *pool, struct mongory_array *a);
/**
 * @brief Wraps a `mongory_table`.
 * @param pool The memory pool for allocation.
 * @param t A pointer to the `mongory_table`.
 * @return A new `mongory_value` of type `MONGORY_TYPE_TABLE`.
 */
mongory_value *mongory_value_wrap_t(mongory_memory_pool *pool, struct mongory_table *t);
/**
 * @brief Wraps a custom regex object.
 * @param pool The memory pool for allocation.
 * @param regex A pointer to the custom regex object.
 * @return A new `mongory_value` of type `MONGORY_TYPE_REGEX`.
 */
mongory_value *mongory_value_wrap_regex(mongory_memory_pool *pool, void *regex);
/**
 * @brief Wraps a generic pointer.
 * @param pool The memory pool for allocation.
 * @param ptr A generic `void*` pointer.
 * @return A new `mongory_value` of type `MONGORY_TYPE_POINTER`.
 */
mongory_value *mongory_value_wrap_ptr(mongory_memory_pool *pool, void *ptr);
/**
 * @brief Wraps an unsupported or external type.
 * @param pool The memory pool for allocation.
 * @param u A pointer to the unsupported data.
 * @return A new `mongory_value` of type `MONGORY_TYPE_UNSUPPORTED`.
 */
mongory_value *mongory_value_wrap_u(mongory_memory_pool *pool, void *u);
/** @} */

/**
 * @def MONGORY_TYPE_MACRO
 * @brief An X-Macro for defining `mongory_type` enum members and associated data.
 *
 * This macro simplifies the definition of types, their string names, and
 * corresponding union fields. Using an X-Macro ensures that the enum, its
 * string representation, and other related data are always kept in sync,
 * reducing boilerplate and preventing bugs.
 *
 * The macro takes one argument, `_`, which is expected to be another macro
 * of the form `_(ENUM_NAME, UNIQUE_NUM_SUFFIX, "STRING_NAME", UNION_FIELD_NAME)`.
 *
 * @note For `MONGORY_TYPE_NULL`, the 'i' field (`int64_t`) is arbitrarily used
 * as the union needs a field, but it's not meaningful for a null value.
 */
#define MONGORY_TYPE_MACRO(_)                                                                                          \
  _(MONGORY_TYPE_NULL, 0, "Null", i) /* Field 'i' is arbitrary for NULL */                                             \
  _(MONGORY_TYPE_BOOL, 10, "Bool", b)                                                                                  \
  _(MONGORY_TYPE_INT, 11, "Int", i)                                                                                    \
  _(MONGORY_TYPE_DOUBLE, 12, "Double", d)                                                                              \
  _(MONGORY_TYPE_STRING, 13, "String", s)                                                                              \
  _(MONGORY_TYPE_ARRAY, 14, "Array", a)                                                                                \
  _(MONGORY_TYPE_TABLE, 15, "Table", t)                                                                                \
  _(MONGORY_TYPE_REGEX, 16, "Regex", regex)          /* Custom regex object pointer */                                 \
  _(MONGORY_TYPE_POINTER, 17, "Pointer", ptr)        /* Generic void pointer */                                        \
  _(MONGORY_TYPE_UNSUPPORTED, 999, "Unsupported", u) /* External/unknown type pointer */

/**
 * @def MONGORY_ENUM_MAGIC
 * @brief A magic number used in generating enum values for `mongory_type`.
 *
 * This number is multiplied with a unique suffix for each type to generate the
 * final enum value. This makes the integer values of the enums non-sequential
 * (e.g., 0, 1030, 1133, ...). This can be useful during debugging, as these
 * distinct values are less likely to be confused with other integer values
 * (like array indices or error codes) when inspecting memory.
 */
#define MONGORY_ENUM_MAGIC 103

/**
 * @enum mongory_type
 * @brief Enumerates the possible data types a `mongory_value` can hold.
 * Values are generated using `MONGORY_TYPE_MACRO` and `MONGORY_ENUM_MAGIC`.
 */
enum mongory_type {
#define DEFINE_ENUM(name, num, str, field) name = num * MONGORY_ENUM_MAGIC,
  MONGORY_TYPE_MACRO(DEFINE_ENUM)
#undef DEFINE_ENUM
};

/**
 * @var mongory_value_compare_fail
 * @brief Special return value from compare functions indicating comparison
 * failure (e.g. incompatible types).
 */
static const int mongory_value_compare_fail = 97;

/**
 * @struct mongory_value
 * @brief Represents a generic value in the Mongory system.
 *
 * It's a tagged union, where `type` indicates which field in the `data` union
 * is active. Each value also carries a pointer to the `mongory_memory_pool`
 * it was allocated from and a `comp` function for comparisons.
 */
struct mongory_value {
  mongory_memory_pool *pool;        /**< Memory pool associated with this value. */
  mongory_type type;                /**< The type of data stored in the union. */
  mongory_value_compare_func comp;  /**< Function to compare this value with
                                       another. */
  mongory_value_to_str_func to_str; /**< Function to convert this value to a
                                      string representation. */
  union {
    bool b;                  /**< Boolean data. */
    int64_t i;               /**< Integer data (64-bit). */
    double d;                /**< Double-precision floating-point data. */
    char *s;                 /**< String data (null-terminated). String memory
                                is typically managed by the pool. */
    struct mongory_array *a; /**< Pointer to a `mongory_array`. */
    struct mongory_table *t; /**< Pointer to a `mongory_table`. */
    void *regex;             /**< Pointer to a custom regex object/structure. */
    void *ptr;               /**< Generic void pointer for other data. */
    void *u;                 /**< Pointer for unsupported/external types. */
  } data;                    /**< Union holding the actual data based on type. */
  void *origin;              /**< Optional pointer to an original external value. This is
                                useful for bridging with other data systems. For example,
                                when wrapping a value from a scripting language (like a
                                Python object or a JavaScript value), this field can hold
                                a handle to the original object. This allows callbacks
                                into the host language without losing the original context. */
};

#endif /* MONGORY_VALUE */
