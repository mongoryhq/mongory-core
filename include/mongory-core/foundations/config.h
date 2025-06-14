#ifndef MONGORY_FOUNDATIONS_CONFIG_H
#define MONGORY_FOUNDATIONS_CONFIG_H
#include <stdbool.h>
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"

typedef bool (*mongory_regex_func)(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value);
typedef mongory_value* (*mongory_deep_convert_func)(mongory_memory_pool *pool, void *value);
typedef mongory_value* (*mongory_shallow_convert_func)(mongory_memory_pool *pool, void *value);
typedef void* (*mongory_recover_func)(mongory_memory_pool *pool, mongory_value *value);

void mongory_init();
void mongory_cleanup();
void mongory_regex_func_set(mongory_regex_func func);
void mongory_value_converter_deep_convert_set(mongory_deep_convert_func deep_convert);
void mongory_value_converter_shallow_convert_set(mongory_shallow_convert_func shallow_convert);
void mongory_value_converter_recover_set(mongory_recover_func recover);

#endif
