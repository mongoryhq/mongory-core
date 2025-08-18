#include "../src/matchers/composite_matcher.h"
#include "../src/test_helper/test_helper.h"
#include "mongory-core.h"
#include "unity.h"

void setUp(void) { setup_test_environment(); }

void tearDown(void) { teardown_test_environment(); }

void test_table_cond_matcher(void) {
  mongory_test_context context = {mongory_matcher_table_cond_new, false, false, false};
  execute_test_case("tests/jsons/table_condition_matcher_test.json", &context);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_table_cond_matcher);
  return UNITY_END();
}
