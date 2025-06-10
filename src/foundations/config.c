#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "mongory-core/foundations/table.h"
#include "mongory-core/foundations/config.h"
#include "../matchers/base_matcher.h"
#include "config_private.h"

mongory_memory_pool *mongory_internal_pool = NULL;
mongory_regex_adapter *mongory_internal_regex_adapter = NULL;

static void mongory_internal_pool_init() {
  if (mongory_internal_pool != NULL) return;

  mongory_internal_pool = mongory_memory_pool_new();
}

static bool mongory_regex_default_func(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value) {
  (void)pool;
  (void)pattern;
  (void)value;
  return false;
}

static void mongory_internal_regex_adapter_init() {
  if (mongory_internal_regex_adapter != NULL) return;

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

static mongory_table *mongory_matcher_mapping = NULL;

static void mongory_matcher_mapping_init() {
  if (mongory_matcher_mapping != NULL) return;

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

void mongory_init() {
  mongory_internal_pool_init();
  mongory_internal_regex_adapter_init();
  mongory_matcher_mapping_init();
}
