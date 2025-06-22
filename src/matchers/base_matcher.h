#ifndef MONGORY_MATCHER_BASE_H
#define MONGORY_MATCHER_BASE_H
#include "mongory-core/foundations/array.h"
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "mongory-core/matchers/matcher.h"
#include <stdbool.h>

mongory_matcher *
mongory_matcher_base_new(mongory_memory_pool *pool,
                         mongory_value *condition); // create new base matcher
mongory_matcher *mongory_matcher_always_true_new(
    mongory_memory_pool *pool,
    mongory_value *condition); // create new always true matcher
mongory_matcher *mongory_matcher_always_false_new(
    mongory_memory_pool *pool,
    mongory_value *condition); // create new always false matcher

bool mongory_try_parse_int(const char *key, int *out); // try parse int

#endif
