#include "../src/foundations/config_private.h"
#include "../src/matchers/base_matcher.h"
#include "../src/matchers/compare_matcher.h"
#include "../src/matchers/composite_matcher.h"
#include "../src/matchers/existance_matcher.h"
#include "../src/matchers/inclusion_matcher.h"
#include "../src/matchers/literal_matcher.h"
#include "../src/matchers/external_matcher.h"
#include "unity.h"
#include <mongory-core.h>
#include <stdio.h>
#include <stdlib.h>

mongory_memory_pool *pool;

void setUp(void) {
  mongory_init();
  pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);
}

void tearDown(void) {
  if (pool != NULL) {
    pool->free(pool);
    pool = NULL;
  }
  mongory_cleanup();
}

void test_mongory_init(void) {
  TEST_ASSERT_NOT_NULL(mongory_internal_pool);
  TEST_ASSERT_NOT_NULL(mongory_matcher_mapping);
  TEST_ASSERT_NOT_NULL(mongory_internal_regex_adapter.match_func);
  TEST_ASSERT_NOT_NULL(mongory_internal_regex_adapter.stringify_func);
}

void test_mongory_cleanup(void) {
  mongory_cleanup();
  TEST_ASSERT_NULL(mongory_internal_pool);
  TEST_ASSERT_NULL(mongory_matcher_mapping);
}

static bool test_regex_func(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value) {
  (void)pool;
  (void)pattern;
  (void)value;
  return true;
}

void test_mongory_regex_func_set(void) {
  mongory_regex_func_set(test_regex_func);
  TEST_ASSERT_NOT_NULL(mongory_internal_regex_adapter.match_func);
  TEST_ASSERT_EQUAL(test_regex_func, mongory_internal_regex_adapter.match_func);
}

static mongory_matcher *test_matcher_build_func(mongory_memory_pool *pool, mongory_value *condition, void *extern_ctx) {
  (void)pool;
  (void)condition;
  (void)extern_ctx;
  return NULL;
}

void test_mongory_matcher_register(void) {
  mongory_matcher_build_func test_build_func = test_matcher_build_func;
  mongory_matcher_register("$test", test_build_func);
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$test"));
  mongory_matcher_build_func retrieved = mongory_matcher_build_func_get("$test");
  TEST_ASSERT_EQUAL(test_build_func, retrieved);
}

void test_mongory_matcher_build_func_get(void) {
  TEST_ASSERT_NOT_NULL(mongory_matcher_mapping);
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$in"));
  TEST_ASSERT_EQUAL(mongory_matcher_in_new, mongory_matcher_build_func_get("$in"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$nin"));
  TEST_ASSERT_EQUAL(mongory_matcher_not_in_new, mongory_matcher_build_func_get("$nin"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$eq"));
  TEST_ASSERT_EQUAL(mongory_matcher_equal_new, mongory_matcher_build_func_get("$eq"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$ne"));
  TEST_ASSERT_EQUAL(mongory_matcher_not_equal_new, mongory_matcher_build_func_get("$ne"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$gt"));
  TEST_ASSERT_EQUAL(mongory_matcher_greater_than_new, mongory_matcher_build_func_get("$gt"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$gte"));
  TEST_ASSERT_EQUAL(mongory_matcher_greater_than_or_equal_new, mongory_matcher_build_func_get("$gte"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$lt"));
  TEST_ASSERT_EQUAL(mongory_matcher_less_than_new, mongory_matcher_build_func_get("$lt"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$lte"));
  TEST_ASSERT_EQUAL(mongory_matcher_less_than_or_equal_new, mongory_matcher_build_func_get("$lte"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$exists"));
  TEST_ASSERT_EQUAL(mongory_matcher_exists_new, mongory_matcher_build_func_get("$exists"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$present"));
  TEST_ASSERT_EQUAL(mongory_matcher_present_new, mongory_matcher_build_func_get("$present"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$regex"));
  TEST_ASSERT_EQUAL(mongory_matcher_regex_new, mongory_matcher_build_func_get("$regex"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$and"));
  TEST_ASSERT_EQUAL(mongory_matcher_and_new, mongory_matcher_build_func_get("$and"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$or"));
  TEST_ASSERT_EQUAL(mongory_matcher_or_new, mongory_matcher_build_func_get("$or"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$elemMatch"));
  TEST_ASSERT_EQUAL(mongory_matcher_elem_match_new, mongory_matcher_build_func_get("$elemMatch"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$every"));
  TEST_ASSERT_EQUAL(mongory_matcher_every_new, mongory_matcher_build_func_get("$every"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$not"));
  TEST_ASSERT_EQUAL(mongory_matcher_not_new, mongory_matcher_build_func_get("$not"));
  TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$size"));
  TEST_ASSERT_EQUAL(mongory_matcher_size_new, mongory_matcher_build_func_get("$size"));
  TEST_ASSERT_NULL(mongory_matcher_build_func_get("$unknown"));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_mongory_init);
  RUN_TEST(test_mongory_cleanup);
  RUN_TEST(test_mongory_regex_func_set);
  RUN_TEST(test_mongory_matcher_register);
  RUN_TEST(test_mongory_matcher_build_func_get);
  return UNITY_END();
}
