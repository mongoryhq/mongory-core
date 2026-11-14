#include "../src/foundations/utils.h"
#include "../src/matchers/composite_matcher.h"
#include "../src/matchers/literal_matcher.h"
#include "../src/test_helper/test_helper.h"
#include "mongory-core.h"
#include "unity.h"
#include <limits.h>
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

void test_array_record_matcher_with_integer_index_condition(void) {
  mongory_memory_pool *pool = get_test_pool();

  // Case 1: {"tags": {"0": "ruby"}} against {"tags": ["ruby", "c"]}
  mongory_value *cond_index_0 = MG_TABLE_WRAP(pool, 1,
    "tags", MG_TABLE_WRAP(pool, 1, "0", mongory_value_wrap_s(pool, "ruby"))
  );
  mongory_matcher *matcher_0 = mongory_matcher_new(pool, cond_index_0, NULL);
  TEST_ASSERT_NOT_NULL(matcher_0);

  mongory_value *record_first_ruby = MG_TABLE_WRAP(pool, 1,
    "tags", MG_ARRAY_WRAP(pool, 2,
      mongory_value_wrap_s(pool, "ruby"),
      mongory_value_wrap_s(pool, "c")
    )
  );
  mongory_value *record_second_ruby = MG_TABLE_WRAP(pool, 1,
    "tags", MG_ARRAY_WRAP(pool, 2,
      mongory_value_wrap_s(pool, "c"),
      mongory_value_wrap_s(pool, "ruby")
    )
  );

  TEST_ASSERT_TRUE(mongory_matcher_match(matcher_0, record_first_ruby));
  TEST_ASSERT_FALSE(mongory_matcher_match(matcher_0, record_second_ruby));

  // Case 2: negative index {"tags": {"-1": "ruby"}}
  mongory_value *cond_index_neg1 = MG_TABLE_WRAP(pool, 1,
    "tags", MG_TABLE_WRAP(pool, 1, "-1", mongory_value_wrap_s(pool, "ruby"))
  );
  mongory_matcher *matcher_neg1 = mongory_matcher_new(pool, cond_index_neg1, NULL);
  TEST_ASSERT_NOT_NULL(matcher_neg1);

  TEST_ASSERT_TRUE(mongory_matcher_match(matcher_neg1, record_second_ruby));
  TEST_ASSERT_FALSE(mongory_matcher_match(matcher_neg1, record_first_ruby));

  // Case 3: nested object at array index {"users": {"0": {"name": "Alice"}}}
  mongory_value *cond_nested = MG_TABLE_WRAP(pool, 1,
    "users", MG_TABLE_WRAP(pool, 1,
      "0", MG_TABLE_WRAP(pool, 1, "name", mongory_value_wrap_s(pool, "Alice"))
    )
  );
  mongory_matcher *matcher_nested = mongory_matcher_new(pool, cond_nested, NULL);
  TEST_ASSERT_NOT_NULL(matcher_nested);

  mongory_value *users_alice_first = MG_TABLE_WRAP(pool, 1,
    "users", MG_ARRAY_WRAP(pool, 2,
      MG_TABLE_WRAP(pool, 1, "name", mongory_value_wrap_s(pool, "Alice")),
      MG_TABLE_WRAP(pool, 1, "name", mongory_value_wrap_s(pool, "Bob"))
    )
  );
  mongory_value *users_bob_first = MG_TABLE_WRAP(pool, 1,
    "users", MG_ARRAY_WRAP(pool, 2,
      MG_TABLE_WRAP(pool, 1, "name", mongory_value_wrap_s(pool, "Bob")),
      MG_TABLE_WRAP(pool, 1, "name", mongory_value_wrap_s(pool, "Alice"))
    )
  );

  TEST_ASSERT_TRUE(mongory_matcher_match(matcher_nested, users_alice_first));
  TEST_ASSERT_FALSE(mongory_matcher_match(matcher_nested, users_bob_first));

  // Case 4: Same condition {"tags": {"0": "ruby"}} against a Table record {"tags": {"0": "ruby"}}
  mongory_value *table_record = MG_TABLE_WRAP(pool, 1,
    "tags", MG_TABLE_WRAP(pool, 1, "0", mongory_value_wrap_s(pool, "ruby"))
  );
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher_0, table_record));

  // Case 5: Disallow leading '0' ("00", "01", "-0", "-01") and '+' ("+0", "+1") as integer indices
  int parsed = 999;
  TEST_ASSERT_TRUE(mongory_try_parse_int("0", &parsed));
  TEST_ASSERT_EQUAL_INT(0, parsed);
  TEST_ASSERT_TRUE(mongory_try_parse_int("1", &parsed));
  TEST_ASSERT_EQUAL_INT(1, parsed);
  TEST_ASSERT_TRUE(mongory_try_parse_int("-1", &parsed));
  TEST_ASSERT_EQUAL_INT(-1, parsed);
  TEST_ASSERT_TRUE(mongory_try_parse_int("2147483647", &parsed));
  TEST_ASSERT_EQUAL_INT(INT_MAX, parsed);
  TEST_ASSERT_TRUE(mongory_try_parse_int("-2147483648", &parsed));
  TEST_ASSERT_EQUAL_INT(INT_MIN, parsed);
  TEST_ASSERT_TRUE(mongory_try_parse_int("42", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int(NULL, NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("-", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("00", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("01", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("-0", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("-01", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("+0", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("+1", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int(" 1", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("12a", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("2147483648", NULL));
  TEST_ASSERT_FALSE(mongory_try_parse_int("-2147483649", NULL));

  // "01" is NOT an array index: does not match array index 1, but matches element table key "01"
  mongory_value *cond_leading_zero = MG_TABLE_WRAP(pool, 1,
    "tags", MG_TABLE_WRAP(pool, 1, "01", mongory_value_wrap_s(pool, "ruby"))
  );
  mongory_matcher *matcher_leading_zero = mongory_matcher_new(pool, cond_leading_zero, NULL);
  TEST_ASSERT_FALSE(mongory_matcher_match(matcher_leading_zero, record_second_ruby));

  mongory_value *record_elem_with_01 = MG_TABLE_WRAP(pool, 1,
    "tags", MG_ARRAY_WRAP(pool, 1,
      MG_TABLE_WRAP(pool, 1, "01", mongory_value_wrap_s(pool, "ruby"))
    )
  );
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher_leading_zero, record_elem_with_01));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_table_cond_matcher);
  RUN_TEST(test_field_matcher_propagates_extern_ctx_to_lazy_array_matcher);
  RUN_TEST(test_trace_with_lazy_array_record_matcher_restores_delegate_matcher);
  RUN_TEST(test_array_record_matcher_with_integer_index_condition);
  return UNITY_END();
}
