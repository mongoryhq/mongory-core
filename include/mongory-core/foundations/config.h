#include <stdbool.h>

#ifndef MONGORY_FOUNDATIONS_CONFIG_H
#define MONGORY_FOUNDATIONS_CONFIG_H

typedef bool (*mongory_regex_func)(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value);

void mongory_init();
void mongory_regex_func_set(mongory_regex_func func);

#endif
