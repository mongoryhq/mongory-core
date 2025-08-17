#include "../src/test_helper/test_helper.h"
#include <mongory-core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void setUp(void) {}
void tearDown(void) {}

char *valid_statuses[] = {"active", "inactive"};

cJSON *mongory_create_json_test_record(void) {
  cJSON *root = cJSON_CreateObject();
  int age = rand() % 100 + 1;
  int status_index = rand() % 2;
  char *status = valid_statuses[status_index];
  cJSON_AddNumberToObject(root, "age", age);
  cJSON_AddStringToObject(root, "status", status);
  if (age >= 18 || strcmp(status, "active") == 0) {
    cJSON_AddTrueToObject(root, "expected");
  } else {
    cJSON_AddFalseToObject(root, "expected");
  }
  return root;
}

bool mongory_array_benchmark_match_test(mongory_value *item, void *acc) {
  mongory_matcher *matcher = (mongory_matcher *)acc;
  cJSON *json_record = (cJSON *)item;
  cJSON *expected_item = cJSON_GetObjectItem(json_record, "expected");
  if (expected_item == NULL) {
    printf("Expected item is NULL\n");
    return false;
  }
  mongory_memory_pool *record_pool = mongory_memory_pool_new();
  mongory_value *json_value = cjson_to_mongory_value_shallow_convert(record_pool, json_record);
  bool result = matcher->match(matcher, json_value);
  int expected = cJSON_IsTrue(expected_item);
  if (result != expected) {
    printf("Result: %d, Expected: %d\n", result, expected);
    char *json_string = cJSON_Print(json_record);
    printf("JSON string: %s\n", json_string);
    free(json_string);
  }
  record_pool->free(record_pool);
  return result;
}

bool mongory_array_free_cjson_object(mongory_value *item, void *acc) {
  (void)acc;
  cJSON *json_record = (cJSON *)item;
  cJSON_Delete(json_record);
  return true;
}

int main() {
  srand(time(NULL));
  mongory_init();
  mongory_value_converter_shallow_convert_set((mongory_shallow_convert_func)cjson_to_mongory_value_shallow_convert);
  mongory_value_converter_deep_convert_set((mongory_deep_convert_func)cjson_to_mongory_value_deep_convert);

  mongory_memory_pool *matcher_pool = mongory_memory_pool_new();
  mongory_value *condition =
      json_string_to_mongory_value(matcher_pool, "{\"$or\": [{\"age\": {\"$gte\": 18}}, {\"status\": \"active\"}]}");
  mongory_matcher *matcher = mongory_matcher_new(matcher_pool, condition, NULL);
  mongory_array *json_array = mongory_array_new(matcher_pool);

  int times = 100000;
  for (int i = 0; i < times; i++) {
    json_array->push(json_array, (mongory_value *)mongory_create_json_test_record());
  }

  printf("Starting benchmark...\n");
  double time_start = clock();
  json_array->each(json_array, matcher, mongory_array_benchmark_match_test);
  double time_end = clock();
  printf("Time taken: %f seconds\n", (time_end - time_start) / CLOCKS_PER_SEC);
  printf("Benchmark done.\n");

  json_array->each(json_array, NULL, mongory_array_free_cjson_object);
  matcher_pool->free(matcher_pool);
  mongory_cleanup();
  return 0;
}
