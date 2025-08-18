#ifndef MONGORY_TEST_HELPER_C
#define MONGORY_TEST_HELPER_C

#include "test_helper.h"
#include "../../src/matchers/base_matcher.h"
#include "../../tests/unity/unity.h"
#include <cjson/cJSON.h>
#include <mongory-core.h>
#include <mongory-core/foundations/memory_pool.h>
#include <mongory-core/foundations/value.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../foundations/string_buffer.h"

static mongory_memory_pool *test_pool = NULL;

static inline mongory_value *
cjson_to_mongory_value_convert_recursive(mongory_memory_pool *pool, cJSON *root,
                                         mongory_value *(*convert_func)(mongory_memory_pool *pool, cJSON *root)) {
  if (!root) {
    return NULL;
  }

  mongory_value *value = NULL;
  mongory_array *array = NULL;
  mongory_table *table = NULL;

  switch (root->type) {
  case cJSON_Array:
    array = mongory_array_new(pool);
    value = mongory_value_wrap_a(pool, array);
    for (cJSON *item = root->child; item; item = item->next) {
      array->push(array, convert_func(pool, item));
    }
    break;
  case cJSON_Object:
    table = mongory_table_new(pool);
    value = mongory_value_wrap_t(pool, table);
    for (cJSON *item = root->child; item; item = item->next) {
      table->set(table, item->string, convert_func(pool, item));
    }
    break;
  case cJSON_String: {
    value = mongory_value_wrap_s(pool, root->valuestring);
    break;
  }
  case cJSON_Number:
    if (root->valuedouble == (double)root->valueint) {
      value = mongory_value_wrap_i(pool, root->valueint);
    } else {
      value = mongory_value_wrap_d(pool, root->valuedouble);
    }
    break;
  case cJSON_True:
    value = mongory_value_wrap_b(pool, true);
    break;
  case cJSON_False:
    value = mongory_value_wrap_b(pool, false);
    break;
  case cJSON_NULL:
    value = mongory_value_wrap_n(pool, NULL);
    break;
  default:
    fprintf(stderr, "Unsupported JSON type: %d\n", root->type);
    break;
  }
  return value;
}

mongory_value *cjson_to_mongory_value_deep_convert(mongory_memory_pool *pool, cJSON *root) {
  return cjson_to_mongory_value_convert_recursive(pool, root, cjson_to_mongory_value_deep_convert);
}

static inline mongory_value *cjson_to_mongory_value_ptr(mongory_memory_pool *pool, cJSON *root) {
  return mongory_value_wrap_ptr(pool, root);
}

mongory_value *cjson_to_mongory_value_shallow_convert(mongory_memory_pool *pool, cJSON *root) {
  return cjson_to_mongory_value_convert_recursive(pool, root, cjson_to_mongory_value_ptr);
}

mongory_value *json_string_to_mongory_value(mongory_memory_pool *pool, const char *json) {
  if (!json) {
    return NULL;
  }

  cJSON *root = cJSON_Parse(json);
  if (!root) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr) {
      fprintf(stderr, "JSON Parse Error: %s\n", error_ptr);
    }
    return NULL;
  }

  mongory_value *value = cjson_to_mongory_value_deep_convert(pool, root);
  cJSON_Delete(root);
  return value;
}

mongory_value *json_to_mongory_value_from_file(mongory_memory_pool *pool, const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    fprintf(stderr, "Failed to open file: %s\n", filename);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *json = calloc(1, file_size + 1);
  if (!json) {
    fclose(file);
    return NULL;
  }

  size_t read_size = fread(json, 1, file_size, file);
  json[read_size] = '\0';
  fclose(file);

  mongory_value *value = json_string_to_mongory_value(pool, json);
  free(json);
  return value;
}

typedef struct mongory_test_execute_context {
  mongory_matcher *matcher;
  int index;
  bool enable_trace;
  bool enable_explain;
  bool show_progress;
} mongory_test_execute_context;

