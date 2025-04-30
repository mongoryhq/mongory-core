#include "table.h"
#include "array.h"

#define MONGORY_TYPE_TABLE(_) \
    _(MONGORY_TYPE_BOOL,   10, "Bool",   b) \
    _(MONGORY_TYPE_INT,    11, "Int",    i) \
    _(MONGORY_TYPE_DOUBLE, 12, "Double", d) \
    _(MONGORY_TYPE_STRING, 13, "String", s) \
    _(MONGORY_TYPE_ARRAY,  14, "Array",  a) \
    _(MONGORY_TYPE_TABLE,  15, "Table",  t)

#define MONGORY_ENUM_MAGIC 103

typedef enum {
  TYPE_NULL = 0,
#define DEFINE_ENUM(name, num, str, field) name = num * MONGORY_ENUM_MAGIC,
  MONGORY_TYPE_TABLE(DEFINE_ENUM)
#undef DEFINE_ENUM
  TYPE_UNSUPPORTED = 999 * MONGORY_ENUM_MAGIC
} mongory_type;

typedef struct mongory_value {
  mongory_type type;
  union {
    int b; // bool
    int i;
    double d;
    char *s;
    mongory_array *a;
    mongory_table *t;
  } data;
} mongory_value;

char* mongory_type_to_string(mongory_type type);
void* mongory_value_extract(mongory_value *value);
mongory_value* mongory_value_wrap_s(char *string);