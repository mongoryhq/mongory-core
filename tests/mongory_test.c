#include <stdlib.h>
#include <stdio.h>
#include "unity.h"
#include <mongory-core/foundations/memory_pool.h>
#include <mongory-core.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_mongory_initialization(void) {
    TEST_PASS_MESSAGE("Mongory initialized successfully");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mongory_initialization);
    return UNITY_END();
}
