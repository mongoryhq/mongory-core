#include "../src/matchers/composite_matcher.h"
#include "../src/test_helper/test_helper.h"
#include "mongory-core.h"
#include "unity.h"

void setUp(void) { setup_test_environment(); }

void tearDown(void) { teardown_test_environment(); }

void test_or_matcher(void) { execute_test_case("tests/jsons/or_matcher_test.json", mongory_matcher_or_new); }

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_or_matcher);
  return UNITY_END();
}