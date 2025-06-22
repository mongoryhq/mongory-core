#ifndef MONGORY_MATCHER_EXISTANCE_H
#define MONGORY_MATCHER_EXISTANCE_H
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "mongory-core/matchers/matcher.h"

mongory_matcher *mongory_matcher_exists_new(mongory_memory_pool *pool,
                                            mongory_value *condition);
mongory_matcher *mongory_matcher_present_new(mongory_memory_pool *pool,
                                             mongory_value *condition);
#endif