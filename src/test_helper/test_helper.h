#ifndef MONGORY_TEST_HELPER_H
#define MONGORY_TEST_HELPER_H

#include <mongory-core.h>
#include <cjson/cJSON.h>

// JSON 轉換相關函數
mongory_value *json_to_value(mongory_memory_pool *pool, const char *json);
mongory_value *json_to_value_from_file(mongory_memory_pool *pool, const char *filename);

// 測試輔助函數
void setup_test_environment(void);
void teardown_test_environment(void);
mongory_memory_pool *get_test_pool(void);

// 斷言輔助函數
void assert_value_equals(mongory_value *expected, mongory_value *actual);
void assert_value_not_equals(mongory_value *expected, mongory_value *actual);
void assert_value_is_null(mongory_value *value);
void assert_value_is_not_null(mongory_value *value);

#endif // MONGORY_TEST_HELPER_H 