#ifndef MONGORY_VALUE
#define MONGORY_VALUE
#include <stdbool.h>
#include <stdint.h>
#include "mongory-core/foundations/memory_pool.h"

struct mongory_array;
struct mongory_table;

struct mongory_value;
typedef struct mongory_value mongory_value;
enum mongory_type;
typedef enum mongory_type mongory_type;
typedef int (*mongory_value_compare_func)(mongory_value *a, mongory_value *b);
char* mongory_type_to_string(mongory_value *value);
void* mongory_value_extract(mongory_value *value);

mongory_value* mongory_value_wrap_n(mongory_memory_pool *pool, void *n);
mongory_value* mongory_value_wrap_b(mongory_memory_pool *pool, bool b);
mongory_value* mongory_value_wrap_i(mongory_memory_pool *pool, int i);
mongory_value* mongory_value_wrap_d(mongory_memory_pool *pool, double d);
mongory_value* mongory_value_wrap_s(mongory_memory_pool *pool, char *s);
mongory_value* mongory_value_wrap_a(mongory_memory_pool *pool, struct mongory_array *a);
mongory_value* mongory_value_wrap_t(mongory_memory_pool *pool, struct mongory_table *t);
mongory_value* mongory_value_wrap_regex(mongory_memory_pool *pool, void *regex);
mongory_value* mongory_value_wrap_ptr(mongory_memory_pool *pool, void *ptr);
mongory_value* mongory_value_wrap_u(mongory_memory_pool *pool, void *u);

#define MONGORY_TYPE_MACRO(_) \
  _(MONGORY_TYPE_NULL,   0, "Null",   i) \
  _(MONGORY_TYPE_BOOL,   10, "Bool",   b) \
  _(MONGORY_TYPE_INT,    11, "Int",    i) \
  _(MONGORY_TYPE_DOUBLE, 12, "Double", d) \
  _(MONGORY_TYPE_STRING, 13, "String", s) \
  _(MONGORY_TYPE_ARRAY,  14, "Array",  a) \
  _(MONGORY_TYPE_TABLE,  15, "Table",  t) \
  _(MONGORY_TYPE_REGEX,  16, "Regex",  regex) \
  _(MONGORY_TYPE_POINTER, 17, "Pointer", ptr) \
  _(MONGORY_TYPE_UNSUPPORTED, 999, "Unsupported", u)

#define MONGORY_ENUM_MAGIC 103

enum mongory_type {
#define DEFINE_ENUM(name, num, str, field) name = num * MONGORY_ENUM_MAGIC,
  MONGORY_TYPE_MACRO(DEFINE_ENUM)
#undef DEFINE_ENUM
};

static const int mongory_value_compare_fail = 97;

struct mongory_value {
  mongory_memory_pool *pool;
  mongory_type type;
  mongory_value_compare_func comp;
  union {
    bool b;
    int64_t i;
    double d;
    char *s;
    struct mongory_array *a;
    struct mongory_table *t;
    void *regex;
    void *ptr;
    void *u;
  } data;
  void *origin;
};

#endif
