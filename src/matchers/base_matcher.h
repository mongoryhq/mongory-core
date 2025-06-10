#ifndef MONGORY_MATCHER_BASE_H
#define MONGORY_MATCHER_BASE_H
#include <stdbool.h>
#include "mongory-core/foundations/array.h"
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"

typedef struct mongory_matcher mongory_matcher;
typedef bool (*mongory_matcher_match_func)(struct mongory_matcher *matcher, mongory_value *value);
typedef mongory_matcher *(*mongory_matcher_build_func)(mongory_memory_pool *pool, mongory_value *condition);

typedef struct mongory_matcher_context {
  mongory_matcher_match_func original_match;
  mongory_array *trace;
} mongory_matcher_context;

struct mongory_matcher {
  char *name;
  mongory_value *condition;
  mongory_matcher_match_func match;
  mongory_memory_pool *pool;
  mongory_matcher_context *context;
};

mongory_matcher* mongory_matcher_base_new(mongory_memory_pool *pool, mongory_value *condition);
#endif