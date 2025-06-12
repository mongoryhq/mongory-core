#ifndef MONGORY_MATCHER_COMPOSITE_H
#define MONGORY_MATCHER_COMPOSITE_H
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "base_matcher.h"

mongory_matcher* mongory_matcher_and_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_or_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_elem_match_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_every_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_table_cond_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_array_record_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_literal_new(mongory_memory_pool *pool, mongory_value *condition);

#endif
