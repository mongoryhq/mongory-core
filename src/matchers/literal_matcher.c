#include <mongory-core.h>
#include "base_matcher.h"
#include "literal_matcher.h"
#include "../foundations/config_private.h"
#include "composite_matcher.h"

mongory_matcher* mongory_matcher_field_new(mongory_memory_pool *pool, char *field, mongory_value *condition) {
  (void)field;
  return mongory_matcher_base_new(pool, condition);
}

mongory_matcher* mongory_matcher_not_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_base_new(pool, condition);
}

mongory_matcher* mongory_matcher_size_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_base_new(pool, condition);
}

mongory_matcher* mongory_matcher_literal_new(mongory_memory_pool *pool, mongory_value *condition) {
  return (mongory_matcher *)mongory_matcher_composite_new(pool, condition);
}
