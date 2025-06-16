#ifndef MONGORY_FOUNDATIONS_CONFIG_H
#define MONGORY_FOUNDATIONS_CONFIG_H
#include <stdbool.h>
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"

typedef bool (*mongory_regex_func)(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value); // regex function
typedef mongory_value* (*mongory_deep_convert_func)(mongory_memory_pool *pool, void *value); // deep convert function
typedef mongory_value* (*mongory_shallow_convert_func)(mongory_memory_pool *pool, void *value); // shallow convert function
typedef void* (*mongory_recover_func)(mongory_memory_pool *pool, mongory_value *value); // recover function

void mongory_init(); // initialize mongory
void mongory_cleanup(); // cleanup mongory
void mongory_regex_func_set(mongory_regex_func func); // set regex function
void mongory_value_converter_deep_convert_set(mongory_deep_convert_func deep_convert); // set deep convert function
void mongory_value_converter_shallow_convert_set(mongory_shallow_convert_func shallow_convert); // set shallow convert function
void mongory_value_converter_recover_set(mongory_recover_func recover); // set recover function

#endif
