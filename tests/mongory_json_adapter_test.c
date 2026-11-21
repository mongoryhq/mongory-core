#include "unity.h"
#include <mongory-core.h>
#include <mongory-core/adapters/json.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_match_json_basic_and_nested(void) {
  mongory_memory_pool *pool = mongory_memory_pool_new();
  const char *cond = "{\"status\":\"active\",\"age\":{\"$gte\":18,\"$lt\":65},\"profile\":{\"city\":\"taipei\"}}";
  mongory_matcher *m = mongory_matcher_new_from_json(pool, cond, strlen(cond), NULL);
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_NULL(pool->error);

  const char *rec_ok =
      "{\"id\":9007199254740993,\"status\":\"active\",\"age\":30,\"profile\":{\"city\":\"taipei\",\"zip\":\"100\"},\"unused\":{\"deep\":[1,2,3,4,5]}}";
  const char *rec_bad_city =
      "{\"status\":\"active\",\"age\":30,\"profile\":{\"city\":\"tokyo\"}}";
  const char *rec_bad_age =
      "{\"status\":\"active\",\"age\":15,\"profile\":{\"city\":\"taipei\"}}";

  TEST_ASSERT_TRUE(mongory_matcher_match_json(m, rec_ok, strlen(rec_ok)));
  TEST_ASSERT_FALSE(mongory_matcher_match_json(m, rec_bad_city, strlen(rec_bad_city)));
  TEST_ASSERT_FALSE(mongory_matcher_match_json(m, rec_bad_age, strlen(rec_bad_age)));

  pool->free(pool);
}

void test_match_json_arrays_and_operators(void) {
  mongory_memory_pool *pool = mongory_memory_pool_new();
  const char *cond =
      "{\"tags\":{\"$in\":[\"c\",\"go\"]},\"scores\":{\"$every\":{\"$gte\":60},\"$size\":3},"
      "\"items\":{\"$elemMatch\":{\"sku\":\"A1\",\"qty\":{\"$gt\":1}}}}";
  mongory_matcher *m = mongory_matcher_new_from_json(pool, cond, strlen(cond), NULL);
  TEST_ASSERT_NOT_NULL(m);

  const char *rec_ok =
      "{\"tags\":[\"ruby\",\"c\"],\"scores\":[60,85,100],\"items\":[{\"sku\":\"B2\",\"qty\":5},{\"sku\":\"A1\",\"qty\":2}]}";
  const char *rec_fail_every =
      "{\"tags\":[\"go\"],\"scores\":[59,85,100],\"items\":[{\"sku\":\"A1\",\"qty\":2}]}";
  const char *rec_fail_size =
      "{\"tags\":[\"go\"],\"scores\":[80,90],\"items\":[{\"sku\":\"A1\",\"qty\":2}]}";

  TEST_ASSERT_TRUE(mongory_matcher_match_json(m, rec_ok, strlen(rec_ok)));
  TEST_ASSERT_FALSE(mongory_matcher_match_json(m, rec_fail_every, strlen(rec_fail_every)));
  TEST_ASSERT_FALSE(mongory_matcher_match_json(m, rec_fail_size, strlen(rec_fail_size)));

  pool->free(pool);
}

void test_match_json_int64_precision_and_table_equality(void) {
  mongory_memory_pool *pool = mongory_memory_pool_new();
  // Snowflake-sized 64-bit integer (> 2^53) must match exact int64 without truncation.
  const char *cond = "{\"snowflake\":9007199254740995,\"meta\":{\"a\":1,\"b\":\"ok\"}}";
  mongory_matcher *m = mongory_matcher_new_from_json(pool, cond, strlen(cond), NULL);
  TEST_ASSERT_NOT_NULL(m);

  const char *rec_exact = "{\"snowflake\":9007199254740995,\"meta\":{\"b\":\"ok\",\"a\":1}}";
  const char *rec_off1 = "{\"snowflake\":9007199254740996,\"meta\":{\"a\":1,\"b\":\"ok\"}}";

  TEST_ASSERT_TRUE(mongory_matcher_match_json(m, rec_exact, strlen(rec_exact)));
  TEST_ASSERT_FALSE(mongory_matcher_match_json(m, rec_off1, strlen(rec_off1)));

  pool->free(pool);
}

void test_match_json_with_reusable_ctx_and_large_payload(void) {
  mongory_memory_pool *pool = mongory_memory_pool_new();
  const char *cond = "{\"target\":true}";
  mongory_matcher *m = mongory_matcher_new_from_json(pool, cond, strlen(cond), NULL);
  TEST_ASSERT_NOT_NULL(m);

  mongory_json_match_ctx *ctx = mongory_json_match_ctx_new();
  TEST_ASSERT_NOT_NULL(ctx);

  // Batch reuse across many small payloads
  for (int i = 0; i < 1000; i++) {
    const char *rec = (i % 2 == 0) ? "{\"target\":true,\"n\":1}" : "{\"target\":false,\"n\":2}";
    bool expected = (i % 2 == 0);
    TEST_ASSERT_EQUAL(expected, mongory_matcher_match_json_with_ctx(m, ctx, rec, strlen(rec)));
  }

  // Large JSON payload (> 16 KiB stack buffer, triggers buffer growth / heap fallback)
  size_t pad_len = 24000;
  char *large_json = (char *)malloc(pad_len + 64);
  TEST_ASSERT_NOT_NULL(large_json);
  memcpy(large_json, "{\"target\":true,\"pad\":\"", 22);
  memset(large_json + 22, 'x', pad_len);
  memcpy(large_json + 22 + pad_len, "\"}", 3);
  size_t total_len = 22 + pad_len + 2;

  TEST_ASSERT_TRUE(mongory_matcher_match_json(m, large_json, total_len));
  TEST_ASSERT_TRUE(mongory_matcher_match_json_with_ctx(m, ctx, large_json, total_len));

  free(large_json);
  mongory_json_match_ctx_free(ctx);
  pool->free(pool);
}

void test_match_json_invalid_inputs(void) {
  mongory_memory_pool *pool = mongory_memory_pool_new();

  // Invalid condition JSON sets MONGORY_ERROR_PARSE
  const char *bad_cond = "{bad json";
  TEST_ASSERT_NULL(mongory_matcher_new_from_json(pool, bad_cond, strlen(bad_cond), NULL));
  TEST_ASSERT_NOT_NULL(pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_PARSE, pool->error->type);

  pool->error = NULL;
  const char *ok_cond = "{\"a\":1}";
  mongory_matcher *m = mongory_matcher_new_from_json(pool, ok_cond, strlen(ok_cond), NULL);
  TEST_ASSERT_NOT_NULL(m);

  TEST_ASSERT_FALSE(mongory_matcher_match_json(m, "{invalid", 8));
  TEST_ASSERT_FALSE(mongory_matcher_match_json(m, "", 0));
  TEST_ASSERT_FALSE(mongory_matcher_match_json(m, NULL, 10));
  TEST_ASSERT_FALSE(mongory_matcher_match_json(NULL, "{\"a\":1}", 7));

  pool->free(pool);
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_match_json_basic_and_nested);
  RUN_TEST(test_match_json_arrays_and_operators);
  RUN_TEST(test_match_json_int64_precision_and_table_equality);
  RUN_TEST(test_match_json_with_reusable_ctx_and_large_payload);
  RUN_TEST(test_match_json_invalid_inputs);
  mongory_cleanup();
  return UNITY_END();
}
