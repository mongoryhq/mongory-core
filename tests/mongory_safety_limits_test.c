#include "../src/foundations/string_buffer.h"
#include "unity.h"
#include <mongory-core.h>
#include <stdlib.h>
#include <string.h>

typedef struct tracking_allocator_state {
  size_t alloc_calls;
  size_t free_calls;
  size_t fail_on_call; // 0 = never fail, k > 0 = fail on the k-th allocation attempt
} tracking_allocator_state;

static void *tracking_alloc(size_t size, void *user_data) {
  tracking_allocator_state *state = (tracking_allocator_state *)user_data;
  if (state->fail_on_call > 0 && state->alloc_calls + 1 >= state->fail_on_call) {
    return NULL;
  }
  state->alloc_calls++;
  return calloc(1, size);
}

static void tracking_free(void *ptr, void *user_data) {
  tracking_allocator_state *state = (tracking_allocator_state *)user_data;
  if (ptr) {
    state->free_calls++;
    free(ptr);
  }
}

void setUp(void) {
  mongory_max_depth_set(MONGORY_DEFAULT_MAX_DEPTH);
  mongory_max_nodes_set(MONGORY_DEFAULT_MAX_NODES);
}

void tearDown(void) {
  mongory_max_depth_set(MONGORY_DEFAULT_MAX_DEPTH);
  mongory_max_nodes_set(MONGORY_DEFAULT_MAX_NODES);
}

void test_custom_allocator_tracks_alloc_and_free(void) {
  tracking_allocator_state state = {0, 0, 0};
  mongory_allocator alloc = {
    .alloc = tracking_alloc,
    .free = tracking_free,
    .user_data = &state,
  };
  mongory_memory_pool *pool = mongory_memory_pool_new_with_allocator(&alloc);
  TEST_ASSERT_NOT_NULL(pool);
  TEST_ASSERT_GREATER_THAN(0, (int)state.alloc_calls);

  void *p1 = MG_ALLOC(pool, 64);
  void *p2 = MG_ALLOC(pool, 4096);
  TEST_ASSERT_NOT_NULL(p1);
  TEST_ASSERT_NOT_NULL(p2);

  pool->free(pool);
  TEST_ASSERT_EQUAL_UINT(state.alloc_calls, state.free_calls);
}

void test_custom_allocator_failure_on_init(void) {
  for (size_t fail_at = 1; fail_at <= 4; fail_at++) {
    tracking_allocator_state state = {0, 0, fail_at};
    mongory_allocator alloc = {
      .alloc = tracking_alloc,
      .free = tracking_free,
      .user_data = &state,
    };
    mongory_memory_pool *pool = mongory_memory_pool_new_with_allocator(&alloc);
    TEST_ASSERT_NULL(pool);
    // All partially allocated blocks must be freed with zero leaks.
    TEST_ASSERT_EQUAL_UINT(state.alloc_calls, state.free_calls);
  }
}

void test_pool_chunk_cap_and_total_byte_limit(void) {
  mongory_memory_pool *pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);
  // Cap geometric doubling at 4096 bytes and total budget at 8192 bytes.
  mongory_memory_pool_set_limits(pool, 4096, 8192);

  // Initial chunk is 2048 bytes.
  TEST_ASSERT_EQUAL_UINT(2048, mongory_memory_pool_total_bytes(pool));

  // Allocate 3000 bytes -> triggers a 4096-byte chunk (total = 6144 <= 8192).
  void *p1 = MG_ALLOC(pool, 3000);
  TEST_ASSERT_NOT_NULL(p1);
  TEST_ASSERT_EQUAL_UINT(6144, mongory_memory_pool_total_bytes(pool));

  // Allocate another 3000 bytes -> would exceed 8192 total budget -> must fail cleanly.
  void *p2 = MG_ALLOC(pool, 3000);
  TEST_ASSERT_NULL(p2);
  TEST_ASSERT_EQUAL_PTR(&MONGORY_ALLOC_ERROR, pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_MEMORY, pool->error->type);

  pool->free(pool);
}

