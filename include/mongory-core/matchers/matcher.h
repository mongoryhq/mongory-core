#ifndef MONGORY_MATCHER_H
#define MONGORY_MATCHER_H

#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"

typedef struct mongory_matcher mongory_matcher; // alias
typedef bool (*mongory_matcher_match_func)(struct mongory_matcher *matcher, mongory_value *value); // match function

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

mongory_matcher* mongory_matcher_new(mongory_memory_pool *pool, mongory_value *condition); // create new matcher

#endif
