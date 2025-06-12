#ifndef MONGORY_MATCHER_H
#define MONGORY_MATCHER_H

#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "base_matcher.h"

mongory_matcher* mongory_matcher_new(mongory_memory_pool *pool, mongory_value *condition);

#endif
