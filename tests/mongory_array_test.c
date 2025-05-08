#include <stdlib.h>
#include <stdio.h>
#include "unity.h"
#include <mongory-core/foundations/memory_pool.h>
#include <mongory-core.h>

mongory_memory_pool *pool;
mongory_array *array;

// 定義回調函數
static bool sum_callback(mongory_value *item, void *acc) {
    int *sum_ptr = (int *)acc;
    *sum_ptr += *(int *)mongory_value_extract(item);
    return true;
}

void setUp(void) {
    pool = mongory_memory_pool_new();
    TEST_ASSERT_NOT_NULL(pool);
    
    array = mongory_array_new(pool);
    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_EQUAL(pool, array->pool);
}

void tearDown(void) {
    if (pool != NULL) {
        pool->free(pool);
        pool = NULL;
        array = NULL;
    }
}

void test_array_creation(void) {
    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_EQUAL(pool, array->pool);
}

void test_array_push_and_get(void) {
    mongory_value *value = mongory_value_wrap_i(pool, 42);
    TEST_ASSERT_NOT_NULL(value);
    
    bool push_result = array->push(array, value);
    TEST_ASSERT_TRUE(push_result);
    
    mongory_value retrieved = array->get(array, 0);
    TEST_ASSERT_EQUAL(42, *(int *)mongory_value_extract(&retrieved));
}

void test_array_set(void) {
    mongory_value *value1 = mongory_value_wrap_i(pool, 1);
    mongory_value *value2 = mongory_value_wrap_i(pool, 2);
    TEST_ASSERT_NOT_NULL(value1);
    TEST_ASSERT_NOT_NULL(value2);
    
    bool set_result = array->set(array, 1, value2);
    TEST_ASSERT_TRUE(set_result);
    
    mongory_value retrieved = array->get(array, 1);
    TEST_ASSERT_EQUAL(2, *(int *)mongory_value_extract(&retrieved));
}

void test_array_each(void) {
    mongory_value *value1 = mongory_value_wrap_i(pool, 1);
    mongory_value *value2 = mongory_value_wrap_i(pool, 2);
    mongory_value *value3 = mongory_value_wrap_i(pool, 3);
    TEST_ASSERT_NOT_NULL(value1);
    TEST_ASSERT_NOT_NULL(value2);
    TEST_ASSERT_NOT_NULL(value3);
    
    array->push(array, value1);
    array->push(array, value2);
    array->push(array, value3);
    
    int sum = 0;
    bool each_result = array->each(array, &sum, sum_callback);
    
    TEST_ASSERT_TRUE(each_result);
    TEST_ASSERT_EQUAL(6, sum);
}

void test_array_out_of_bounds(void) {
    mongory_value retrieved = array->get(array, 999);
    TEST_ASSERT_EQUAL(MONGORY_TYPE_NULL, retrieved.type);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_array_creation);
    RUN_TEST(test_array_push_and_get);
    RUN_TEST(test_array_set);
    RUN_TEST(test_array_each);
    RUN_TEST(test_array_out_of_bounds);
    return UNITY_END();
} 