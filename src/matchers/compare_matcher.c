#include <mongory-core.h>
#include "base_matcher.h"
#include "compare_matcher.h"

mongory_matcher* mongory_matcher_compare_new(mongory_memory_pool *pool, mongory_value *condition, mongory_matcher_match_func match_func) {
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  mongory_matcher_context *context = pool->alloc(pool->ctx, sizeof(mongory_matcher_context));
  context->original_match = match_func;
  matcher->match = match_func;
  matcher->context = context;
  return matcher;
}

bool mongory_compare_equal_match(mongory_matcher *matcher, mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) return false;
  return result == 0;
}

mongory_matcher* mongory_compare_equal_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition, mongory_compare_equal_match);
}

bool mongory_compare_not_equal_match(mongory_matcher *matcher, mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) return true;
  return result != 0;
}

mongory_matcher* mongory_compare_not_equal_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition, mongory_compare_not_equal_match);
}

bool mongory_compare_greater_than_match(mongory_matcher *matcher, mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) return false;
  return result == 1;
}

mongory_matcher* mongory_compare_greater_than_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition, mongory_compare_greater_than_match);
}

bool mongory_compare_less_than_match(mongory_matcher *matcher, mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) return false;
  return result == -1;
}

mongory_matcher* mongory_compare_less_than_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition, mongory_compare_less_than_match);
}

bool mongory_compare_greater_than_or_equal_match(mongory_matcher *matcher, mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) return false;
  return result >= 0;
}

mongory_matcher* mongory_compare_greater_than_or_equal_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition, mongory_compare_greater_than_or_equal_match);
}

bool mongory_compare_less_than_or_equal_match(mongory_matcher *matcher, mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) return false;
  return result <= 0;
}

mongory_matcher* mongory_compare_less_than_or_equal_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition, mongory_compare_less_than_or_equal_match);
}
