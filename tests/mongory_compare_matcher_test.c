#include "../src/matchers/base_matcher.h"
#include "../src/matchers/compare_matcher.h"
#include "unity.h"
#include <mongory-core.h>
#include <stdio.h>
#include <stdlib.h>

mongory_memory_pool *pool;

void setUp(void) {
  pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);
}

void tearDown(void) {
  if (pool != NULL) {
    pool->free(pool);
    pool = NULL;
  }
}

void test_compare_equal(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_equal_new(pool, condition, NULL);

  mongory_value *value1 = mongory_value_wrap_i(pool, 42);
  mongory_value *value2 = mongory_value_wrap_i(pool, 43);
  mongory_value *value_string = mongory_value_wrap_s(pool, "42");

  TEST_ASSERT_TRUE(matcher->match(matcher, value1));
  TEST_ASSERT_FALSE(matcher->match(matcher, value2));
  TEST_ASSERT_FALSE(matcher->match(matcher, value_string));
}

void test_compare_not_equal(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_not_equal_new(pool, condition, NULL);

  mongory_value *value1 = mongory_value_wrap_i(pool, 42);
  mongory_value *value2 = mongory_value_wrap_i(pool, 43);
  mongory_value *value_string = mongory_value_wrap_s(pool, "42");

  TEST_ASSERT_FALSE(matcher->match(matcher, value1));
  TEST_ASSERT_TRUE(matcher->match(matcher, value2));
  TEST_ASSERT_TRUE(matcher->match(matcher, value_string));
}

void test_compare_greater_than(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_greater_than_new(pool, condition, NULL);

  mongory_value *value1 = mongory_value_wrap_i(pool, 43);
  mongory_value *value2 = mongory_value_wrap_i(pool, 42);
  mongory_value *value3 = mongory_value_wrap_i(pool, 41);
  mongory_value *value_string = mongory_value_wrap_s(pool, "42");

  TEST_ASSERT_TRUE(matcher->match(matcher, value1));
  TEST_ASSERT_FALSE(matcher->match(matcher, value2));
  TEST_ASSERT_FALSE(matcher->match(matcher, value3));
  TEST_ASSERT_FALSE(matcher->match(matcher, value_string));
}

void test_compare_less_than(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_less_than_new(pool, condition, NULL);

  mongory_value *value1 = mongory_value_wrap_i(pool, 41);
  mongory_value *value2 = mongory_value_wrap_i(pool, 42);
  mongory_value *value3 = mongory_value_wrap_i(pool, 43);
  mongory_value *value_string = mongory_value_wrap_s(pool, "42");

  TEST_ASSERT_TRUE(matcher->match(matcher, value1));
  TEST_ASSERT_FALSE(matcher->match(matcher, value2));
  TEST_ASSERT_FALSE(matcher->match(matcher, value3));
  TEST_ASSERT_FALSE(matcher->match(matcher, value_string));
}

void test_compare_greater_than_or_equal(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_greater_than_or_equal_new(pool, condition, NULL);

  mongory_value *value1 = mongory_value_wrap_i(pool, 43);
  mongory_value *value2 = mongory_value_wrap_i(pool, 42);
  mongory_value *value3 = mongory_value_wrap_i(pool, 41);
  mongory_value *value_string = mongory_value_wrap_s(pool, "42");

  TEST_ASSERT_TRUE(matcher->match(matcher, value1));
  TEST_ASSERT_TRUE(matcher->match(matcher, value2));
  TEST_ASSERT_FALSE(matcher->match(matcher, value3));
  TEST_ASSERT_FALSE(matcher->match(matcher, value_string));
}

void test_compare_less_than_or_equal(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_less_than_or_equal_new(pool, condition, NULL);

  mongory_value *value1 = mongory_value_wrap_i(pool, 41);
  mongory_value *value2 = mongory_value_wrap_i(pool, 42);
  mongory_value *value3 = mongory_value_wrap_i(pool, 43);
  mongory_value *value_string = mongory_value_wrap_s(pool, "42");

  TEST_ASSERT_TRUE(matcher->match(matcher, value1));
  TEST_ASSERT_TRUE(matcher->match(matcher, value2));
  TEST_ASSERT_FALSE(matcher->match(matcher, value3));
  TEST_ASSERT_FALSE(matcher->match(matcher, value_string));
}

void test_compare_equal_and_not_equal_on_array(void) {
  mongory_value *scalar_cond = mongory_value_wrap_i(pool, 42);
  mongory_matcher *eq_scalar = mongory_matcher_equal_new(pool, scalar_cond, NULL);
  mongory_matcher *ne_scalar = mongory_matcher_not_equal_new(pool, scalar_cond, NULL);

  mongory_array *arr_with_42 = mongory_array_new(pool);
  arr_with_42->push(arr_with_42, mongory_value_wrap_i(pool, 10));
  arr_with_42->push(arr_with_42, mongory_value_wrap_i(pool, 42));
  mongory_value *val_arr_with_42 = mongory_value_wrap_a(pool, arr_with_42);

  mongory_array *arr_without_42 = mongory_array_new(pool);
  arr_without_42->push(arr_without_42, mongory_value_wrap_i(pool, 10));
  arr_without_42->push(arr_without_42, mongory_value_wrap_i(pool, 20));
  mongory_value *val_arr_without_42 = mongory_value_wrap_a(pool, arr_without_42);

  TEST_ASSERT_TRUE(eq_scalar->match(eq_scalar, val_arr_with_42));
  TEST_ASSERT_FALSE(eq_scalar->match(eq_scalar, val_arr_without_42));
  TEST_ASSERT_FALSE(ne_scalar->match(ne_scalar, val_arr_with_42));
  TEST_ASSERT_TRUE(ne_scalar->match(ne_scalar, val_arr_without_42));

  // Array condition: matches whole array OR an element that is itself that array
  mongory_matcher *eq_arr = mongory_matcher_equal_new(pool, val_arr_without_42, NULL);
  mongory_matcher *ne_arr = mongory_matcher_not_equal_new(pool, val_arr_without_42, NULL);

  mongory_array *nested_arr = mongory_array_new(pool);
  nested_arr->push(nested_arr, val_arr_without_42);
  nested_arr->push(nested_arr, mongory_value_wrap_i(pool, 99));
  mongory_value *val_nested_arr = mongory_value_wrap_a(pool, nested_arr);

  TEST_ASSERT_TRUE(eq_arr->match(eq_arr, val_arr_without_42));
  TEST_ASSERT_TRUE(eq_arr->match(eq_arr, val_nested_arr));
  TEST_ASSERT_FALSE(eq_arr->match(eq_arr, val_arr_with_42));

  TEST_ASSERT_FALSE(ne_arr->match(ne_arr, val_arr_without_42));
  TEST_ASSERT_FALSE(ne_arr->match(ne_arr, val_nested_arr));
  TEST_ASSERT_TRUE(ne_arr->match(ne_arr, val_arr_with_42));
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_compare_equal);
  RUN_TEST(test_compare_not_equal);
  RUN_TEST(test_compare_greater_than);
  RUN_TEST(test_compare_less_than);
  RUN_TEST(test_compare_greater_than_or_equal);
  RUN_TEST(test_compare_less_than_or_equal);
  RUN_TEST(test_compare_equal_and_not_equal_on_array);
  mongory_cleanup();
  return UNITY_END();
}
