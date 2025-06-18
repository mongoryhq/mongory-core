#include <mongory-core.h>
#include "base_matcher.h"
#include "composite_matcher.h"
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "literal_matcher.h"
#include "../foundations/config_private.h"
#include "composite_matcher.h"

mongory_matcher* mongory_matcher_new(mongory_memory_pool *pool, mongory_value *condition) {
  return mongory_matcher_table_cond_new(pool, condition);
}
