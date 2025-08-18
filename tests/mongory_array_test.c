#include "unity.h"
#include <mongory-core.h>
#include "../src/foundations/array_private.h"
#include <stdio.h>
#include <stdlib.h>

mongory_memory_pool *pool;
mongory_array *array;

// Define callback function
static bool sum_callback(mongory_value *item, void *acc) {
  int *sum_ptr = (int *)acc;
  *sum_ptr += *(int *)mongory_value_extract(item);
  return true;
}

static size_t sort_callback(mongory_value *item, void *ctx) {
  (void)ctx;
  return *(size_t *)mongory_value_extract(item);
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

  mongory_value *retrieved = array->get(array, 0);
  TEST_ASSERT_NOT_NULL(retrieved);
  TEST_ASSERT_EQUAL(42, *(int *)mongory_value_extract(retrieved));
}

void test_array_set(void) {
  mongory_value *value1 = mongory_value_wrap_i(pool, 1);
  mongory_value *value2 = mongory_value_wrap_i(pool, 2);
  TEST_ASSERT_NOT_NULL(value1);
  TEST_ASSERT_NOT_NULL(value2);

  bool set_result = array->set(array, 1, value2);
  TEST_ASSERT_TRUE(set_result);

  mongory_value *retrieved = array->get(array, 1);
  TEST_ASSERT_NOT_NULL(retrieved);
  TEST_ASSERT_EQUAL(2, *(int *)mongory_value_extract(retrieved));
}

void test_array_each(void) {
  mongory_array *array = mongory_array_nested_wrap(pool, 3,
    mongory_value_wrap_i(pool, 1),
    mongory_value_wrap_i(pool, 2),
    mongory_value_wrap_i(pool, 3)
  );

  int sum = 0;
  bool each_result = array->each(array, &sum, sum_callback);

  TEST_ASSERT_TRUE(each_result);
  TEST_ASSERT_EQUAL(6, sum);
}

void test_array_out_of_bounds(void) {
  mongory_value *retrieved = array->get(array, 999);
  TEST_ASSERT_NULL(retrieved);
}

void test_array_sort_by(void) {
  mongory_array *array = mongory_array_nested_wrap(pool, 10,
    mongory_value_wrap_i(pool, 7),
    mongory_value_wrap_i(pool, 3),
    mongory_value_wrap_i(pool, 8),
    mongory_value_wrap_i(pool, 6),
    mongory_value_wrap_i(pool, 10),
    mongory_value_wrap_i(pool, 5),
    mongory_value_wrap_i(pool, 4),
    mongory_value_wrap_i(pool, 9),
    mongory_value_wrap_i(pool, 1),
    mongory_value_wrap_i(pool, 2)
  );

  mongory_array *sorted_array = mongory_array_sort_by(array, pool, NULL, sort_callback);

  TEST_ASSERT_EQUAL(1, *(int *)mongory_value_extract(sorted_array->get(sorted_array, 0)));
  TEST_ASSERT_EQUAL(2, *(int *)mongory_value_extract(sorted_array->get(sorted_array, 1)));
  TEST_ASSERT_EQUAL(3, *(int *)mongory_value_extract(sorted_array->get(sorted_array, 2)));
  TEST_ASSERT_EQUAL(4, *(int *)mongory_value_extract(sorted_array->get(sorted_array, 3)));
  TEST_ASSERT_EQUAL(5, *(int *)mongory_value_extract(sorted_array->get(sorted_array, 4)));
  TEST_ASSERT_EQUAL(6, *(int *)mongory_value_extract(sorted_array->get(sorted_array, 5)));
  TEST_ASSERT_EQUAL(7, *(int *)mongory_value_extract(sorted_array->get(sorted_array, 6)));
  TEST_ASSERT_EQUAL(8, *(int *)mongory_value_extract(sorted_array->get(sorted_array, 7)));
  TEST_ASSERT_EQUAL(9, *(int *)mongory_value_extract(sorted_array->get(sorted_array, 8)));
  TEST_ASSERT_EQUAL(10, *(int *)mongory_value_extract(sorted_array->get(sorted_array, 9)));
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_array_creation);
  RUN_TEST(test_array_push_and_get);
  RUN_TEST(test_array_set);
  RUN_TEST(test_array_each);
  RUN_TEST(test_array_out_of_bounds);
  RUN_TEST(test_array_sort_by);
  mongory_cleanup();
  return UNITY_END();
}
