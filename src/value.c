#include <stddef.h>
#include <stdlib.h>
#include "private.h"
#include "value.h"
#include "table.h"
#include "array.h"

#define MONGORY_ENUM_MAGIC 103

enum mongory_type {
  MONGORY_TYPE_NULL = 0,
#define DEFINE_ENUM(name, num, str, field) name = num * MONGORY_ENUM_MAGIC,
  MONGORY_TYPE_TABLE(DEFINE_ENUM)
#undef DEFINE_ENUM
  MONGORY_TYPE_UNSUPPORTED = 999 * MONGORY_ENUM_MAGIC
};

struct mongory_value {
  mongory_type type;
  union {
    bool b;
    int i;
    double d;
    char *s;
    mongory_array *a;
    mongory_table *t;
  } data;
};

char* mongory_type_to_string(mongory_value *value) {
  switch (value->type) {
#define CASE_GEN(name, num, str, field) case name: return str;
  MONGORY_TYPE_TABLE(CASE_GEN)
#undef CASE_GEN
  default:
      return "UnknownType";
  }
}

void* mongory_value_extract(mongory_value *value) {
  switch (value->type) {
#define EXTRACT_CASE(name, num, str, field) case name: return (void *)&value->data.field;
  MONGORY_TYPE_TABLE(EXTRACT_CASE)
#undef EXTRACT_CASE
  default:
      return NULL;
  }
}

mongory_value* mongory_value_wrap_s(char *string) {
  mongory_value *value = malloc(sizeof(mongory_value));
  value->type = MONGORY_TYPE_STRING;
  value->data.s = string;
  return value;
};