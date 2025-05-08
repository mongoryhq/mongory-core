#include <stdlib.h>
#include <stdio.h>
#include "unity.h"
#include <mongory-core/foundations/memory_pool.h>
#include <mongory-core.h>

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

void test_boolean_value(void) {
    mongory_value *value_b = mongory_value_wrap_b(pool, true);
    TEST_ASSERT_NOT_NULL(value_b);
    TEST_ASSERT_EQUAL(pool, value_b->pool);
    TEST_ASSERT_EQUAL_STRING("Bool", mongory_type_to_string(value_b));
    TEST_ASSERT_TRUE(*(bool *)mongory_value_extract(value_b));
}

void test_integer_value(void) {
    mongory_value *value_i = mongory_value_wrap_i(pool, 123);
    TEST_ASSERT_NOT_NULL(value_i);
    TEST_ASSERT_EQUAL(pool, value_i->pool);
    TEST_ASSERT_EQUAL_STRING("Int", mongory_type_to_string(value_i));
    TEST_ASSERT_EQUAL(123, *(int *)mongory_value_extract(value_i));
}

void test_double_value(void) {
    double test_value = 0.123;
    mongory_value *value_d = mongory_value_wrap_d(pool, test_value);
    TEST_ASSERT_NOT_NULL(value_d);
    TEST_ASSERT_EQUAL(pool, value_d->pool);
    TEST_ASSERT_EQUAL_STRING("Double", mongory_type_to_string(value_d));
    double extracted_value = *(double *)mongory_value_extract(value_d);
    TEST_ASSERT_TRUE(extracted_value >= test_value - 0.0001 && extracted_value <= test_value + 0.0001);
}

void test_string_value(void) {
    mongory_value *value_s = mongory_value_wrap_s(pool, "Hello");
    TEST_ASSERT_NOT_NULL(value_s);
    TEST_ASSERT_EQUAL(pool, value_s->pool);
    TEST_ASSERT_EQUAL_STRING("String", mongory_type_to_string(value_s));
    TEST_ASSERT_EQUAL_STRING("Hello", *(char **)mongory_value_extract(value_s));
}

void test_array_value(void) {
    mongory_array *array = mongory_array_new(pool);
    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_EQUAL(pool, array->pool);

    mongory_value *value_a = mongory_value_wrap_a(pool, array);
    TEST_ASSERT_NOT_NULL(value_a);
    TEST_ASSERT_EQUAL(pool, value_a->pool);
    TEST_ASSERT_EQUAL_STRING("Array", mongory_type_to_string(value_a));
}

void test_table_value(void) {
    mongory_table *table = pool->alloc(pool, sizeof(mongory_table));
    TEST_ASSERT_NOT_NULL(table);

    mongory_value *value_t = mongory_value_wrap_t(pool, table);
    TEST_ASSERT_NOT_NULL(value_t);
    TEST_ASSERT_EQUAL(pool, value_t->pool);
    TEST_ASSERT_EQUAL_STRING("Table", mongory_type_to_string(value_t));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_boolean_value);
    RUN_TEST(test_integer_value);
    RUN_TEST(test_double_value);
    RUN_TEST(test_string_value);
    RUN_TEST(test_array_value);
    RUN_TEST(test_table_value);
    return UNITY_END();
}
