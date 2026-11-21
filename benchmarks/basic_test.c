#include "../src/test_helper/test_helper.h"
#include <mongory-core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void setUp(void) {}
void tearDown(void) {}

char *valid_statuses[] = {"active", "inactive"};

yyjson_doc *mongory_create_json_test_record(void) {
  int age = rand() % 100 + 1;
  int status_index = rand() % 2;
  const char *status = valid_statuses[status_index];
  bool expected = (age >= 18 || strcmp(status, "active") == 0);
  char buf[128];
  int len = snprintf(buf, sizeof(buf), "{\"age\":%d,\"status\":\"%s\",\"expected\":%s}",
                     age, status, expected ? "true" : "false");
  return yyjson_read(buf, (size_t)len, 0);
}

bool mongory_array_benchmark_match_test(mongory_value *item, void *acc) {
  mongory_matcher *matcher = (mongory_matcher *)acc;
  yyjson_doc *doc = (yyjson_doc *)item;
  yyjson_val *json_record = yyjson_doc_get_root(doc);
  yyjson_val *expected_item = yyjson_obj_get(json_record, "expected");
  if (expected_item == NULL) {
    printf("Expected item is NULL\n");
    return false;
  }
  mongory_memory_pool *record_pool = mongory_memory_pool_new();
  mongory_value *json_value = yyjson_to_mongory_value_shallow_convert(record_pool, json_record);
  bool result = matcher->match(matcher, json_value);
  bool expected = yyjson_get_bool(expected_item);
  if (result != expected) {
    printf("Result: %d, Expected: %d\n", result, expected);
    char *json_string = yyjson_write(doc, 0, NULL);
    printf("JSON string: %s\n", json_string);
    free(json_string);
  }
  record_pool->free(record_pool);
  return result;
}

bool mongory_array_free_yyjson_doc(mongory_value *item, void *acc) {
  (void)acc;
  yyjson_doc *doc = (yyjson_doc *)item;
  yyjson_doc_free(doc);
  return true;
}

int main() {
  srand((unsigned int)time(NULL));
  mongory_init();
  mongory_value_converter_shallow_convert_set((mongory_shallow_convert_func)yyjson_to_mongory_value_shallow_convert);
  mongory_value_converter_deep_convert_set((mongory_deep_convert_func)yyjson_to_mongory_value_deep_convert);

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

  json_array->each(json_array, NULL, mongory_array_free_yyjson_doc);
  matcher_pool->free(matcher_pool);
  mongory_cleanup();
  return 0;
}
