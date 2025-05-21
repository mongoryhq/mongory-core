#include <mongory-core.h>

mongory_matcher* mongory_matcher_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = pool->alloc(pool, sizeof(mongory_matcher));
  matcher->pool = pool;
  matcher->condition = condition;
  return matcher;
}
