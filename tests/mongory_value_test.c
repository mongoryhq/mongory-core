#include "../src/test_helper/test_helper.h"
#include "unity.h"
#include "../src/foundations/string_buffer.h"
#include "mongory-core.h"
#include <stdio.h>
#include <stdlib.h>

void setUp(void) { setup_test_environment(); }

void tearDown(void) { teardown_test_environment(); }

void test_boolean_value(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *value_b = mongory_value_wrap_b(pool, true);
  TEST_ASSERT_NOT_NULL(value_b);
  TEST_ASSERT_EQUAL(pool, value_b->pool);
  TEST_ASSERT_EQUAL_STRING("Bool", mongory_type_to_string(value_b));
  TEST_ASSERT_TRUE(*(bool *)mongory_value_extract(value_b));
}

void test_integer_value(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *value_i = mongory_value_wrap_i(pool, 123);
  TEST_ASSERT_NOT_NULL(value_i);
  TEST_ASSERT_EQUAL(pool, value_i->pool);
  TEST_ASSERT_EQUAL_STRING("Int", mongory_type_to_string(value_i));
  TEST_ASSERT_EQUAL(123, *(int *)mongory_value_extract(value_i));
}

void test_double_value(void) {
  mongory_memory_pool *pool = get_test_pool();
  double test_value = 0.123;
  mongory_value *value_d = mongory_value_wrap_d(pool, test_value);
  TEST_ASSERT_NOT_NULL(value_d);
  TEST_ASSERT_EQUAL(pool, value_d->pool);
  TEST_ASSERT_EQUAL_STRING("Double", mongory_type_to_string(value_d));
  double extracted_value = *(double *)mongory_value_extract(value_d);
  TEST_ASSERT_TRUE(extracted_value >= test_value - 0.0001 &&
                   extracted_value <= test_value + 0.0001);
}

void test_string_value(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *value_s = mongory_value_wrap_s(pool, "Hello");
  TEST_ASSERT_NOT_NULL(value_s);
  TEST_ASSERT_EQUAL(pool, value_s->pool);
  TEST_ASSERT_EQUAL_STRING("String", mongory_type_to_string(value_s));
  TEST_ASSERT_EQUAL_STRING("Hello", *(char **)mongory_value_extract(value_s));
}

void test_array_value(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_array *array = mongory_array_new(pool);
  TEST_ASSERT_NOT_NULL(array);
  TEST_ASSERT_EQUAL(pool, array->pool);

  mongory_value *value_a = mongory_value_wrap_a(pool, array);
  TEST_ASSERT_NOT_NULL(value_a);
  TEST_ASSERT_EQUAL(pool, value_a->pool);
  TEST_ASSERT_EQUAL_STRING("Array", mongory_type_to_string(value_a));
}

void test_table_value(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_table *table = pool->alloc(pool->ctx, sizeof(mongory_table));
  TEST_ASSERT_NOT_NULL(table);

  mongory_value *value_t = mongory_value_wrap_t(pool, table);
  TEST_ASSERT_NOT_NULL(value_t);
  TEST_ASSERT_EQUAL(pool, value_t->pool);
  TEST_ASSERT_EQUAL_STRING("Table", mongory_type_to_string(value_t));
}

void test_null_value(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *null_val = mongory_value_wrap_n(pool, NULL);
  TEST_ASSERT_NOT_NULL(null_val);
  TEST_ASSERT_EQUAL(pool, null_val->pool);
  TEST_ASSERT_EQUAL_STRING("Null", mongory_type_to_string(null_val));
  TEST_ASSERT_EQUAL(0, null_val->comp(null_val, null_val));
  TEST_ASSERT_EQUAL(mongory_value_compare_fail,
                    null_val->comp(null_val, mongory_value_wrap_b(pool, true)));
}

void test_unsupported_value(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *unsupported_val = mongory_value_wrap_u(pool, NULL);
  TEST_ASSERT_NOT_NULL(unsupported_val);
  TEST_ASSERT_EQUAL(pool, unsupported_val->pool);
  TEST_ASSERT_EQUAL_STRING("Unsupported",
                           mongory_type_to_string(unsupported_val));
  TEST_ASSERT_EQUAL(mongory_value_compare_fail,
                    unsupported_val->comp(unsupported_val, unsupported_val));
}

