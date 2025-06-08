#include <mongory-core.h>
#include "base_matcher.h"

mongory_matcher* mongory_matcher_base_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = pool->alloc(pool->ctx, sizeof(mongory_matcher));
  matcher->pool = pool;
  matcher->condition = condition;
  return matcher;
}
