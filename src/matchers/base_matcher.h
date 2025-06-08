#include <stdbool.h>

typedef struct mongory_matcher mongory_matcher;
typedef bool (*mongory_matcher_match_func)(struct mongory_matcher *matcher, mongory_value *value);

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
