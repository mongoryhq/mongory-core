#ifndef MONGORY_MATCHER_COMPOSITE_H
#define MONGORY_MATCHER_COMPOSITE_H
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "base_matcher.h"

typedef struct mongory_composite_matcher {
  mongory_matcher base;
  mongory_matcher *left;
  mongory_matcher *right;
} mongory_composite_matcher;

mongory_matcher* mongory_matcher_and_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_or_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_elem_match_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_every_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_matcher_table_cond_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_composite_matcher* mongory_matcher_composite_new(mongory_memory_pool *pool, mongory_value *condition);

#endif
