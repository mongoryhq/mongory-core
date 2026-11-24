#include "../src/matchers/base_matcher.h"
#include "../src/matchers/inclusion_matcher.h"
#include "mongory-core.h"
#include "unity.h"
#include <math.h>
#include <stdio.h>

mongory_memory_pool *pool;
mongory_array *condition_array;
mongory_array *value_array;
void setUp(void) {
  pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);
  condition_array = mongory_array_new(pool);
  TEST_ASSERT_NOT_NULL(condition_array);
  value_array = mongory_array_new(pool);
  TEST_ASSERT_NOT_NULL(value_array);
}

void tearDown(void) {
  if (pool != NULL) {
    pool->free(pool);
    pool = NULL;
    condition_array = NULL;
    value_array = NULL;
  }
}

void test_in_matcher(void) {
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 42));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 55));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 66));

  mongory_value *condition = mongory_value_wrap_a(pool, condition_array);
  mongory_matcher *matcher = mongory_matcher_in_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  mongory_value *value = mongory_value_wrap_i(pool, 42);
  TEST_ASSERT_TRUE(matcher->match(matcher, value));

  value = mongory_value_wrap_i(pool, 55);
  TEST_ASSERT_TRUE(matcher->match(matcher, value));

  value = mongory_value_wrap_i(pool, 66);
  TEST_ASSERT_TRUE(matcher->match(matcher, value));

  value = mongory_value_wrap_i(pool, 77);
  TEST_ASSERT_FALSE(matcher->match(matcher, value));

  value = mongory_value_wrap_i(pool, 88);
  TEST_ASSERT_FALSE(matcher->match(matcher, value));
}

void test_in_matcher_with_array_target(void) {
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 42));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 55));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 66));

  mongory_value *condition = mongory_value_wrap_a(pool, condition_array);
  mongory_matcher *matcher = mongory_matcher_in_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  TEST_ASSERT_FALSE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 77));
  TEST_ASSERT_FALSE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 42));
  TEST_ASSERT_TRUE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));
}

void test_in_matcher_invalid_condition(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_in_new(pool, condition, NULL);
  TEST_ASSERT_NULL(matcher);
  TEST_ASSERT_NOT_NULL(pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_INVALID_ARGUMENT, pool->error->type);
  TEST_ASSERT_EQUAL_STRING("$in condition must be a valid array.", pool->error->message);
}

void test_not_in_matcher(void) {
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 42));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 55));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 66));

  mongory_value *condition = mongory_value_wrap_a(pool, condition_array);
  mongory_matcher *matcher = mongory_matcher_not_in_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  TEST_ASSERT_TRUE(matcher->match(matcher, mongory_value_wrap_i(pool, 77)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 42));
  TEST_ASSERT_FALSE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 55));
  TEST_ASSERT_FALSE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));
}

void test_not_in_matcher_with_array_target(void) {
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 42));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 55));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 66));

  mongory_value *condition = mongory_value_wrap_a(pool, condition_array);
  mongory_matcher *matcher = mongory_matcher_not_in_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  TEST_ASSERT_TRUE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 77));
  TEST_ASSERT_TRUE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));

  value_array->push(value_array, mongory_value_wrap_i(pool, 42));
  TEST_ASSERT_FALSE(matcher->match(matcher, mongory_value_wrap_a(pool, value_array)));
}

void test_not_in_matcher_invalid_condition(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_not_in_new(pool, condition, NULL);
  TEST_ASSERT_NULL(matcher);
  TEST_ASSERT_NOT_NULL(pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_INVALID_ARGUMENT, pool->error->type);
  TEST_ASSERT_EQUAL_STRING("$nin condition must be a valid array.", pool->error->message);
}

void test_large_in_hash_strings_and_numbers(void) {
  for (int i = 0; i < 100; i++) {
    char buf[32];
    snprintf(buf, sizeof(buf), "item_%d", i);
    char *s = pool->alloc(pool, 32);
    TEST_ASSERT_NOT_NULL(s);
    snprintf(s, 32, "%s", buf);
    condition_array->push(condition_array, mongory_value_wrap_s(pool, s));
  }
  condition_array->push(condition_array, mongory_value_wrap_i(pool, 42));
  condition_array->push(condition_array, mongory_value_wrap_d(pool, 3.5));
  condition_array->push(condition_array, mongory_value_wrap_d(pool, -0.0));
  condition_array->push(condition_array, mongory_value_wrap_d(pool, NAN));
  condition_array->push(condition_array, mongory_value_wrap_i(pool, (INT64_C(1) << 54) + 7));
  condition_array->push(condition_array, mongory_value_wrap_b(pool, true));
  condition_array->push(condition_array, mongory_value_wrap_n(pool, NULL));

  mongory_value *condition = mongory_value_wrap_a(pool, condition_array);
  mongory_matcher *in_m = mongory_matcher_in_new(pool, condition, NULL);
  mongory_matcher *nin_m = mongory_matcher_not_in_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(in_m);
  TEST_ASSERT_NOT_NULL(nin_m);
  TEST_ASSERT_NULL(pool->error);

  // String hits & misses
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_s(pool, "item_0")));
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_s(pool, "item_99")));
  TEST_ASSERT_FALSE(in_m->match(in_m, mongory_value_wrap_s(pool, "item_100")));
  TEST_ASSERT_TRUE(nin_m->match(nin_m, mongory_value_wrap_s(pool, "item_100")));
  TEST_ASSERT_FALSE(nin_m->match(nin_m, mongory_value_wrap_s(pool, "item_50")));

  // Cross-type int/double equality (42 == 42.0, -0.0 == 0, 3.5 == 3.5)
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_i(pool, 42)));
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_d(pool, 42.0)));
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_i(pool, 0)));
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_d(pool, 0.0)));
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_d(pool, 3.5)));
  TEST_ASSERT_FALSE(in_m->match(in_m, mongory_value_wrap_d(pool, 3.6)));

  // NaN never equals NaN
  TEST_ASSERT_FALSE(in_m->match(in_m, mongory_value_wrap_d(pool, NAN)));

  // Large int64 (> 2^53) via fallback
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_i(pool, (INT64_C(1) << 54) + 7)));
  TEST_ASSERT_FALSE(in_m->match(in_m, mongory_value_wrap_i(pool, (INT64_C(1) << 54) + 8)));

  // Bool & null (strict: true != 1, false not in set)
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_b(pool, true)));
  TEST_ASSERT_FALSE(in_m->match(in_m, mongory_value_wrap_b(pool, false)));
  TEST_ASSERT_FALSE(in_m->match(in_m, mongory_value_wrap_i(pool, 1)));
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_n(pool, NULL)));

  // Array intersection with hash set
  value_array->push(value_array, mongory_value_wrap_s(pool, "nope"));
  TEST_ASSERT_FALSE(in_m->match(in_m, mongory_value_wrap_a(pool, value_array)));
  value_array->push(value_array, mongory_value_wrap_s(pool, "item_77"));
  TEST_ASSERT_TRUE(in_m->match(in_m, mongory_value_wrap_a(pool, value_array)));
  TEST_ASSERT_FALSE(nin_m->match(nin_m, mongory_value_wrap_a(pool, value_array)));
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_in_matcher);
  RUN_TEST(test_in_matcher_with_array_target);
  RUN_TEST(test_in_matcher_invalid_condition);
  RUN_TEST(test_not_in_matcher);
  RUN_TEST(test_not_in_matcher_with_array_target);
  RUN_TEST(test_not_in_matcher_invalid_condition);
  RUN_TEST(test_large_in_hash_strings_and_numbers);
  mongory_cleanup();
  return UNITY_END();
}
