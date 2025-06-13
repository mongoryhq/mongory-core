#include <mongory-core.h>
#include "base_matcher.h"
#include "array_record_matcher.h"
#include "../foundations/config_private.h"
#include "composite_matcher.h"
#include "literal_matcher.h"

mongory_matcher* mongory_matcher_array_record_new(mongory_memory_pool *pool, mongory_value *condition) {
  return (mongory_matcher *)mongory_matcher_composite_new(pool, condition);
}