void test_boolean_comparison(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *true_val = mongory_value_wrap_b(pool, true);
  mongory_value *false_val = mongory_value_wrap_b(pool, false);
  mongory_value *another_true = mongory_value_wrap_b(pool, true);
  mongory_value *int_val = mongory_value_wrap_i(pool, 1);

  TEST_ASSERT_EQUAL(0, true_val->comp(true_val, another_true));
  TEST_ASSERT_NOT_EQUAL(0, true_val->comp(true_val, false_val));
  TEST_ASSERT_NOT_EQUAL(0, false_val->comp(false_val, true_val));
  TEST_ASSERT_EQUAL(mongory_value_compare_fail,
                    true_val->comp(true_val, int_val));
}

void test_integer_comparison(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *val_1 = mongory_value_wrap_i(pool, 1);
  mongory_value *val_2 = mongory_value_wrap_i(pool, 2);
  mongory_value *val_1_again = mongory_value_wrap_i(pool, 1);
  mongory_value *double_val = mongory_value_wrap_d(pool, 1.5);
  mongory_value *bool_val = mongory_value_wrap_b(pool, true);

  TEST_ASSERT_EQUAL(0, val_1->comp(val_1, val_1_again));
  TEST_ASSERT_EQUAL(-1, val_1->comp(val_1, val_2));
  TEST_ASSERT_EQUAL(1, val_2->comp(val_2, val_1));
  TEST_ASSERT_EQUAL(-1, val_1->comp(val_1, double_val));
  TEST_ASSERT_EQUAL(mongory_value_compare_fail, val_1->comp(val_1, bool_val));
}

void test_double_comparison(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *val_1_0 = mongory_value_wrap_d(pool, 1.0);
  mongory_value *val_1_5 = mongory_value_wrap_d(pool, 1.5);
  mongory_value *val_1_0_again = mongory_value_wrap_d(pool, 1.0);
  mongory_value *int_val = mongory_value_wrap_i(pool, 1);
  mongory_value *bool_val = mongory_value_wrap_b(pool, true);

  TEST_ASSERT_EQUAL(0, val_1_0->comp(val_1_0, val_1_0_again));
  TEST_ASSERT_EQUAL(-1, val_1_0->comp(val_1_0, val_1_5));
  TEST_ASSERT_EQUAL(1, val_1_5->comp(val_1_5, val_1_0));
  TEST_ASSERT_EQUAL(0, val_1_0->comp(val_1_0, int_val));
  TEST_ASSERT_EQUAL(mongory_value_compare_fail,
                    val_1_0->comp(val_1_0, bool_val));
}

void test_string_comparison(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *str_a = mongory_value_wrap_s(pool, "apple");
  mongory_value *str_b = mongory_value_wrap_s(pool, "banana");
  mongory_value *str_a_again = mongory_value_wrap_s(pool, "apple");
  mongory_value *int_val = mongory_value_wrap_i(pool, 1);

  TEST_ASSERT_EQUAL(0, str_a->comp(str_a, str_a_again));
  TEST_ASSERT_EQUAL(-1, str_a->comp(str_a, str_b));
  TEST_ASSERT_EQUAL(1, str_b->comp(str_b, str_a));
  TEST_ASSERT_EQUAL(mongory_value_compare_fail, str_a->comp(str_a, int_val));
}

void test_array_comparison(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_array *array1 = mongory_array_new(pool);
  mongory_array *array2 = mongory_array_new(pool);
  mongory_value *array_val1 = mongory_value_wrap_a(pool, array1);
  mongory_value *array_val2 = mongory_value_wrap_a(pool, array2);

  array1->push(array1, mongory_value_wrap_i(pool, 1));
  array2->push(array2, mongory_value_wrap_i(pool, 2));

  TEST_ASSERT_EQUAL(-1, array_val1->comp(array_val1, array_val2));
  TEST_ASSERT_EQUAL(1, array_val2->comp(array_val2, array_val1));
}

