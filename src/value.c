#include <stddef.h>
#include "value.h"
#include "private.h"

const char* type_to_string(mongory_type type) {
  switch (type) {
#define CASE_GEN(name, num, str, field) case name: return str;
  MONGORY_TYPE_TABLE(CASE_GEN)
#undef CASE_GEN
  default:
      return "UnknownType";
  }
}

const void* mongory_value_extract(mongory_value *value) {
  switch (*value->type) {
#define EXTRACT_CASE(name, num, str, field) case name: return &(value->field);

  MONGORY_TYPE_TABLE(EXTRACT_CASE)
#undef EXTRACT_CASE
  default:
      return NULL;
  }
}
