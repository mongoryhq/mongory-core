#include "../src/foundations/string_buffer.h"
#include "unity.h"
#include <mongory-core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

mongory_memory_pool *pool;
mongory_string_buffer *buffer;

void setUp(void) {
  pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);

  buffer = mongory_string_buffer_new(pool);
  TEST_ASSERT_NOT_NULL(buffer);
  TEST_ASSERT_EQUAL(pool, buffer->pool);
}

void tearDown(void) {
  if (pool != NULL) {
    pool->free(pool);
    pool = NULL;
    buffer = NULL;
  }
}

void test_string_buffer_creation(void) {
  TEST_ASSERT_NOT_NULL(buffer);
  TEST_ASSERT_EQUAL(pool, buffer->pool);
  TEST_ASSERT_NOT_NULL(buffer->buffer);
  TEST_ASSERT_EQUAL(0, buffer->size);
  TEST_ASSERT_EQUAL(256, buffer->capacity); // MONGORY_STRING_BUFFER_INITIAL_CAPACITY
  TEST_ASSERT_EQUAL_STRING("", mongory_string_buffer_cstr(buffer));
}

void test_string_buffer_append(void) {
  const char *test_str = "Hello";
  mongory_string_buffer_append(buffer, test_str);

  TEST_ASSERT_EQUAL(5, buffer->size);
  TEST_ASSERT_EQUAL_STRING("Hello", mongory_string_buffer_cstr(buffer));

  // 測試附加更多字串
  mongory_string_buffer_append(buffer, " World");
  TEST_ASSERT_EQUAL(11, buffer->size);
  TEST_ASSERT_EQUAL_STRING("Hello World", mongory_string_buffer_cstr(buffer));
}

void test_string_buffer_append_empty_string(void) {
  mongory_string_buffer_append(buffer, "");

  TEST_ASSERT_EQUAL(0, buffer->size);
  TEST_ASSERT_EQUAL_STRING("", mongory_string_buffer_cstr(buffer));
}

void test_string_buffer_appendf(void) {
  mongory_string_buffer_appendf(buffer, "Number: %d", 42);

  TEST_ASSERT_EQUAL(10, buffer->size);
  TEST_ASSERT_EQUAL_STRING("Number: 42", mongory_string_buffer_cstr(buffer));

  // 測試附加更多格式化字串
  mongory_string_buffer_appendf(buffer, ", String: %s", "test");
  TEST_ASSERT_EQUAL(24, buffer->size);
  TEST_ASSERT_EQUAL_STRING("Number: 42, String: test", mongory_string_buffer_cstr(buffer));
}

void test_string_buffer_appendf_complex(void) {
  mongory_string_buffer_appendf(buffer, "Float: %.2f, Char: %c, Hex: 0x%x", 3.14159, 'A', 255);

  const char *expected = "Float: 3.14, Char: A, Hex: 0xff";
  TEST_ASSERT_EQUAL_STRING(expected, mongory_string_buffer_cstr(buffer));
}

void test_string_buffer_clear(void) {
  mongory_string_buffer_append(buffer, "Hello World");
  TEST_ASSERT_EQUAL(11, buffer->size);

  mongory_string_buffer_clear(buffer);

  TEST_ASSERT_EQUAL(0, buffer->size);
  TEST_ASSERT_EQUAL(256, buffer->capacity); // 重設為初始容量
  TEST_ASSERT_EQUAL_STRING("", mongory_string_buffer_cstr(buffer));
}

void test_string_buffer_dynamic_growth(void) {
  // 建立一個超過初始容量的長字串
  char large_string[300];
  for (int i = 0; i < 299; i++) {
    large_string[i] = 'A';
  }
  large_string[299] = '\0';

  mongory_string_buffer_append(buffer, large_string);

  TEST_ASSERT_EQUAL(299, buffer->size);
  TEST_ASSERT_GREATER_THAN(256, buffer->capacity); // 容量應該已經增長
  TEST_ASSERT_EQUAL_STRING(large_string, mongory_string_buffer_cstr(buffer));
}

void test_string_buffer_multiple_growth(void) {
  // 測試多次增長
  for (int i = 0; i < 10; i++) {
    mongory_string_buffer_appendf(buffer, "This is line %d with some text. ", i);
  }

  TEST_ASSERT_GREATER_THAN(300, buffer->size);
  TEST_ASSERT_GREATER_THAN(256, buffer->capacity);

  char *result = mongory_string_buffer_cstr(buffer);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_TRUE(strstr(result, "This is line 0") != NULL);
  TEST_ASSERT_TRUE(strstr(result, "This is line 9") != NULL);
}

void test_string_buffer_mixed_operations(void) {
  mongory_string_buffer_append(buffer, "Start: ");
  mongory_string_buffer_appendf(buffer, "%d", 123);
  mongory_string_buffer_append(buffer, " Middle ");
  mongory_string_buffer_appendf(buffer, "%.1f", 45.6);
  mongory_string_buffer_append(buffer, " End");

  TEST_ASSERT_EQUAL_STRING("Start: 123 Middle 45.6 End", mongory_string_buffer_cstr(buffer));
}

void test_string_buffer_cstr_consistency(void) {
  mongory_string_buffer_append(buffer, "Test");

  char *str1 = mongory_string_buffer_cstr(buffer);
  char *str2 = mongory_string_buffer_cstr(buffer);

  TEST_ASSERT_EQUAL(str1, str2); // 應該回傳相同的指標
  TEST_ASSERT_EQUAL_STRING("Test", str1);
}

void test_string_buffer_after_clear_and_reuse(void) {
  mongory_string_buffer_append(buffer, "First content");
  mongory_string_buffer_clear(buffer);
  mongory_string_buffer_append(buffer, "Second content");

  TEST_ASSERT_EQUAL_STRING("Second content", mongory_string_buffer_cstr(buffer));
  TEST_ASSERT_EQUAL(14, buffer->size);
}

void test_string_buffer_large_formatted_string(void) {
  // 測試大型格式化字串
  mongory_string_buffer_appendf(buffer, "Large number: %ld, repeated %d times", 123456789L, 1000);

  const char *result = mongory_string_buffer_cstr(buffer);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_TRUE(strstr(result, "123456789") != NULL);
  TEST_ASSERT_TRUE(strstr(result, "1000") != NULL);
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();

  RUN_TEST(test_string_buffer_creation);
  RUN_TEST(test_string_buffer_append);
  RUN_TEST(test_string_buffer_append_empty_string);
  RUN_TEST(test_string_buffer_appendf);
  RUN_TEST(test_string_buffer_appendf_complex);
  RUN_TEST(test_string_buffer_clear);
  RUN_TEST(test_string_buffer_dynamic_growth);
  RUN_TEST(test_string_buffer_multiple_growth);
  RUN_TEST(test_string_buffer_mixed_operations);
  RUN_TEST(test_string_buffer_cstr_consistency);
  RUN_TEST(test_string_buffer_after_clear_and_reuse);
  RUN_TEST(test_string_buffer_large_formatted_string);

  mongory_cleanup();
  return UNITY_END();
}
