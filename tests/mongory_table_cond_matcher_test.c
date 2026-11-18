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

void test_array_record_matcher_mongodb_comparison_semantics(void) {
  mongory_memory_pool *pool = get_test_pool();

  mongory_value *record_scores = MG_TABLE_WRAP(pool, 1,
    "scores", MG_ARRAY_WRAP(pool, 3,
      mongory_value_wrap_i(pool, 3),
      mongory_value_wrap_i(pool, 6),
      mongory_value_wrap_i(pool, 9)
    )
  );
  mongory_value *record_low_scores = MG_TABLE_WRAP(pool, 1,
    "scores", MG_ARRAY_WRAP(pool, 3,
      mongory_value_wrap_i(pool, 1),
      mongory_value_wrap_i(pool, 2),
      mongory_value_wrap_i(pool, 3)
    )
  );

  // {"scores": {"$gt": 5}} -> matches if any element > 5
  mongory_value *cond_gt = MG_TABLE_WRAP(pool, 1,
    "scores", MG_TABLE_WRAP(pool, 1, "$gt", mongory_value_wrap_i(pool, 5))
  );
  mongory_matcher *matcher_gt = mongory_matcher_new(pool, cond_gt, NULL);
  TEST_ASSERT_NOT_NULL(matcher_gt);
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher_gt, record_scores));
  TEST_ASSERT_FALSE(mongory_matcher_match(matcher_gt, record_low_scores));

  // {"scores": {"$gt": 5, "$lt": 8}} -> element-level implicit $elemMatch (6 satisfies both > 5 and < 8)
  mongory_value *cond_gt_lt = MG_TABLE_WRAP(pool, 1,
    "scores", MG_TABLE_WRAP(pool, 2,
      "$gt", mongory_value_wrap_i(pool, 5),
      "$lt", mongory_value_wrap_i(pool, 8)
    )
  );
  mongory_matcher *matcher_gt_lt = mongory_matcher_new(pool, cond_gt_lt, NULL);
  TEST_ASSERT_NOT_NULL(matcher_gt_lt);
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher_gt_lt, record_scores));

  // {"scores": {"$ne": 6}} -> false if any element is 6, true if no element is 6
  mongory_value *cond_ne = MG_TABLE_WRAP(pool, 1,
    "scores", MG_TABLE_WRAP(pool, 1, "$ne", mongory_value_wrap_i(pool, 6))
  );
  mongory_matcher *matcher_ne = mongory_matcher_new(pool, cond_ne, NULL);
  TEST_ASSERT_NOT_NULL(matcher_ne);
  TEST_ASSERT_FALSE(mongory_matcher_match(matcher_ne, record_scores));
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher_ne, record_low_scores));

  // {"scores": {"$eq": 6}} -> true if any element is 6
  mongory_value *cond_eq_scalar = MG_TABLE_WRAP(pool, 1,
    "scores", MG_TABLE_WRAP(pool, 1, "$eq", mongory_value_wrap_i(pool, 6))
  );
  mongory_matcher *matcher_eq_scalar = mongory_matcher_new(pool, cond_eq_scalar, NULL);
  TEST_ASSERT_NOT_NULL(matcher_eq_scalar);
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher_eq_scalar, record_scores));
  TEST_ASSERT_FALSE(mongory_matcher_match(matcher_eq_scalar, record_low_scores));

  // {"scores": {"$eq": [1, 2, 3]}} -> matches whole array [1, 2, 3] or nested [[1, 2, 3], 9]
  mongory_value *cond_eq_arr = MG_TABLE_WRAP(pool, 1,
    "scores", MG_TABLE_WRAP(pool, 1, "$eq", MG_ARRAY_WRAP(pool, 3,
      mongory_value_wrap_i(pool, 1),
      mongory_value_wrap_i(pool, 2),
      mongory_value_wrap_i(pool, 3)
    ))
  );
  mongory_matcher *matcher_eq_arr = mongory_matcher_new(pool, cond_eq_arr, NULL);
  TEST_ASSERT_NOT_NULL(matcher_eq_arr);
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher_eq_arr, record_low_scores));
  TEST_ASSERT_FALSE(mongory_matcher_match(matcher_eq_arr, record_scores));
}

