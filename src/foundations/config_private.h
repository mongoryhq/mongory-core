#ifndef MONGORY_FOUNDATIONS_CONFIG_PRIVATE_H
#define MONGORY_FOUNDATIONS_CONFIG_PRIVATE_H
#include "mongory-core/foundations/config.h"
#include "mongory-core/foundations/table.h"
#include "../matchers/base_matcher.h"

typedef struct mongory_regex_adapter {
  mongory_regex_func func;
} mongory_regex_adapter;

extern mongory_memory_pool *mongory_internal_pool;
extern mongory_regex_adapter *mongory_internal_regex_adapter;
extern mongory_table *mongory_matcher_mapping;

void mongory_matcher_register(char *name, mongory_matcher_build_func build_func);
mongory_matcher_build_func mongory_matcher_build_func_get(char *name);

#endif
