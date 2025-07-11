#include "unity.h"
#include <mongory-core.h>
#include <stdio.h>
#include <stdlib.h>

mongory_memory_pool *pool;
mongory_table *table;

void setUp(void) {
  pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);
  table = mongory_table_new(pool);
  TEST_ASSERT_NOT_NULL(table);
}

void tearDown(void) {
  if (pool != NULL) {
    pool->free(pool);
    pool = NULL;
    table = NULL;
  }
}

void test_table_set_and_get(void) {
  mongory_value *value = mongory_value_wrap_i(pool, 42);
  TEST_ASSERT_NOT_NULL(value);

  bool set_result = table->set(table, "test_key", value);
  TEST_ASSERT_TRUE(set_result);

  mongory_value *retrieved = table->get(table, "test_key");
  TEST_ASSERT_NOT_NULL(retrieved);
  TEST_ASSERT_EQUAL(42, *(int *)mongory_value_extract(retrieved));

  mongory_value *value_2 = mongory_value_wrap_i(pool, 55);
  TEST_ASSERT_NOT_NULL(value_2);

  bool set_result_2 = table->set(table, "test_key", value_2);
  TEST_ASSERT_TRUE(set_result_2);

  mongory_value *retrieved_2 = table->get(table, "test_key");
  TEST_ASSERT_NOT_NULL(retrieved_2);
  TEST_ASSERT_EQUAL(55, *(int *)mongory_value_extract(retrieved_2));

  mongory_value *retrieved_3 = table->get(table, "test_key_2");
  TEST_ASSERT_NULL(retrieved_3);
}

void test_table_delete(void) {
  mongory_value *value = mongory_value_wrap_s(pool, "test_value");
  TEST_ASSERT_NOT_NULL(value);

  table->set(table, "test_key", value);
  mongory_value *retrieved = table->get(table, "test_key");
  TEST_ASSERT_NOT_NULL(retrieved);
  TEST_ASSERT_EQUAL_STRING("test_value", retrieved->data.s);

  bool del_result = table->del(table, "test_key");
  TEST_ASSERT_TRUE(del_result);

  mongory_value *retrieved_2 = table->get(table, "test_key");
  TEST_ASSERT_NULL(retrieved_2);
}

bool test_table_each_callback(char *key, mongory_value *value, void *acc) {
  (void)key;
  (void)value;
  (*(int *)acc)++;
  return true;
}

void test_table_each(void) {
  table->set(table, "key1", mongory_value_wrap_i(pool, 1));
  table->set(table, "key2", mongory_value_wrap_s(pool, "test"));

  int count = 0;
  bool callback_result = table->each(table, &count, test_table_each_callback);

  TEST_ASSERT_TRUE(callback_result);
  TEST_ASSERT_EQUAL(2, count);
}

void test_table_get_nonexistent(void) {
  mongory_value *retrieved = table->get(table, "nonexistent_key");
  TEST_ASSERT_NULL(retrieved);
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_table_set_and_get);
  RUN_TEST(test_table_delete);
  RUN_TEST(test_table_each);
  RUN_TEST(test_table_get_nonexistent);
  mongory_cleanup();
  return UNITY_END();
}
