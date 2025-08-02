#include "../src/matchers/base_matcher.h"
#include "../src/matchers/inclusion_matcher.h"
#include "mongory-core.h"
#include "unity.h"

mongory_memory_pool *pool;
mongory_array *condition_array;
mongory_array *value_array;
void setUp(void) {
  pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);
  condition_array = mongory_array_new(pool);
  TEST_ASSERT_NOT_NULL(condition_array);
  value_array = mongory_array_new(pool);
  TEST_ASSERT_NOT_NULL(value_array);
}

void tearDown(void) {
  if (pool != NULL) {
    pool->free(pool);
    pool = NULL;
    condition_array = NULL;
    value_array = NULL;
  }
}

void test_in_matcher(void) {
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 42));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 55));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 66));

  mongory_value *condition = mongory_value_wrap_a(pool, condition_array);
  mongory_matcher *matcher = mongory_matcher_in_new(pool, condition);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  mongory_value *value = mongory_value_wrap_i(pool, 42);
  TEST_ASSERT_TRUE(matcher->match(matcher, value));

  value = mongory_value_wrap_i(pool, 55);
  TEST_ASSERT_TRUE(matcher->match(matcher, value));

  value = mongory_value_wrap_i(pool, 66);
  TEST_ASSERT_TRUE(matcher->match(matcher, value));

  value = mongory_value_wrap_i(pool, 77);
  TEST_ASSERT_FALSE(matcher->match(matcher, value));

  value = mongory_value_wrap_i(pool, 88);
  TEST_ASSERT_FALSE(matcher->match(matcher, value));
}

void test_in_matcher_with_array_target(void) {
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 42));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 55));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 66));

  mongory_value *condition = mongory_value_wrap_a(pool, condition_array);
  mongory_matcher *matcher = mongory_matcher_in_new(pool, condition);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  TEST_ASSERT_FALSE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 77));
  TEST_ASSERT_FALSE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 42));
  TEST_ASSERT_TRUE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));
}

void test_in_matcher_invalid_condition(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_in_new(pool, condition);
  TEST_ASSERT_NULL(matcher);
  TEST_ASSERT_NOT_NULL(pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_INVALID_ARGUMENT, pool->error->type);
  TEST_ASSERT_EQUAL_STRING("$in condition must be a valid array.", pool->error->message);
}

void test_not_in_matcher(void) {
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 42));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 55));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 66));

  mongory_value *condition = mongory_value_wrap_a(pool, condition_array);
  mongory_matcher *matcher = mongory_matcher_not_in_new(pool, condition);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  TEST_ASSERT_TRUE(matcher->match(matcher, mongory_value_wrap_i(pool, 77)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 42));
  TEST_ASSERT_FALSE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 55));
  TEST_ASSERT_FALSE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));
}

void test_not_in_matcher_with_array_target(void) {
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 42));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 55));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 66));

  mongory_value *condition = mongory_value_wrap_a(pool, condition_array);
  mongory_matcher *matcher = mongory_matcher_not_in_new(pool, condition);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  TEST_ASSERT_TRUE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 77));
  TEST_ASSERT_TRUE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 42));
  TEST_ASSERT_FALSE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));
}

void test_not_in_matcher_invalid_condition(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_not_in_new(pool, condition);
  TEST_ASSERT_NULL(matcher);
  TEST_ASSERT_NOT_NULL(pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_INVALID_ARGUMENT, pool->error->type);
  TEST_ASSERT_EQUAL_STRING("$nin condition must be a valid array.", pool->error->message);
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_in_matcher);
  RUN_TEST(test_in_matcher_with_array_target);
  RUN_TEST(test_in_matcher_invalid_condition);
  RUN_TEST(test_not_in_matcher);
  RUN_TEST(test_not_in_matcher_with_array_target);
  RUN_TEST(test_not_in_matcher_invalid_condition);
  mongory_cleanup();
  return UNITY_END();
}
