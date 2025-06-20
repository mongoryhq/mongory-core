#include <string.h>
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "mongory-core/foundations/table.h"
#include "mongory-core/foundations/config.h"
#include "../matchers/base_matcher.h"
#include "config_private.h"
#include "../matchers/inclusion_matcher.h"
#include "../matchers/compare_matcher.h"
#include "../matchers/existance_matcher.h"
#include "../matchers/regex_matcher.h"
#include "../matchers/composite_matcher.h"
#include "../matchers/literal_matcher.h"

mongory_memory_pool *mongory_internal_pool = NULL;
mongory_regex_adapter *mongory_internal_regex_adapter = NULL;
mongory_table *mongory_matcher_mapping = NULL;
mongory_value_converter *mongory_internal_value_converter = NULL;

static inline void mongory_internal_pool_init() {
  if (mongory_internal_pool != NULL) {
    return;
  }

  mongory_internal_pool = mongory_memory_pool_new();
}

static inline bool mongory_regex_default_func(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value) {
  (void)pool;
  (void)pattern;
  (void)value;
  return false;
}

static inline void mongory_internal_regex_adapter_init() {
  if (mongory_internal_regex_adapter != NULL) {
    return;
  }

  mongory_internal_regex_adapter = mongory_internal_pool->alloc(mongory_internal_pool->ctx, sizeof(mongory_regex_adapter));
  if (mongory_internal_regex_adapter == NULL) {
    return;
  }

  mongory_internal_regex_adapter->func = mongory_regex_default_func;
}

void mongory_regex_func_set(mongory_regex_func func) {
  if (mongory_internal_regex_adapter == NULL) {
    mongory_internal_regex_adapter_init();
  }
  if (mongory_internal_regex_adapter != NULL) {
    mongory_internal_regex_adapter->func = func;
  }
}

static inline void mongory_matcher_mapping_init() {
  if (mongory_matcher_mapping != NULL) {
    return;
  }

  mongory_matcher_mapping = mongory_table_new(mongory_internal_pool);
  if (mongory_matcher_mapping == NULL) {
    return;
  }
}

void mongory_matcher_register(char *name, mongory_matcher_build_func build_func) {
  mongory_value *value = mongory_value_wrap_ptr(mongory_internal_pool, build_func);
  if (value == NULL) {
    return;
  }

  mongory_matcher_mapping->set(mongory_matcher_mapping, name, value);
}

mongory_matcher_build_func mongory_matcher_build_func_get(char *name) {
  mongory_value *value = mongory_matcher_mapping->get(mongory_matcher_mapping, name);
  if (value == NULL) {
    return NULL;
  }

  return (mongory_matcher_build_func)value->data.ptr;
}

static inline void mongory_internal_value_converter_init() {
  if (mongory_internal_value_converter != NULL) {
    return;
  }

  mongory_internal_value_converter = mongory_internal_pool->alloc(mongory_internal_pool->ctx, sizeof(mongory_value_converter));
  if (mongory_internal_value_converter == NULL) {
    return;
  }
}

void mongory_value_converter_deep_convert_set(mongory_deep_convert_func deep_convert) {
  mongory_internal_value_converter->deep_convert = deep_convert;
}

void mongory_value_converter_shallow_convert_set(mongory_shallow_convert_func shallow_convert) {
  mongory_internal_value_converter->shallow_convert = shallow_convert;
}

void mongory_value_converter_recover_set(mongory_recover_func recover) {
  mongory_internal_value_converter->recover = recover;
}

char* mongory_string_cpy(mongory_memory_pool *pool, char *str) {
  if (str == NULL) {
    return NULL;
  }

  size_t len = strlen(str);
  char *new_str = pool->alloc(pool->ctx, len + 1);
  if (new_str == NULL) {
    return NULL;
  }

  strcpy(new_str, str);

  return new_str;
}

void mongory_init() {
  mongory_internal_pool_init();
  mongory_internal_regex_adapter_init();
  mongory_matcher_mapping_init();
  mongory_internal_value_converter_init();
  mongory_matcher_register("$in", mongory_matcher_in_new);
  mongory_matcher_register("$nin", mongory_matcher_not_in_new);
  mongory_matcher_register("$eq", mongory_matcher_equal_new);
  mongory_matcher_register("$ne", mongory_matcher_not_equal_new);
  mongory_matcher_register("$gt", mongory_matcher_greater_than_new);
  mongory_matcher_register("$gte", mongory_matcher_greater_than_or_equal_new);
  mongory_matcher_register("$lt", mongory_matcher_less_than_new);
  mongory_matcher_register("$lte", mongory_matcher_less_than_or_equal_new);
  mongory_matcher_register("$exists", mongory_matcher_exists_new);
  mongory_matcher_register("$present", mongory_matcher_present_new);
  mongory_matcher_register("$regex", mongory_matcher_regex_new);
  mongory_matcher_register("$and", mongory_matcher_and_new);
  mongory_matcher_register("$or", mongory_matcher_or_new);
  mongory_matcher_register("$elemMatch", mongory_matcher_elem_match_new);
  mongory_matcher_register("$every", mongory_matcher_every_new);
  mongory_matcher_register("$not", mongory_matcher_not_new);
  mongory_matcher_register("$size", mongory_matcher_size_new);
}

void mongory_cleanup() {
  if (mongory_internal_pool != NULL) {
    mongory_internal_pool->free(mongory_internal_pool);
    mongory_internal_pool = NULL;
  }
  mongory_internal_regex_adapter = NULL;
  mongory_matcher_mapping = NULL;
  mongory_internal_value_converter = NULL;
}
