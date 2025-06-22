#ifndef MONGORY_MATCHER_ARRAY_RECORD_H
#define MONGORY_MATCHER_ARRAY_RECORD_H
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "mongory-core/matchers/matcher.h"

mongory_matcher *mongory_matcher_array_record_new(mongory_memory_pool *pool,
                                                  mongory_value *condition);

#endif