#ifndef MONGORY_MATCHER_BASE_H
#define MONGORY_MATCHER_BASE_H
#include <stdbool.h>
#include "mongory-core/foundations/array.h"
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"

typedef struct mongory_matcher mongory_matcher; // alias
typedef bool (*mongory_matcher_match_func)(struct mongory_matcher *matcher, mongory_value *value); // match function
typedef mongory_matcher *(*mongory_matcher_build_func)(mongory_memory_pool *pool, mongory_value *condition); // build function

typedef struct mongory_matcher_context {
  mongory_matcher_match_func original_match; // original match function
  mongory_array *trace; // trace
} mongory_matcher_context;

struct mongory_matcher {
  char *name; // name
  mongory_value *condition; // condition
  mongory_matcher_match_func match; // match function
  mongory_memory_pool *pool; // memory pool
  mongory_matcher_context context; // context
};

mongory_matcher* mongory_matcher_base_new(mongory_memory_pool *pool, mongory_value *condition); // create new base matcher
mongory_matcher* mongory_matcher_always_true_new(mongory_memory_pool *pool, mongory_value *condition); // create new always true matcher
mongory_matcher* mongory_matcher_always_false_new(mongory_memory_pool *pool, mongory_value *condition); // create new always false matcher

bool mongory_try_parse_int(const char *key, int *out); // try parse int

#endif