void test_pool_reset_then_larger_allocation_safe(void) {
  mongory_memory_pool *pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);

  // Grow pool with a 2048-byte chunk -> 4096-byte chunk.
  TEST_ASSERT_NOT_NULL(MG_ALLOC(pool, 2048));
  TEST_ASSERT_NOT_NULL(MG_ALLOC(pool, 1024));

  pool->reset(pool);

  // Requesting 8192 bytes after reset must NOT overflow the 2048 or 4096 chunks.
  char *big = (char *)MG_ALLOC(pool, 8192);
  TEST_ASSERT_NOT_NULL(big);
  memset(big, 0x5A, 8192);
  TEST_ASSERT_EQUAL_HEX8(0x5A, big[0]);
  TEST_ASSERT_EQUAL_HEX8(0x5A, big[8191]);

  pool->free(pool);
}

void test_string_buffer_oom_safety(void) {
  mongory_memory_pool *pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);
  // Allow only the initial 2048-byte chunk.
  mongory_memory_pool_set_limits(pool, 2048, 2048);

  mongory_string_buffer *buf = mongory_string_buffer_new(pool);
  TEST_ASSERT_NOT_NULL(buf);

  // Repeatedly append until pool hits its 2048-byte limit; must never crash.
  for (int i = 0; i < 200; i++) {
    mongory_string_buffer_append(buf, "0123456789abcdef");
    mongory_string_buffer_appendf(buf, ":%d", i);
  }
  TEST_ASSERT_NOT_NULL(pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_MEMORY, pool->error->type);
  TEST_ASSERT_NOT_NULL(mongory_string_buffer_cstr(buf));

  // Clear under OOM must also not crash.
  mongory_string_buffer_clear(buf);
  TEST_ASSERT_NOT_NULL(mongory_string_buffer_cstr(buf));

  pool->free(pool);
}

void test_matcher_compile_oom_sweep(void) {
  // Build a non-trivial condition in an unconstrained pool.
  mongory_memory_pool *src_pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(src_pool);

  mongory_value *in_arr = MG_ARRAY_WRAP(src_pool, 3,
    mongory_value_wrap_s(src_pool, "a"),
    mongory_value_wrap_s(src_pool, "b"),
    mongory_value_wrap_s(src_pool, "c")
  );
  mongory_value *cond = MG_TABLE_WRAP(src_pool, 4,
    "age", MG_TABLE_WRAP(src_pool, 2, "$gte", mongory_value_wrap_i(src_pool, 18), "$lt", mongory_value_wrap_i(src_pool, 65)),
    "tag", MG_TABLE_WRAP(src_pool, 1, "$in", in_arr),
    "active", MG_TABLE_WRAP(src_pool, 1, "$exists", mongory_value_wrap_b(src_pool, true)),
    "$not", MG_TABLE_WRAP(src_pool, 1, "banned", mongory_value_wrap_b(src_pool, true))
  );

  // Sweep tight byte budgets to hit OOM at different internal allocation points.
  for (size_t budget = 2048; budget <= 8192; budget += 256) {
    mongory_memory_pool *pool = mongory_memory_pool_new();
    TEST_ASSERT_NOT_NULL(pool);
    // Exhaust most of the initial 2048-byte chunk first so OOM happens early.
    (void)MG_ALLOC(pool, 1900);
    mongory_memory_pool_set_limits(pool, 512, budget);

    mongory_matcher *m = mongory_matcher_new(pool, cond, NULL);
    if (m == NULL) {
      TEST_ASSERT_NOT_NULL(pool->error);
      TEST_ASSERT_EQUAL(MONGORY_ERROR_MEMORY, pool->error->type);
    } else {
      mongory_memory_pool *eval_pool = mongory_memory_pool_new();
      mongory_memory_pool_set_limits(eval_pool, 2048, 2048);
      (void)MG_ALLOC(eval_pool, 1900);
      (void)mongory_matcher_explain_tree(m, eval_pool);
      eval_pool->free(eval_pool);
    }
    pool->free(pool);
  }

  src_pool->free(src_pool);
}

