#include <stdlib.h>
#include <stdio.h>
#include "unity.h"
#include <mongory-core.h>
#include "../src/foundations/config_private.h"

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

void test_mongory_init(void) {
    TEST_ASSERT_NOT_NULL(mongory_internal_pool);
    TEST_ASSERT_NOT_NULL(mongory_internal_regex_adapter);
}

void test_mongory_cleanup(void) {
    mongory_cleanup();
    TEST_ASSERT_NULL(mongory_internal_pool);
    TEST_ASSERT_NULL(mongory_internal_regex_adapter);
    mongory_init();
}

static bool test_regex_func(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value) {
    (void)pool;
    (void)pattern;
    (void)value;
    return true;
}

void test_mongory_regex_func_set(void) {
    mongory_regex_func_set(test_regex_func);
    TEST_ASSERT_NOT_NULL(mongory_internal_regex_adapter);
    TEST_ASSERT_NOT_NULL(mongory_internal_regex_adapter->func);
    TEST_ASSERT_EQUAL(test_regex_func, mongory_internal_regex_adapter->func);
}

static mongory_matcher* test_matcher_build_func(mongory_memory_pool *pool, mongory_value *condition) {
    (void)pool;
    (void)condition;
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
    mongory_matcher_build_func in_matcher = mongory_matcher_build_func_get("$in");
    TEST_ASSERT_NOT_NULL(in_matcher);

    mongory_matcher_build_func unknown_matcher = mongory_matcher_build_func_get("$unknown");
    TEST_ASSERT_NULL(unknown_matcher);
}

int main(void) {
    UNITY_BEGIN();
    mongory_init();
    RUN_TEST(test_mongory_init);
    RUN_TEST(test_mongory_cleanup);
    RUN_TEST(test_mongory_regex_func_set);
    RUN_TEST(test_mongory_matcher_register);
    RUN_TEST(test_mongory_matcher_build_func_get);
    mongory_cleanup();
    return UNITY_END();
}
