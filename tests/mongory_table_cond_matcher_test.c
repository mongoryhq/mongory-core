#include "../src/matchers/composite_matcher.h"
#include "../src/test_helper/test_helper.h"
#include "mongory-core.h"
#include "unity.h"
#include <string.h>

void setUp(void) { setup_test_environment(); }

void tearDown(void) { teardown_test_environment(); }

void test_table_cond_matcher(void) {
  mongory_test_context context = {mongory_matcher_table_cond_new, false, false, false};
  execute_test_case("tests/jsons/table_condition_matcher_test.json", &context);
}

static void *captured_extern_ctx = NULL;

static bool custom_ctx_lookup(char *key) {
  return strcmp(key, "$ctxCheck") == 0;
}

static mongory_matcher_custom_context *custom_ctx_build(char *key, mongory_value *condition, void *extern_ctx) {
  (void)key;
  captured_extern_ctx = extern_ctx;
  mongory_matcher_custom_context *ctx = MG_ALLOC_PTR(condition->pool, mongory_matcher_custom_context);
  ctx->name = "CtxCheck";
  ctx->external_matcher = extern_ctx;
  return ctx;
}

static bool custom_ctx_match(void *external_matcher, mongory_value *value) {
  (void)value;
  return external_matcher != NULL;
}

void test_field_matcher_propagates_extern_ctx_to_lazy_array_matcher(void) {
  mongory_memory_pool *pool = get_test_pool();
  int dummy_ctx = 12345;
  captured_extern_ctx = NULL;

  mongory_custom_matcher_lookup_func_set(custom_ctx_lookup);
  mongory_custom_matcher_build_func_set(custom_ctx_build);
  mongory_custom_matcher_match_func_set(custom_ctx_match);

  mongory_value *condition = MG_TABLE_WRAP(pool, 1,
    "tags", MG_TABLE_WRAP(pool, 1, "$ctxCheck", mongory_value_wrap_b(pool, true))
  );
  mongory_matcher *matcher = mongory_matcher_new(pool, condition, &dummy_ctx);
  TEST_ASSERT_NOT_NULL(matcher);

  captured_extern_ctx = NULL;
  mongory_value *array_record = MG_TABLE_WRAP(pool, 1,
    "tags", MG_ARRAY_WRAP(pool, 1, mongory_value_wrap_s(pool, "ruby"))
  );
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher, array_record));
  TEST_ASSERT_EQUAL_PTR(&dummy_ctx, captured_extern_ctx);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_table_cond_matcher);
  RUN_TEST(test_field_matcher_propagates_extern_ctx_to_lazy_array_matcher);
  return UNITY_END();
}
