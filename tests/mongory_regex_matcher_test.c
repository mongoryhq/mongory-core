#include "../src/foundations/config_private.h"
#include "../src/matchers/base_matcher.h"
#include "../src/matchers/external_matcher.h"
#include "mongory-core.h"
#include "mongory-core/foundations/config.h"
#include "unity.h"
#include <string.h>

mongory_memory_pool *pool;
bool mock_regex_match_result = false;
bool mock_regex_match(mongory_memory_pool *pool, mongory_value *condition, mongory_value *value) {
  (void)pool;
  (void)condition;
  (void)value;
  return mock_regex_match_result;
}

char captured_options[8];
bool mock_regex_options_match(mongory_memory_pool *pool, mongory_value *condition, const char *options,
                              mongory_value *value) {
  (void)pool;
  (void)condition;
  strncpy(captured_options, options, sizeof(captured_options) - 1);
  return strcmp(value->data.s, "RUBY") == 0;
}

void setUp(void) {
  pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);
  mongory_regex_func_set(mock_regex_match);
  mongory_regex_options_func_set(mock_regex_options_match);
  mock_regex_match_result = false;
  memset(captured_options, 0, sizeof(captured_options));
}

void tearDown(void) {
  if (pool != NULL) {
    pool->free(pool);
    pool = NULL;
  }
}

void test_regex_matcher_match(void) {
  mongory_value *condition = mongory_value_wrap_s(pool, "test");
  mongory_matcher *matcher = mongory_matcher_regex_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  mongory_value *value = mongory_value_wrap_s(pool, "test");
  TEST_ASSERT_FALSE(matcher->match(matcher, value));

  mock_regex_match_result = true;
  TEST_ASSERT_TRUE(matcher->match(matcher, value));

  mock_regex_match_result = false;
  TEST_ASSERT_FALSE(matcher->match(matcher, value));
}

void test_regex_matcher_match_with_invalid_condition(void) {
  mongory_value *condition = mongory_value_wrap_i(pool, 42);
  mongory_matcher *matcher = mongory_matcher_regex_new(pool, condition, NULL);
  TEST_ASSERT_NULL(matcher);
  TEST_ASSERT_NOT_NULL(pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_INVALID_ARGUMENT, pool->error->type);
  TEST_ASSERT_EQUAL_STRING("$regex condition must be a string or a regex object.", pool->error->message);
}

void test_regex_matcher_match_with_invalid_value(void) {
  mongory_value *condition = mongory_value_wrap_s(pool, "test");
  mongory_matcher *matcher = mongory_matcher_regex_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  mock_regex_match_result = true;
  mongory_value *value = mongory_value_wrap_i(pool, 42);
  TEST_ASSERT_FALSE(matcher->match(matcher, value));
}

static mongory_value *regex_condition(mongory_value *options) {
  mongory_value *inner = MG_TABLE_WRAP(pool, 1, "$regex", mongory_value_wrap_s(pool, "^ru"));
  if (options) {
    inner->data.t->set(inner->data.t, "$options", options);
  }
  return MG_TABLE_WRAP(pool, 1, "name", inner);
}

static void assert_build_error(mongory_value *condition, mongory_error_type type, const char *message) {
  mongory_matcher *matcher = mongory_matcher_new(pool, condition, NULL);
  TEST_ASSERT_NULL(matcher);
  TEST_ASSERT_NOT_NULL(pool->error);
  TEST_ASSERT_EQUAL(type, pool->error->type);
  TEST_ASSERT_EQUAL_STRING(message, pool->error->message);
}

void test_regex_options_uses_options_adapter(void) {
  mongory_matcher *matcher = mongory_matcher_new(pool, regex_condition(mongory_value_wrap_s(pool, "ims")), NULL);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);

  TEST_ASSERT_TRUE(mongory_matcher_match(matcher, MG_TABLE_WRAP(pool, 1, "name", mongory_value_wrap_s(pool, "RUBY"))));
  TEST_ASSERT_EQUAL_STRING("ims", captured_options);
  TEST_ASSERT_FALSE(mongory_matcher_match(matcher, MG_TABLE_WRAP(pool, 1, "name", mongory_value_wrap_s(pool, "go"))));
}

