#ifndef MONGORY_TEST_HELPER_H
#define MONGORY_TEST_HELPER_H

#include "../src/foundations/config_private.h"
#include <cjson/cJSON.h>
#include <mongory-core.h>

// JSON convert functions
mongory_value *json_string_to_mongory_value(mongory_memory_pool *pool, const char *json);
mongory_value *json_to_value_from_file(mongory_memory_pool *pool, const char *filename);
mongory_value *cjson_to_mongory_value_deep_convert(mongory_memory_pool *pool, cJSON *root);
mongory_value *cjson_to_mongory_value_shallow_convert(mongory_memory_pool *pool, cJSON *root);

// Test helper functions
void setup_test_environment(void);
void teardown_test_environment(void);
mongory_memory_pool *get_test_pool(void);

typedef struct mongory_test_context {
  mongory_matcher_build_func matcher_build_func;
  bool enable_trace;
  bool enable_explain;
  bool show_progress;
} mongory_test_context;

void execute_test_case(char *file_name, mongory_test_context *context);

// Assertion helper functions
void assert_value_equals(mongory_value *expected, mongory_value *actual);
void assert_value_not_equals(mongory_value *expected, mongory_value *actual);
void assert_value_is_null(mongory_value *value);
void assert_value_is_not_null(mongory_value *value);

#endif // MONGORY_TEST_HELPER_H
