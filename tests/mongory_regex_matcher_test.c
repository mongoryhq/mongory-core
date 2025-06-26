#include "../src/foundations/config_private.h"
#include "../src/matchers/base_matcher.h"
#include "../src/matchers/regex_matcher.h"
#include "mongory-core.h"
#include "mongory-core/foundations/config.h"
#include "unity.h"

mongory_memory_pool *pool;
bool mock_regex_match_result = false;
bool mock_regex_match(mongory_memory_pool *pool, mongory_value *condition,
                      mongory_value *value) {
  (void)pool;
  (void)condition;
  (void)value;
  return mock_regex_match_result;
}

void setUp(void) {
  pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);
  mongory_regex_func_set(mock_regex_match);
}

void tearDown(void) {
  if (pool != NULL) {
    pool->free(pool);
    pool = NULL;
  }
}

void test_regex_matcher_match(void) {
  mongory_value *condition = mongory_value_wrap_s(pool, "test");
  mongory_matcher *matcher = mongory_matcher_regex_new(pool, condition);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  mongory_value *value = mongory_value_wrap_s(pool, "test");
  TEST_ASSERT_FALSE(matcher->match(matcher, value));

  mock_regex_match_result = true;
  TEST_ASSERT_TRUE(matcher->match(matcher, value));

  mock_regex_match_result = false;
  TEST_ASSERT_FALSE(matcher->match(matcher, value));
}

void test_regex_matcher_match_with_invalid_condition(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_regex_new(pool, condition);
  TEST_ASSERT_NULL(matcher);
  TEST_ASSERT_NOT_NULL(pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_INVALID_ARGUMENT, pool->error->type);
  TEST_ASSERT_EQUAL_STRING("$regex condition must be a string or a regex object.",
                           pool->error->message);
}

void test_regex_matcher_match_with_invalid_value(void) {
  mongory_value *condition = mongory_value_wrap_s(pool, "test");
  mongory_matcher *matcher = mongory_matcher_regex_new(pool, condition);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  mock_regex_match_result = true;
  mongory_value *value = mongory_value_wrap_i(pool, 42);
  TEST_ASSERT_FALSE(matcher->match(matcher, value));
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_regex_matcher_match);
  RUN_TEST(test_regex_matcher_match_with_invalid_condition);
  RUN_TEST(test_regex_matcher_match_with_invalid_value);
  mongory_cleanup();
  return UNITY_END();
}