static void assert_unknown_operator(mongory_value *condition) {
  mongory_memory_pool *pool = get_test_pool();
  pool->error = NULL;
  TEST_ASSERT_NULL(mongory_matcher_new(pool, condition, NULL));
  TEST_ASSERT_NOT_NULL(pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_INVALID_ARGUMENT, pool->error->type);
  TEST_ASSERT_EQUAL_STRING("Unknown operator: $foo", pool->error->message);
  pool->error = NULL;
}

void test_unknown_operator_fails_at_any_depth(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *foo = MG_TABLE_WRAP(pool, 1, "$foo", mongory_value_wrap_i(pool, 1));

  assert_unknown_operator(foo);
  assert_unknown_operator(MG_TABLE_WRAP(pool, 1, "age", foo));
  assert_unknown_operator(MG_TABLE_WRAP(pool, 1, "$and", MG_ARRAY_WRAP(pool, 1, MG_TABLE_WRAP(pool, 1, "age", foo))));
  assert_unknown_operator(MG_TABLE_WRAP(pool, 1, "$or", MG_ARRAY_WRAP(pool, 1, foo)));
  assert_unknown_operator(MG_TABLE_WRAP(pool, 1, "items", MG_TABLE_WRAP(pool, 1, "$elemMatch", foo)));
  assert_unknown_operator(MG_TABLE_WRAP(pool, 1, "items", MG_TABLE_WRAP(pool, 1, "$every", foo)));
  assert_unknown_operator(MG_TABLE_WRAP(pool, 1, "age", MG_TABLE_WRAP(pool, 1, "$not", foo)));
  assert_unknown_operator(MG_TABLE_WRAP(pool, 1, "tags",
    MG_TABLE_WRAP(pool, 2, "$size", mongory_value_wrap_i(pool, 1), "$foo", mongory_value_wrap_i(pool, 1))));
}

void test_dollar_inside_field_name_is_a_field(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *condition = MG_TABLE_WRAP(pool, 1, "price$", mongory_value_wrap_i(pool, 1));
  mongory_matcher *matcher = mongory_matcher_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(pool->error);
  TEST_ASSERT_TRUE(mongory_matcher_match(matcher, MG_TABLE_WRAP(pool, 1, "price$", mongory_value_wrap_i(pool, 1))));
}

