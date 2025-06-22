#ifndef MONGORY_MATCHER_COMPARE_H
#define MONGORY_MATCHER_COMPARE_H
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "mongory-core/matchers/matcher.h"
#include <stdbool.h>

mongory_matcher *mongory_matcher_equal_new(mongory_memory_pool *pool,
                                           mongory_value *condition);
mongory_matcher *mongory_matcher_not_equal_new(mongory_memory_pool *pool,
                                               mongory_value *condition);
mongory_matcher *mongory_matcher_greater_than_new(mongory_memory_pool *pool,
                                                  mongory_value *condition);
mongory_matcher *mongory_matcher_less_than_new(mongory_memory_pool *pool,
                                               mongory_value *condition);
mongory_matcher *
mongory_matcher_greater_than_or_equal_new(mongory_memory_pool *pool,
                                          mongory_value *condition);
mongory_matcher *
mongory_matcher_less_than_or_equal_new(mongory_memory_pool *pool,
                                       mongory_value *condition);
#endif