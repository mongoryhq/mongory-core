#include "../src/foundations/memory_pool.c"
#include "unity.h"
#include <mongory-core.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static jmp_buf jmp;
mongory_memory_pool *pool;
mongory_memory_pool_ctx *pool_ctx;

void setUp(void) {
  pool = mongory_memory_pool_new();
  pool_ctx = (mongory_memory_pool_ctx *)pool->ctx;
  TEST_ASSERT_NOT_NULL(pool_ctx);
}

void tearDown(void) {
  if (pool != NULL) {
    pool->free(pool);
    pool = NULL;
  }
}

void signal_handler(int sig) {
  (void)sig; // Suppress unused parameter warning
  longjmp(jmp, 1);
}

void test_initial_pool_size(void) { TEST_ASSERT_EQUAL(2048, pool_ctx->chunk_size); }

void test_pool_allocation(void) {
  for (int i = 0; i < 1000; i++) {
    pool->alloc(pool->ctx, sizeof(char[6]));
  }
  TEST_ASSERT_GREATER_THAN(0, pool_ctx->chunk_size);
}

void test_string_allocation(void) {
  char *string = pool->alloc(pool->ctx, sizeof(char[6]));
  string[0] = 'a';
  string[1] = 'b';
  string[2] = 'c';
  string[3] = 'd';
  string[4] = '\0';
  TEST_ASSERT_EQUAL_STRING("abcd", string);
}

void test_double_free_prevention(void) {
#ifdef _WIN32
  // Skip this test on Windows due to signal handling differences
  TEST_PASS_MESSAGE("Double free test skipped on Windows");
#else
  signal(SIGABRT, signal_handler);

  if (setjmp(jmp) == 0) {
    char *string = pool->alloc(pool->ctx, sizeof(char[6]));
    pool->free(pool);
    pool = NULL; // Prevent tearDown from freeing the pool again
    free(string);
    TEST_FAIL_MESSAGE("Double free should have been prevented");
  } else {
    TEST_PASS_MESSAGE("Double free was caught as expected");
  }
#endif
}

void test_reset_resets_used_and_current_to_head(void) {
  // Force pool to grow to multiple chunks
  (void)pool->alloc(pool->ctx, 2048);
  (void)pool->alloc(pool->ctx, 16);

  TEST_ASSERT_NOT_NULL(pool_ctx->head->next);
  TEST_ASSERT_NOT_EQUAL(pool_ctx->head, pool_ctx->current);

  pool->reset(pool->ctx);

  // current 指回 head，並且所有節點 used 歸零
  TEST_ASSERT_EQUAL_PTR(pool_ctx->head, pool_ctx->current);
  mongory_memory_node *node = pool_ctx->head;
  while (node) {
    TEST_ASSERT_EQUAL(0, (int)node->used);
    node = node->next;
  }
}

void test_reset_does_not_change_chunk_size_or_nodes(void) {
  // 讓 pool 成長並記錄節點狀態
  (void)pool->alloc(pool->ctx, 2048);
  (void)pool->alloc(pool->ctx, 16);

  size_t prev_chunk_size = pool_ctx->chunk_size;
  mongory_memory_node *head_before = pool_ctx->head;
  int node_count_before = 0;
  for (mongory_memory_node *n = pool_ctx->head; n; n = n->next)
    node_count_before++;

  pool->reset(pool->ctx);

  // chunk_size 與節點鏈結不應改變
  TEST_ASSERT_EQUAL(prev_chunk_size, pool_ctx->chunk_size);
  TEST_ASSERT_EQUAL_PTR(head_before, pool_ctx->head);
  int node_count_after = 0;
  for (mongory_memory_node *n = pool_ctx->head; n; n = n->next)
    node_count_after++;
  TEST_ASSERT_EQUAL(node_count_before, node_count_after);
}

void test_reset_allows_reuse_from_start(void) {
  // 先使用一些空間
  (void)pool->alloc(pool->ctx, 128);
  pool->reset(pool->ctx);

  // 重設後再次配置，應從 head->ptr 起始位置配置
  void *p = pool->alloc(pool->ctx, 16);
  TEST_ASSERT_EQUAL_PTR(pool_ctx->head->ptr, p);
  TEST_ASSERT_EQUAL(16 + (8 - (16 % 8)) % 8, (int)pool_ctx->head->used);
}

void test_multiple_resets_are_idempotent(void) {
  (void)pool->alloc(pool->ctx, 64);
  pool->reset(pool->ctx);
  mongory_memory_node *head_after_first = pool_ctx->head;
  size_t used_after_first = pool_ctx->head->used;

  pool->reset(pool->ctx);
  TEST_ASSERT_EQUAL_PTR(head_after_first, pool_ctx->head);
  TEST_ASSERT_EQUAL(used_after_first, pool_ctx->head->used);
}

void test_reset_does_not_clear_traced_memory(void) {
  // 建立外部配置並追蹤
  char *ext = (char *)malloc(32);
  TEST_ASSERT_NOT_NULL(ext);
  pool->trace(pool->ctx, ext, 32);
  TEST_ASSERT_NOT_NULL(pool_ctx->extra);

  mongory_memory_node *extra_before = pool_ctx->extra;
  pool->reset(pool->ctx);

  // reset 不應清除 extra 的追蹤
  TEST_ASSERT_EQUAL_PTR(extra_before, pool_ctx->extra);
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_initial_pool_size);
  RUN_TEST(test_pool_allocation);
  RUN_TEST(test_string_allocation);
  RUN_TEST(test_double_free_prevention);
  RUN_TEST(test_reset_resets_used_and_current_to_head);
  RUN_TEST(test_reset_does_not_change_chunk_size_or_nodes);
  RUN_TEST(test_reset_allows_reuse_from_start);
  RUN_TEST(test_multiple_resets_are_idempotent);
  RUN_TEST(test_reset_does_not_clear_traced_memory);
  mongory_cleanup();
  return UNITY_END();
}
