#ifndef MONGORY_MATCHER_LITERAL_H
#define MONGORY_MATCHER_LITERAL_H
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "base_matcher.h"

mongory_matcher* mongory_matcher_field_new(mongory_memory_pool *pool, char *field, mongory_value *condition);
mongory_matcher* mongory_matcher_not_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_size_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_literal_new(mongory_memory_pool *pool, mongory_value *condition);

#endif
