#include <stddef.h>
#include <stdlib.h>
#include <mongory-core/foundations/memory_pool.h>
#include <mongory-core/foundations/value.h>
#include <mongory-core/foundations/table.h>
#include <mongory-core/foundations/array.h>

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