void test_lazy_array_record_matcher_updates_ancestor_priorities(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *condition = MG_TABLE_WRAP(pool, 3,
    "scores", MG_TABLE_WRAP(pool, 2,
      "$size", MG_TABLE_WRAP(pool, 2,
        "$gte", mongory_value_wrap_i(pool, 3),
        "$lte", mongory_value_wrap_i(pool, 5)
      ),
      "$every", MG_TABLE_WRAP(pool, 1, "$gte", mongory_value_wrap_i(pool, 60))
    ),
    "tags", mongory_value_wrap_s(pool, "ts"),
    "reviews", MG_TABLE_WRAP(pool, 2,
      "approved", mongory_value_wrap_b(pool, true),
      "rating", MG_TABLE_WRAP(pool, 1, "$gte", mongory_value_wrap_i(pool, 4))
    )
  );
  mongory_matcher *root = mongory_matcher_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(root);

  mongory_composite_matcher *comp = (mongory_composite_matcher *)root;
  double root_before = root->priority;
  mongory_field_matcher *tags_m = NULL;
  mongory_field_matcher *reviews_m = NULL;
  mongory_field_matcher *scores_m = NULL;
  for (size_t i = 0; i < comp->children->count; i++) {
    mongory_field_matcher *fm = (mongory_field_matcher *)comp->children->get(comp->children, i);
    if (strcmp(fm->field, "tags") == 0) tags_m = fm;
    else if (strcmp(fm->field, "reviews") == 0) reviews_m = fm;
    else if (strcmp(fm->field, "scores") == 0) scores_m = fm;
  }
  TEST_ASSERT_NOT_NULL(tags_m);
  TEST_ASSERT_NOT_NULL(reviews_m);
  TEST_ASSERT_NOT_NULL(scores_m);

  double tags_before = tags_m->literal.base.priority;
  double reviews_before = reviews_m->literal.base.priority;
  double scores_before = scores_m->literal.base.priority;

  mongory_value *record = MG_TABLE_WRAP(pool, 4,
    "candidate", mongory_value_wrap_s(pool, "Alice"),
    "tags", MG_ARRAY_WRAP(pool, 3,
      mongory_value_wrap_s(pool, "go"),
      mongory_value_wrap_s(pool, "ts"),
      mongory_value_wrap_s(pool, "rust")
    ),
    "scores", MG_ARRAY_WRAP(pool, 3,
      mongory_value_wrap_i(pool, 82),
      mongory_value_wrap_i(pool, 91),
      mongory_value_wrap_i(pool, 77)
    ),
    "reviews", MG_ARRAY_WRAP(pool, 3,
      MG_TABLE_WRAP(pool, 3, "reviewer", mongory_value_wrap_s(pool, "r1"), "approved", mongory_value_wrap_b(pool, false), "rating", mongory_value_wrap_i(pool, 5)),
      MG_TABLE_WRAP(pool, 3, "reviewer", mongory_value_wrap_s(pool, "r2"), "approved", mongory_value_wrap_b(pool, true), "rating", mongory_value_wrap_i(pool, 5)),
      MG_TABLE_WRAP(pool, 3, "reviewer", mongory_value_wrap_s(pool, "r3"), "approved", mongory_value_wrap_b(pool, true), "rating", mongory_value_wrap_i(pool, 4))
    )
  );
  TEST_ASSERT_TRUE(mongory_matcher_match(root, record));

  TEST_ASSERT_NOT_NULL(tags_m->literal.array_record_matcher);
  TEST_ASSERT_NOT_NULL(reviews_m->literal.array_record_matcher);
  TEST_ASSERT_EQUAL_FLOAT((float)(1.0 + tags_m->literal.array_record_matcher->priority), (float)tags_m->literal.base.priority);
  TEST_ASSERT_EQUAL_FLOAT((float)(1.0 + reviews_m->literal.array_record_matcher->priority), (float)reviews_m->literal.base.priority);
  TEST_ASSERT_TRUE(tags_m->literal.base.priority > tags_before);
  TEST_ASSERT_TRUE(reviews_m->literal.base.priority > reviews_before);
  TEST_ASSERT_TRUE(root->priority > root_before);
  TEST_ASSERT_EQUAL_FLOAT(
    (float)(2.0 + tags_m->literal.base.priority + reviews_m->literal.base.priority + scores_m->literal.base.priority),
    (float)root->priority
  );
  TEST_ASSERT_EQUAL_FLOAT((float)scores_before, (float)scores_m->literal.base.priority);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_table_cond_matcher);
  RUN_TEST(test_field_matcher_propagates_extern_ctx_to_lazy_array_matcher);
  RUN_TEST(test_trace_with_lazy_array_record_matcher_restores_delegate_matcher);
  RUN_TEST(test_array_record_matcher_with_integer_index_condition);
  RUN_TEST(test_array_record_matcher_mongodb_comparison_semantics);
  RUN_TEST(test_unknown_operator_fails_at_any_depth);
  RUN_TEST(test_dollar_inside_field_name_is_a_field);
  RUN_TEST(test_lazy_array_record_matcher_updates_ancestor_priorities);
  return UNITY_END();
}
