#include <stddef.h>
#include <stdlib.h>
#include <mongory-core/foundations/memory_pool.h>
#include <mongory-core/foundations/value.h>
#include <mongory-core/foundations/table.h>
#include <mongory-core/foundations/array.h>

char* mongory_type_to_string(mongory_value *value) {
  switch (value->type) {
#define CASE_GEN(name, num, str, field) case name: return str;
  MONGORY_TYPE_MACRO(CASE_GEN)
#undef CASE_GEN
  default:
      return "UnknownType";
  }
}

void* mongory_value_extract(mongory_value *value) {
  switch (value->type) {
#define EXTRACT_CASE(name, num, str, field) case name: return (void *)&value->data.field;
  MONGORY_TYPE_MACRO(EXTRACT_CASE)
#undef EXTRACT_CASE
  default:
      return NULL;
  }
}

mongory_value* mongory_value_new(mongory_memory_pool *pool) {
  return pool->alloc(pool, sizeof(mongory_value));
}

mongory_value* mongory_value_wrap_b(mongory_memory_pool *pool, bool b) {
  mongory_value *value = mongory_value_new(pool);
  value->type = MONGORY_TYPE_BOOL;
  value->data.b = b;
  return value;
};

mongory_value* mongory_value_wrap_i(mongory_memory_pool *pool, int i) {
  mongory_value *value = mongory_value_new(pool);
  value->type = MONGORY_TYPE_INT;
  value->data.i = i;
  return value;
};

mongory_value* mongory_value_wrap_d(mongory_memory_pool *pool, double d) {
  mongory_value *value = mongory_value_new(pool);
  value->type = MONGORY_TYPE_DOUBLE;
  value->data.d = d;
  return value;
};

mongory_value* mongory_value_wrap_s(mongory_memory_pool *pool, char *s) {
  mongory_value *value = mongory_value_new(pool);
  value->type = MONGORY_TYPE_STRING;
  value->data.s = s;
  return value;
};

mongory_value* mongory_value_wrap_a(mongory_memory_pool *pool, struct mongory_array *a) {
  mongory_value *value = mongory_value_new(pool);
  value->type = MONGORY_TYPE_ARRAY;
  value->data.a = a;
  return value;
};

mongory_value* mongory_value_wrap_t(mongory_memory_pool *pool, struct mongory_table *t) {
  mongory_value *value = mongory_value_new(pool);
  value->type = MONGORY_TYPE_TABLE;
  value->data.t = t;
  return value;
};
