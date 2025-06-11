#include <stdlib.h>
#include <stdio.h>
#include "unity.h"
#include <mongory-core.h>
#include "../src/matchers/base_matcher.h"
#include "../src/matchers/compare_matcher.h"

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
    mongory_matcher *matcher = mongory_matcher_equal_new(pool, condition);

    mongory_value *value1 = mongory_value_wrap_i(pool, 42);
    mongory_value *value2 = mongory_value_wrap_i(pool, 43);
    mongory_value *value_string = mongory_value_wrap_s(pool, "42");

    TEST_ASSERT_TRUE(matcher->match(matcher, value1));
    TEST_ASSERT_FALSE(matcher->match(matcher, value2));
    TEST_ASSERT_FALSE(matcher->match(matcher, value_string));
}

void test_compare_not_equal(void) {
    mongory_value *condition = mongory_value_wrap_i(pool, 42);
    mongory_matcher *matcher = mongory_matcher_not_equal_new(pool, condition);

    mongory_value *value1 = mongory_value_wrap_i(pool, 42);
    mongory_value *value2 = mongory_value_wrap_i(pool, 43);
    mongory_value *value_string = mongory_value_wrap_s(pool, "42");

    TEST_ASSERT_FALSE(matcher->match(matcher, value1));
    TEST_ASSERT_TRUE(matcher->match(matcher, value2));
    TEST_ASSERT_TRUE(matcher->match(matcher, value_string));
}

void test_compare_greater_than(void) {
    mongory_value *condition = mongory_value_wrap_i(pool, 42);
    mongory_matcher *matcher = mongory_matcher_greater_than_new(pool, condition);

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
    mongory_matcher *matcher = mongory_matcher_less_than_new(pool, condition);

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
    mongory_matcher *matcher = mongory_matcher_greater_than_or_equal_new(pool, condition);

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
    mongory_matcher *matcher = mongory_matcher_less_than_or_equal_new(pool, condition);

    mongory_value *value1 = mongory_value_wrap_i(pool, 41);
    mongory_value *value2 = mongory_value_wrap_i(pool, 42);
    mongory_value *value3 = mongory_value_wrap_i(pool, 43);
    mongory_value *value_string = mongory_value_wrap_s(pool, "42");

    TEST_ASSERT_TRUE(matcher->match(matcher, value1));
    TEST_ASSERT_TRUE(matcher->match(matcher, value2));
    TEST_ASSERT_FALSE(matcher->match(matcher, value3));
    TEST_ASSERT_FALSE(matcher->match(matcher, value_string));
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
    return UNITY_END();
}