void test_table_comparison(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_table *table1 = mongory_table_new(pool);
  mongory_table *table2 = mongory_table_new(pool);

  mongory_value *table_val1 = mongory_value_wrap_t(pool, table1);
  mongory_value *table_val2 = mongory_value_wrap_t(pool, table2);

  table1->set(table1, "a", mongory_value_wrap_i(pool, 1));
  table2->set(table2, "a", mongory_value_wrap_i(pool, 2));

  TEST_ASSERT_EQUAL(mongory_value_compare_fail,
                    table_val1->comp(table_val1, table_val2));
  TEST_ASSERT_EQUAL(mongory_value_compare_fail,
                    table_val2->comp(table_val2, table_val1));
}

void test_json_to_value_string(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *value =
      json_string_to_mongory_value(pool, "\"Hello, World!\"");
  TEST_ASSERT_NOT_NULL(value);
  TEST_ASSERT_EQUAL_STRING("Hello, World!", value->data.s);
}

void test_json_to_value_number(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *value = json_string_to_mongory_value(pool, "42");
  TEST_ASSERT_NOT_NULL(value);
  TEST_ASSERT_EQUAL_INT(42, value->data.i);
}

void test_json_to_value_array(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *value = json_string_to_mongory_value(pool, "[1, 2, 3]");
  TEST_ASSERT_NOT_NULL(value);
  TEST_ASSERT_EQUAL(MONGORY_TYPE_ARRAY, value->type);

  mongory_array *array = value->data.a;
  TEST_ASSERT_EQUAL_INT(3, array->count);

  mongory_value *first = array->get(array, 0);
  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_EQUAL_INT(1, first->data.i);
}

void test_json_to_value_object(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *value =
      json_string_to_mongory_value(pool, "{\"name\": \"John\", \"age\": 30}");
  TEST_ASSERT_NOT_NULL(value);
  TEST_ASSERT_EQUAL(MONGORY_TYPE_TABLE, value->type);

  mongory_table *table = value->data.t;
  mongory_value *name = table->get(table, "name");
  mongory_value *age = table->get(table, "age");

  TEST_ASSERT_NOT_NULL(name);
  TEST_ASSERT_NOT_NULL(age);
  TEST_ASSERT_EQUAL_STRING("John", name->data.s);
  TEST_ASSERT_EQUAL_INT(30, age->data.i);
}

void test_value_stringify(void) {
    mongory_memory_pool *pool = get_test_pool();
    mongory_table *table = mongory_table_new(pool);
    mongory_array *array = mongory_array_new(pool);

    array->push(array, mongory_value_wrap_i(pool, 1));
    array->push(array, mongory_value_wrap_s(pool, "two"));
    array->push(array, mongory_value_wrap_b(pool, true));

    table->set(table, "name", mongory_value_wrap_s(pool, "John"));
    table->set(table, "age", mongory_value_wrap_i(pool, 30));
    table->set(table, "isStudent", mongory_value_wrap_b(pool, false));
    table->set(table, "courses", mongory_value_wrap_a(pool, array));

    mongory_value *value = mongory_value_wrap_t(pool, table);
    mongory_string_buffer *buffer = mongory_string_buffer_new(pool);
    value->to_str(value, buffer);

    const char *expected = "{\"age\":30,\"isStudent\":false,\"name\":\"John\",\"courses\":[1,\"two\",true]}";
    TEST_ASSERT_EQUAL_STRING(expected, mongory_string_buffer_cstr(buffer));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_boolean_value);
  RUN_TEST(test_integer_value);
  RUN_TEST(test_double_value);
  RUN_TEST(test_string_value);
  RUN_TEST(test_array_value);
  RUN_TEST(test_table_value);
  RUN_TEST(test_boolean_comparison);
  RUN_TEST(test_integer_comparison);
  RUN_TEST(test_double_comparison);
  RUN_TEST(test_string_comparison);
  RUN_TEST(test_array_comparison);
  RUN_TEST(test_table_comparison);
  RUN_TEST(test_null_value);
  RUN_TEST(test_unsupported_value);
  RUN_TEST(test_json_to_value_string);
  RUN_TEST(test_json_to_value_number);
  RUN_TEST(test_json_to_value_array);
  RUN_TEST(test_json_to_value_object);
  RUN_TEST(test_value_stringify);
  return UNITY_END();
}
