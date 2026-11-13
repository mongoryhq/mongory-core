#include "../src/matchers/composite_matcher.h"
#include "../src/matchers/literal_matcher.h"
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

void test_trace_with_lazy_array_record_matcher_restores_delegate_matcher(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *condition = MG_TABLE_WRAP(pool, 1,
    "tags", mongory_value_wrap_s(pool, "ruby")
  );
  mongory_matcher *matcher = mongory_matcher_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(matcher);

  mongory_memory_pool *trace_pool = mongory_memory_pool_new();
  mongory_value *array_record = MG_TABLE_WRAP(trace_pool, 1,
    "tags", MG_ARRAY_WRAP(trace_pool, 2,
      mongory_value_wrap_s(trace_pool, "c"),
      mongory_value_wrap_s(trace_pool, "ruby")
    )
  );

  // First trace on an array record triggers lazy array_record_matcher creation
  TEST_ASSERT_TRUE(mongory_matcher_trace(matcher, array_record));
  trace_pool->free(trace_pool);

  // Subsequent normal match on a scalar record must use restored delegate_matcher
  mongory_value *scalar_record = MG_TABLE_WRAP(pool, 1,
    "tags", mongory_value_wrap_s(pool, "ruby")
  );
  mongory_field_matcher *field_m = (mongory_field_matcher *)matcher;
  TEST_ASSERT_NULL(field_m->literal.delegate_matcher->trace_stack);
  TEST_ASSERT_NULL(field_m->literal.array_record_matcher->trace_stack);
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher, scalar_record));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_table_cond_matcher);
  RUN_TEST(test_field_matcher_propagates_extern_ctx_to_lazy_array_matcher);
  RUN_TEST(test_trace_with_lazy_array_record_matcher_restores_delegate_matcher);
  return UNITY_END();
}
