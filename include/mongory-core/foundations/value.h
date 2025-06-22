#ifndef MONGORY_VALUE
#define MONGORY_VALUE
#include "mongory-core/foundations/memory_pool.h"
#include <stdbool.h>
#include <stdint.h>

struct mongory_array; // forward declaration
struct mongory_table; // forward declaration

struct mongory_value;                       // forward declaration
typedef struct mongory_value mongory_value; // alias
enum mongory_type;                          // forward declaration
typedef enum mongory_type mongory_type;     // alias
typedef int (*mongory_value_compare_func)(mongory_value *a,
                                          mongory_value *b); // compare function
char *mongory_type_to_string(mongory_value *value);  // convert type to string
char *mongory_value_to_string(mongory_value *value); // convert value to string
void *mongory_value_extract(mongory_value *value);   // extract value

mongory_value *mongory_value_wrap_n(mongory_memory_pool *pool,
                                    void *n); // wrap null
mongory_value *mongory_value_wrap_b(mongory_memory_pool *pool,
                                    bool b); // wrap boolean
mongory_value *mongory_value_wrap_i(mongory_memory_pool *pool,
                                    int i); // wrap integer
mongory_value *mongory_value_wrap_d(mongory_memory_pool *pool,
                                    double d); // wrap double
mongory_value *mongory_value_wrap_s(mongory_memory_pool *pool,
                                    char *s); // wrap string
mongory_value *mongory_value_wrap_a(mongory_memory_pool *pool,
                                    struct mongory_array *a); // wrap array
mongory_value *mongory_value_wrap_t(mongory_memory_pool *pool,
                                    struct mongory_table *t); // wrap table
mongory_value *mongory_value_wrap_regex(mongory_memory_pool *pool,
                                        void *regex); // wrap regex
mongory_value *mongory_value_wrap_ptr(mongory_memory_pool *pool,
                                      void *ptr); // wrap pointer
mongory_value *mongory_value_wrap_u(mongory_memory_pool *pool,
                                    void *u); // wrap unsupported

#define MONGORY_TYPE_MACRO(_)                                                  \
  _(MONGORY_TYPE_NULL, 0, "Null", i)                                           \
  _(MONGORY_TYPE_BOOL, 10, "Bool", b)                                          \
  _(MONGORY_TYPE_INT, 11, "Int", i)                                            \
  _(MONGORY_TYPE_DOUBLE, 12, "Double", d)                                      \
  _(MONGORY_TYPE_STRING, 13, "String", s)                                      \
  _(MONGORY_TYPE_ARRAY, 14, "Array", a)                                        \
  _(MONGORY_TYPE_TABLE, 15, "Table", t)                                        \
  _(MONGORY_TYPE_REGEX, 16, "Regex", regex)                                    \
  _(MONGORY_TYPE_POINTER, 17, "Pointer", ptr)                                  \
  _(MONGORY_TYPE_UNSUPPORTED, 999, "Unsupported", u)

#define MONGORY_ENUM_MAGIC 103

enum mongory_type {
#define DEFINE_ENUM(name, num, str, field) name = num * MONGORY_ENUM_MAGIC,
  MONGORY_TYPE_MACRO(DEFINE_ENUM)
#undef DEFINE_ENUM
};

static const int mongory_value_compare_fail = 97;

struct mongory_value {
  mongory_memory_pool *pool;       // memory pool
  mongory_type type;               // type
  mongory_value_compare_func comp; // compare function
  union {
    bool b;                  // boolean
    int64_t i;               // integer
    double d;                // double
    char *s;                 // string
    struct mongory_array *a; // array
    struct mongory_table *t; // table
    void *regex;             // regex
    void *ptr;               // pointer
    void *u;                 // unsupported
  } data;                    // data
  void *origin;              // for bridged value pointer
};

#endif