static mongory_value *build_nested_condition(mongory_memory_pool *pool, int depth) {
  mongory_value *cur = mongory_value_wrap_i(pool, 42);
  for (int i = 0; i < depth; i++) {
    mongory_table *tbl = mongory_table_new(pool);
    TEST_ASSERT_NOT_NULL(tbl);
    tbl->set(tbl, "x", cur);
    cur = mongory_value_wrap_t(pool, tbl);
  }
  return cur;
}

void test_max_depth_limit(void) {
  mongory_memory_pool *pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);

  mongory_max_depth_set(8);
  mongory_value *ok_cond = build_nested_condition(pool, 8);
  mongory_matcher *ok_m = mongory_matcher_new(pool, ok_cond, NULL);
  TEST_ASSERT_NOT_NULL(ok_m);
  TEST_ASSERT_NULL(pool->error);

  mongory_value *deep_cond = build_nested_condition(pool, 9);
  mongory_matcher *deep_m = mongory_matcher_new(pool, deep_cond, NULL);
  TEST_ASSERT_NULL(deep_m);
  TEST_ASSERT_EQUAL_PTR(&MONGORY_DEPTH_LIMIT_ERROR, pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_LIMIT_EXCEEDED, pool->error->type);

  // Disabling limit (0) allows deeper trees.
  pool->error = NULL;
  mongory_max_depth_set(0);
  mongory_matcher *unlimited_m = mongory_matcher_new(pool, deep_cond, NULL);
  TEST_ASSERT_NOT_NULL(unlimited_m);
  TEST_ASSERT_NULL(pool->error);

  pool->free(pool);
}

void test_max_nodes_limit_and_large_in_counts_as_one(void) {
  mongory_memory_pool *pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(pool);

  mongory_max_nodes_set(8);

  // A single $in with 100 elements should count as 1 leaf node (+ 1 Field node = 2 nodes <= 8).
  mongory_array *arr = mongory_array_new(pool);
  for (int i = 0; i < 100; i++) {
    arr->push(arr, mongory_value_wrap_i(pool, i));
  }
  mongory_value *in_cond = MG_TABLE_WRAP(pool, 1, "x", MG_TABLE_WRAP(pool, 1, "$in", mongory_value_wrap_a(pool, arr)));
  mongory_matcher *in_m = mongory_matcher_new(pool, in_cond, NULL);
  TEST_ASSERT_NOT_NULL(in_m);
  TEST_ASSERT_NULL(pool->error);

  // A table with 10 fields creates > 20 nodes -> must fail with MONGORY_NODE_LIMIT_ERROR.
  mongory_table *wide = mongory_table_new(pool);
  char key[16];
  for (int i = 0; i < 10; i++) {
    snprintf(key, sizeof(key), "f%d", i);
    wide->set(wide, key, mongory_value_wrap_i(pool, i));
  }
  mongory_matcher *wide_m = mongory_matcher_new(pool, mongory_value_wrap_t(pool, wide), NULL);
  TEST_ASSERT_NULL(wide_m);
  TEST_ASSERT_EQUAL_PTR(&MONGORY_NODE_LIMIT_ERROR, pool->error);
  TEST_ASSERT_EQUAL(MONGORY_ERROR_LIMIT_EXCEEDED, pool->error->type);

  pool->free(pool);
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_custom_allocator_tracks_alloc_and_free);
  RUN_TEST(test_custom_allocator_failure_on_init);
  RUN_TEST(test_pool_chunk_cap_and_total_byte_limit);
  RUN_TEST(test_pool_reset_then_larger_allocation_safe);
  RUN_TEST(test_string_buffer_oom_safety);
  RUN_TEST(test_matcher_compile_oom_sweep);
  RUN_TEST(test_max_depth_limit);
  RUN_TEST(test_max_nodes_limit_and_large_in_counts_as_one);
  mongory_cleanup();
  return UNITY_END();
}
