#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mongory-core.h>
#include <cjson/cJSON.h>
#include <mongory-core/foundations/value.h>
#include <mongory-core/foundations/memory_pool.h>
#include "test_helper.h"
#include "../../tests/unity/unity.h"

static mongory_memory_pool *test_pool = NULL;

// JSON 轉換相關函數
static mongory_value *cjson_to_mongory_value(mongory_memory_pool *pool, cJSON *root) {
    if (!root) return NULL;

    mongory_value *value = NULL;
    mongory_array *array = NULL;
    mongory_table *table = NULL;
    
    switch (root->type) {
        case cJSON_String: {
            char *str_copy = pool->alloc(pool->ctx, strlen(root->valuestring) + 1);
            strcpy(str_copy, root->valuestring);
            value = mongory_value_wrap_s(pool, str_copy);
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
        case cJSON_Array:
            array = mongory_array_new(pool);
            value = mongory_value_wrap_a(pool, array);
            for (cJSON *item = root->child; item; item = item->next) {
                mongory_value *item_value = cjson_to_mongory_value(pool, item);
                if (item_value) {
                    array->push(array, item_value);
                }
            }
            break;
        case cJSON_Object:
            table = mongory_table_new(pool);
            value = mongory_value_wrap_t(pool, table);
            for (cJSON *item = root->child; item; item = item->next) {
                mongory_value *item_value = cjson_to_mongory_value(pool, item);
                if (item_value) {
                    char *key_copy = pool->alloc(pool->ctx, strlen(item->string) + 1);
                    strcpy(key_copy, item->string);
                    table->set(table, key_copy, item_value);
                }
            }
            break;
        default:
            fprintf(stderr, "Unsupported JSON type: %d\n", root->type);
            break;
    }

    return value;
}

mongory_value *json_string_to_mongory_value(mongory_memory_pool *pool, const char *json) {
    if (!json) return NULL;
    
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            fprintf(stderr, "JSON Parse Error: %s\n", error_ptr);
        }
        return NULL;
    }

    mongory_value *value = cjson_to_mongory_value(pool, root);
    cJSON_Delete(root);
    return value;
}

mongory_value *json_to_value_from_file(mongory_memory_pool *pool, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json = malloc(file_size + 1);
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

void setup_test_environment(void) {
    test_pool = mongory_memory_pool_new();
    TEST_ASSERT_NOT_NULL(test_pool);
}

void teardown_test_environment(void) {
    if (test_pool) {
        test_pool->free(test_pool);
        test_pool = NULL;
    }
}

mongory_memory_pool *get_test_pool(void) {
    return test_pool;
}

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

void assert_value_is_null(mongory_value *value) {
    TEST_ASSERT_NULL(value);
}

void assert_value_is_not_null(mongory_value *value) {
    TEST_ASSERT_NOT_NULL(value);
}