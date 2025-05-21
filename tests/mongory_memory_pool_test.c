#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include "unity.h"
#include <mongory-core.h>

static jmp_buf jmp;
mongory_memory_pool *pool;

void setUp(void) {
    pool = mongory_memory_pool_new();
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
    TEST_ASSERT_EQUAL(1024, pool->chunk_size);
}

void test_pool_allocation(void) {
    for (int i = 0; i < 1000; i++) {
        pool->alloc(pool, sizeof(char[6]));
    }
    TEST_ASSERT_GREATER_THAN(0, pool->chunk_size);
}

void test_string_allocation(void) {
    char *string = pool->alloc(pool, sizeof(char[6]));
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
        char *string = pool->alloc(pool, sizeof(char[6]));
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
    RUN_TEST(test_initial_pool_size);
    RUN_TEST(test_pool_allocation);
    RUN_TEST(test_string_allocation);
    RUN_TEST(test_double_free_prevention);
    return UNITY_END();
}
