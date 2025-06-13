#include <stdlib.h>
#include <stdio.h>
#include "unity.h"
#include <mongory-core.h>
#include "../src/foundations/config_private.h"
#include "../src/matchers/inclusion_matcher.h"

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
    TEST_ASSERT_NOT_NULL(mongory_internal_regex_adapter);
}

void test_mongory_cleanup(void) {
    mongory_cleanup();
    TEST_ASSERT_NULL(mongory_internal_pool);
    TEST_ASSERT_NULL(mongory_internal_regex_adapter);
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

bool print_matcher_mapping(char *key, mongory_value *value, void *acc) {
    (void)acc;
    printf("key: %s, value: %p\n", key, value);
    return true;
}

void test_mongory_matcher_build_func_get(void) {
    TEST_ASSERT_NOT_NULL(mongory_matcher_mapping);
    
    mongory_matcher_mapping->each(mongory_matcher_mapping, NULL, print_matcher_mapping);
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$in"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$nin"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$eq"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$ne"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$gt"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$gte"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$lt"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$lte"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$exists"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$present"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$regex"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$and"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$or"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$elemMatch"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$every"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$not"));
    TEST_ASSERT_NOT_NULL(mongory_matcher_build_func_get("$size"));
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