void test_regex_empty_options_uses_plain_adapter(void) {
  mongory_matcher *matcher = mongory_matcher_new(pool, regex_condition(mongory_value_wrap_s(pool, "")), NULL);
  TEST_ASSERT_NOT_NULL(matcher);

  mock_regex_match_result = true;
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher, MG_TABLE_WRAP(pool, 1, "name", mongory_value_wrap_s(pool, "go"))));
  TEST_ASSERT_EQUAL_STRING("", captured_options);
}

void test_regex_options_travel_into_implicit_elem_match(void) {
  mongory_matcher *matcher = mongory_matcher_new(pool, regex_condition(mongory_value_wrap_s(pool, "i")), NULL);
  TEST_ASSERT_NOT_NULL(matcher);

  mongory_value *hit = MG_TABLE_WRAP(pool, 1, "name",
    MG_ARRAY_WRAP(pool, 2, mongory_value_wrap_s(pool, "go"), mongory_value_wrap_s(pool, "RUBY")));
  mongory_value *miss = MG_TABLE_WRAP(pool, 1, "name",
    MG_ARRAY_WRAP(pool, 2, mongory_value_wrap_s(pool, "go"), mongory_value_wrap_s(pool, "c")));
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher, hit));
  TEST_ASSERT_NULL(pool->error);
  TEST_ASSERT_EQUAL_STRING("i", captured_options);
  TEST_ASSERT_FALSE(mongory_matcher_match(matcher, miss));
}

void test_regex_options_without_regex_fails(void) {
  mongory_value *condition = MG_TABLE_WRAP(pool, 1, "name",
    MG_TABLE_WRAP(pool, 1, "$options", mongory_value_wrap_s(pool, "i")));
  assert_build_error(condition, MONGORY_ERROR_INVALID_ARGUMENT, "$options requires a sibling $regex.");
}

void test_regex_options_with_unsupported_flag_fails(void) {
  assert_build_error(regex_condition(mongory_value_wrap_s(pool, "ix")), MONGORY_ERROR_INVALID_ARGUMENT,
                     "$options has unsupported flag 'x'; allowed flags are i, m, s.");
}

void test_regex_options_non_string_fails(void) {
  assert_build_error(regex_condition(mongory_value_wrap_i(pool, 1)), MONGORY_ERROR_INVALID_ARGUMENT,
                     "$options must be a string.");
}

void test_regex_options_with_compiled_regex_fails(void) {
  mongory_value *inner = MG_TABLE_WRAP(pool, 2, "$regex", mongory_value_wrap_regex(pool, (void *)"compiled"),
                                       "$options", mongory_value_wrap_s(pool, "i"));
  assert_build_error(MG_TABLE_WRAP(pool, 1, "name", inner), MONGORY_ERROR_INVALID_ARGUMENT,
                     "$options cannot modify a compiled regex object.");
}

void test_regex_options_without_options_adapter_fails(void) {
  mongory_regex_options_func_set(NULL);
  assert_build_error(regex_condition(mongory_value_wrap_s(pool, "i")), MONGORY_ERROR_UNSUPPORTED_OPERATION,
                     "$options requires a regex adapter set via mongory_regex_options_func_set.");
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_regex_matcher_match);
  RUN_TEST(test_regex_matcher_match_with_invalid_condition);
  RUN_TEST(test_regex_matcher_match_with_invalid_value);
  RUN_TEST(test_regex_options_uses_options_adapter);
  RUN_TEST(test_regex_empty_options_uses_plain_adapter);
  RUN_TEST(test_regex_options_travel_into_implicit_elem_match);
  RUN_TEST(test_regex_options_without_regex_fails);
  RUN_TEST(test_regex_options_with_unsupported_flag_fails);
  RUN_TEST(test_regex_options_non_string_fails);
  RUN_TEST(test_regex_options_with_compiled_regex_fails);
  RUN_TEST(test_regex_options_without_options_adapter_fails);
  mongory_cleanup();
  return UNITY_END();
}
