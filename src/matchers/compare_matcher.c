#include "compare_matcher.h"
#include "base_matcher.h"
#include <mongory-core.h>

mongory_matcher *
mongory_matcher_compare_new(mongory_memory_pool *pool, mongory_value *condition,
                            mongory_matcher_match_func match_func) {
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  if (matcher == NULL) {
    return NULL;
  }
  matcher->match = match_func;
  matcher->context.original_match = match_func;
  return matcher;
}

static inline bool mongory_matcher_equal_match(mongory_matcher *matcher,
                                               mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return false;
  }
  return result == 0;
}

mongory_matcher *mongory_matcher_equal_new(mongory_memory_pool *pool,
                                           mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition,
                                     mongory_matcher_equal_match);
}

static inline bool mongory_matcher_not_equal_match(mongory_matcher *matcher,
                                                   mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return true;
  }
  return result != 0;
}

mongory_matcher *mongory_matcher_not_equal_new(mongory_memory_pool *pool,
                                               mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition,
                                     mongory_matcher_not_equal_match);
}

static inline bool mongory_matcher_greater_than_match(mongory_matcher *matcher,
                                                      mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return false;
  }
  return result == 1;
}

mongory_matcher *mongory_matcher_greater_than_new(mongory_memory_pool *pool,
                                                  mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition,
                                     mongory_matcher_greater_than_match);
}

static inline bool mongory_matcher_less_than_match(mongory_matcher *matcher,
                                                   mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return false;
  }
  return result == -1;
}

mongory_matcher *mongory_matcher_less_than_new(mongory_memory_pool *pool,
                                               mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition,
                                     mongory_matcher_less_than_match);
}

static inline bool
mongory_matcher_greater_than_or_equal_match(mongory_matcher *matcher,
                                            mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return false;
  }
  return result >= 0;
}

mongory_matcher *
mongory_matcher_greater_than_or_equal_new(mongory_memory_pool *pool,
                                          mongory_value *condition) {
  return mongory_matcher_compare_new(
      pool, condition, mongory_matcher_greater_than_or_equal_match);
}

static inline bool
mongory_matcher_less_than_or_equal_match(mongory_matcher *matcher,
                                         mongory_value *value) {
  int result = value->comp(value, matcher->condition);
  if (result == mongory_value_compare_fail) {
    return false;
  }
  return result <= 0;
}

mongory_matcher *
mongory_matcher_less_than_or_equal_new(mongory_memory_pool *pool,
                                       mongory_value *condition) {
  return mongory_matcher_compare_new(pool, condition,
                                     mongory_matcher_less_than_or_equal_match);
}
