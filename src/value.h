#include "table.h"
#include "array.h"

#define MONGORY_TYPE_TABLE(_) \
    _(TYPE_BOOL,   1, "Bool",   b) \
    _(TYPE_INT,    2, "Int",    i) \
    _(TYPE_DOUBLE, 3, "Double", d) \
    _(TYPE_STRING, 4, "String", s) \
    _(TYPE_ARRAY,  5, "Array",  a) \
    _(TYPE_TABLE,  6, "Table",  t)

#define MONGORY_ENUM_MAGIC 103

typedef enum {
  TYPE_NULL = 0,
#define DEFINE_ENUM(name, num, str, field) name = num * MONGORY_ENUM_MAGIC,
  MONGORY_TYPE_TABLE(DEFINE_ENUM)
#undef DEFINE_ENUM
  TYPE_UNSUPPORTED = 999 * MONGORY_ENUM_MAGIC
} mongory_type;

typedef struct mongory_value {
  mongory_type *type;
  union {
    int b; // bool
    int i;
    double d;
    const char *s;
    mongory_array *a;
    mongory_table *t;
  };
} mongory_value;

const char* type_to_string(mongory_type type);
const void* mongory_value_extract(mongory_value *value);