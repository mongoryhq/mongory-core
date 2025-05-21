#include <stdbool.h>

mongory_matcher* mongory_compare_equal_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_compare_not_equal_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_compare_greater_than_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_compare_less_than_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_compare_greater_than_or_equal_new(mongory_memory_pool *pool, mongory_value *condition);
mongory_matcher* mongory_compare_less_than_or_equal_new(mongory_memory_pool *pool, mongory_value *condition);
