#ifndef MONGORY_MATCHER_INCLUSION_H
#define MONGORY_MATCHER_INCLUSION_H
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "base_matcher.h"

mongory_matcher* mongory_matcher_in_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_not_in_new(mongory_memory_pool *pool, mongory_value *condition);
#endif