#ifndef MONGORY_MATCHER_REGEX_H
#define MONGORY_MATCHER_REGEX_H
#include "mongory-core/foundations/value.h"
#include "mongory-core/foundations/memory_pool.h"
#include "base_matcher.h"

mongory_matcher *mongory_matcher_regex_new(mongory_memory_pool *pool, mongory_value *condition);

#endif
