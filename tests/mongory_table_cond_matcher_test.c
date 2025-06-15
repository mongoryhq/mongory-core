#include "mongory-core.h"
#include "unity.h"
#include "../src/matchers/base_matcher.h"
#include "../src/matchers/composite_matcher.h"
#include "../src/test_helper/test_helper.h"

mongory_array *test_cases;

void setUp(void) {
  setup_test_environment();
  mongory_value *parsed = json_to_value_from_file(get_test_pool(), "tests/jsons/table_condition_test.json");
  TEST_ASSERT_NOT_NULL(parsed);
  test_cases = parsed->data.a;
  TEST_ASSERT_NOT_NULL(test_cases);
}

void tearDown(void) {
  teardown_test_environment();
}

void test_table_cond_matcher(void) {
  test_cases->each(test_cases, mongory_matcher_table_cond_new, execute_test_case);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_table_cond_matcher);
  return UNITY_END();
}
