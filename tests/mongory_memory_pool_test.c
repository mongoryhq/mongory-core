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

void test_initial_pool_size(void) {
  TEST_ASSERT_EQUAL(256, pool_ctx->chunk_size);
}

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
}

int main(void) {
  UNITY_BEGIN();
  mongory_init();
  RUN_TEST(test_initial_pool_size);
  RUN_TEST(test_pool_allocation);
  RUN_TEST(test_string_allocation);
  RUN_TEST(test_double_free_prevention);
  mongory_cleanup();
  return UNITY_END();
}