bool execute_test_record(mongory_value *test_record, void *acc) {
  mongory_test_execute_context *context = (mongory_test_execute_context *)acc;
  mongory_string_buffer *record_buffer = mongory_string_buffer_new(test_record->pool);
  mongory_string_buffer_append(record_buffer, test_record->to_str(test_record, test_record->pool));
  if (context->show_progress) {
    printf("Running test record: %d -> %s\n", context->index, mongory_string_buffer_cstr(record_buffer));
  }
  mongory_matcher *matcher = context->matcher;
  mongory_value *data_value = test_record->data.t->get(test_record->data.t, "data");
  mongory_value *expected_value = test_record->data.t->get(test_record->data.t, "expected");
  bool expected = expected_value->data.b;
  TEST_ASSERT_NOT_NULL(data_value);
  TEST_ASSERT_NOT_NULL(expected_value);

  bool result = matcher->match(matcher, data_value);
  if (context->enable_trace) {
    mongory_matcher_trace(matcher, data_value);
  }
  if (expected != result) {
    printf("Test failed\n");
  }
  TEST_ASSERT_EQUAL(expected, matcher->match(matcher, data_value));
  context->index++;
  return true;
}

bool execute_each_test_case(mongory_value *test_case, void *acc) {
  mongory_test_context *context = (mongory_test_context *)acc;
  mongory_value *description_value = test_case->data.t->get(test_case->data.t, "description");
  mongory_value *condition_value = test_case->data.t->get(test_case->data.t, "condition");
  mongory_value *records_value = test_case->data.t->get(test_case->data.t, "records");
  TEST_ASSERT_NOT_NULL(description_value);
  if (context->show_progress) {
    printf("====\n");
    printf("Running test case: %s\n", description_value->data.s);
  }
  TEST_ASSERT_NOT_NULL(condition_value);
  TEST_ASSERT_NOT_NULL(records_value);
  mongory_memory_pool *matcher_pool = mongory_memory_pool_new();
  mongory_matcher *matcher = context->matcher_build_func(matcher_pool, condition_value, NULL);
  TEST_ASSERT_NOT_NULL(matcher);
  TEST_ASSERT_NULL(matcher_pool->error);
  mongory_array *records = records_value->data.a;
  mongory_test_execute_context execute_context = {matcher, 0, context->enable_trace, context->enable_explain, context->show_progress};
  records->each(records, &execute_context, execute_test_record);
  if (context->enable_explain) {
    mongory_memory_pool *stdout_pool = mongory_memory_pool_new();
    mongory_matcher_explain(matcher, stdout_pool);
    stdout_pool->free(stdout_pool);
  }
  matcher->pool->free(matcher->pool);
  return true;
}

void execute_test_case(char *file_name, mongory_test_context *context) {
  mongory_value *parsed = json_to_mongory_value_from_file(get_test_pool(), file_name);
  TEST_ASSERT_NOT_NULL(parsed);
  mongory_array *test_cases = parsed->data.a;
  TEST_ASSERT_NOT_NULL(test_cases);
  test_cases->each(test_cases, context, execute_each_test_case);
}

void setup_test_environment(void) {
  mongory_init();
  test_pool = mongory_memory_pool_new();
  TEST_ASSERT_NOT_NULL(test_pool);
}

void teardown_test_environment(void) {
  mongory_cleanup();
  if (test_pool) {
    test_pool->free(test_pool);
    test_pool = NULL;
  }
}

mongory_memory_pool *get_test_pool(void) { return test_pool; }

void assert_value_equals(mongory_value *expected, mongory_value *actual) {
  TEST_ASSERT_NOT_NULL(expected);
  TEST_ASSERT_NOT_NULL(actual);
  TEST_ASSERT_EQUAL_INT(0, actual->comp(actual, expected));
}

void assert_value_not_equals(mongory_value *expected, mongory_value *actual) {
  TEST_ASSERT_NOT_NULL(expected);
  TEST_ASSERT_NOT_NULL(actual);
  TEST_ASSERT_NOT_EQUAL(0, actual->comp(actual, expected));
}

void assert_value_is_null(mongory_value *value) { TEST_ASSERT_NULL(value); }

void assert_value_is_not_null(mongory_value *value) { TEST_ASSERT_NOT_NULL(value); }

#endif // MONGORY_TEST_HELPER_C