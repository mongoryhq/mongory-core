#include "mongory-core.h"
#include "unity.h"
#include "../src/matchers/base_matcher.h"
#include "../src/matchers/existance_matcher.h"

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

void test_exists_matcher_true(void) {
    mongory_value *condition = mongory_value_wrap_b(pool, true);
    mongory_matcher *matcher = mongory_matcher_exists_new(pool, condition);
    TEST_ASSERT_NOT_NULL(matcher);
    TEST_ASSERT_NULL(pool->error);

    mongory_value *value = mongory_value_wrap_i(pool, 42);
    TEST_ASSERT_TRUE(matcher->match(matcher, value));

    value = NULL;
    TEST_ASSERT_FALSE(matcher->match(matcher, value));
}

void test_exists_matcher_false(void) {
    mongory_value *condition = mongory_value_wrap_b(pool, false);
    mongory_matcher *matcher = mongory_matcher_exists_new(pool, condition);
    TEST_ASSERT_NOT_NULL(matcher);
    TEST_ASSERT_NULL(pool->error);

    mongory_value *value = mongory_value_wrap_i(pool, 42);
    TEST_ASSERT_FALSE(matcher->match(matcher, value));

    value = NULL;
    TEST_ASSERT_TRUE(matcher->match(matcher, value));
}

void test_exists_matcher_invalid_condition(void) {
    mongory_value *condition = mongory_value_wrap_i(pool, 42);
    mongory_matcher *matcher = mongory_matcher_exists_new(pool, condition);
    TEST_ASSERT_NULL(matcher);
    TEST_ASSERT_NOT_NULL(pool->error);
    TEST_ASSERT_EQUAL(MONGORY_ERROR_INVALID_ARGUMENT, pool->error->type);
    TEST_ASSERT_EQUAL_STRING("Condition must be a boolean value.", pool->error->message);
}

void test_present_matcher_true(void) {
    mongory_value *condition = mongory_value_wrap_b(pool, true);
    mongory_matcher *matcher = mongory_matcher_present_new(pool, condition);
    TEST_ASSERT_NOT_NULL(matcher);
    TEST_ASSERT_NULL(pool->error);
    mongory_value *value = mongory_value_wrap_i(pool, 42);
    TEST_ASSERT_TRUE(matcher->match(matcher, value));

    value = NULL;
    TEST_ASSERT_FALSE(matcher->match(matcher, value));
}

void test_present_matcher_false(void) {
    mongory_value *condition = mongory_value_wrap_b(pool, false);
    mongory_matcher *matcher = mongory_matcher_present_new(pool, condition);
    TEST_ASSERT_NOT_NULL(matcher);
    TEST_ASSERT_NULL(pool->error);

    mongory_value *value = mongory_value_wrap_i(pool, 42);
    TEST_ASSERT_FALSE(matcher->match(matcher, value));

    value = NULL;
    TEST_ASSERT_TRUE(matcher->match(matcher, value));
}

void test_present_matcher_invalid_condition(void) {
    mongory_value *condition = mongory_value_wrap_i(pool, 42);
    mongory_matcher *matcher = mongory_matcher_present_new(pool, condition);
    TEST_ASSERT_NULL(matcher);
    TEST_ASSERT_NOT_NULL(pool->error);
    TEST_ASSERT_EQUAL(MONGORY_ERROR_INVALID_ARGUMENT, pool->error->type);
    TEST_ASSERT_EQUAL_STRING("Condition must be a boolean value.", pool->error->message);
}

int main(void) {
    UNITY_BEGIN();
    mongory_init();
    RUN_TEST(test_exists_matcher_true);
    RUN_TEST(test_exists_matcher_false);
    RUN_TEST(test_exists_matcher_invalid_condition);
    RUN_TEST(test_present_matcher_true);
    RUN_TEST(test_present_matcher_false);
    RUN_TEST(test_present_matcher_invalid_condition);
    mongory_cleanup();
    return UNITY_END();
}

