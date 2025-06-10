#ifndef MONGORY_MATCHER_COMPARE_H
#define MONGORY_MATCHER_COMPARE_H
#include <stdbool.h>
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "base_matcher.h"

mongory_matcher* mongory_compare_equal_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_compare_not_equal_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_compare_greater_than_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_compare_less_than_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_compare_greater_than_or_equal_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_compare_less_than_or_equal_new(mongory_memory_pool *pool, mongory_value *condition);
#endif