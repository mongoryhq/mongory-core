#include <mongory-core.h>
#include "base_matcher.h"
#include "composite_matcher.h"


mongory_matcher* mongory_matcher_and_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_base_new(pool, condition);
}
mongory_matcher* mongory_matcher_or_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_base_new(pool, condition);
}
mongory_matcher* mongory_matcher_elem_match_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_base_new(pool, condition);
}
mongory_matcher* mongory_matcher_every_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_base_new(pool, condition);
}
mongory_matcher* mongory_matcher_table_cond_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_base_new(pool, condition);
}
mongory_matcher* mongory_matcher_array_record_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_base_new(pool, condition);
}
mongory_matcher* mongory_matcher_literal_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_base_new(pool, condition);
}