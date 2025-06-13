#include <mongory-core.h>
#include "base_matcher.h"
#include "composite_matcher.h"

typedef struct mongory_composite_matcher {
  mongory_matcher *base;
  mongory_matcher *left;
  mongory_matcher *right;
} mongory_composite_matcher;

mongory_matcher* mongory_matcher_composite_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (matcher == NULL) {
    return NULL;
  }
  matcher->name = NULL;
  matcher->match = NULL;
  mongory_composite_matcher *composite = (mongory_composite_matcher *)pool->alloc(pool->ctx, sizeof(mongory_composite_matcher));
  if (composite == NULL) {
    return NULL;
  }
  composite->base = matcher;
  composite->left = NULL;
  composite->right = NULL;
  return composite;
}

mongory_matcher* mongory_matcher_and_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_composite_new(pool, condition);
}
mongory_matcher* mongory_matcher_or_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_composite_new(pool, condition);
}
mongory_matcher* mongory_matcher_elem_match_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_composite_new(pool, condition);
}
mongory_matcher* mongory_matcher_every_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_composite_new(pool, condition);
}
mongory_matcher* mongory_matcher_table_cond_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_composite_new(pool, condition);
}
mongory_matcher* mongory_matcher_array_record_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_composite_new(pool, condition);
}
mongory_matcher* mongory_matcher_literal_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_composite_new(pool, condition);
}
