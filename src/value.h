#ifndef MONGORY_VALUE
#define MONGORY_VALUE
#include <stdbool.h>

struct mongory_value;
typedef struct mongory_value mongory_value;
enum mongory_type;
typedef enum mongory_type mongory_type;
char* mongory_type_to_string(mongory_value *value);
void* mongory_value_extract(mongory_value *value);
mongory_value* mongory_value_wrap_s(char *string);

#define MONGORY_TYPE_TABLE(_) \
  _(MONGORY_TYPE_BOOL,   10, "Bool",   b) \
  _(MONGORY_TYPE_INT,    11, "Int",    i) \
  _(MONGORY_TYPE_DOUBLE, 12, "Double", d) \
  _(MONGORY_TYPE_STRING, 13, "String", s) \
  _(MONGORY_TYPE_ARRAY,  14, "Array",  a) \
  _(MONGORY_TYPE_TABLE,  15, "Table",  t)

#endif