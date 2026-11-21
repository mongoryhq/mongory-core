#include <cjson/cJSON.h>
#include <mongory-core.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_JSON_MAX_DEPTH 24

static bool fuzz_json_regex_match(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value) {
  (void)pool;
  if (!pattern || pattern->type != MONGORY_TYPE_STRING || !pattern->data.s) return false;
  if (!value || value->type != MONGORY_TYPE_STRING || !value->data.s) return false;
  return strstr(value->data.s, pattern->data.s) != NULL;
}

static bool fuzz_json_regex_options_match(mongory_memory_pool *pool, mongory_value *pattern, const char *options,
                                          mongory_value *value) {
  (void)options;
  return fuzz_json_regex_match(pool, pattern, value);
}

static mongory_value *cjson_to_value(mongory_memory_pool *pool, const cJSON *node, int depth) {
  if (!node || depth > FUZZ_JSON_MAX_DEPTH) {
    return mongory_value_wrap_n(pool, NULL);
  }
  if (cJSON_IsNull(node)) {
    return mongory_value_wrap_n(pool, NULL);
  }
  if (cJSON_IsBool(node)) {
    return mongory_value_wrap_b(pool, cJSON_IsTrue(node) ? true : false);
  }
  if (cJSON_IsNumber(node)) {
    double d = node->valuedouble;
    if (d == (double)node->valueint) {
      return mongory_value_wrap_i(pool, (int64_t)node->valueint);
    }
    return mongory_value_wrap_d(pool, d);
  }
  if (cJSON_IsString(node)) {
    return mongory_value_wrap_s(pool, node->valuestring ? node->valuestring : "");
  }
  if (cJSON_IsArray(node)) {
    mongory_array *arr = mongory_array_new(pool);
    if (!arr) return NULL;
    for (const cJSON *child = node->child; child; child = child->next) {
      mongory_value *v = cjson_to_value(pool, child, depth + 1);
      if (v) arr->push(arr, v);
    }
    return mongory_value_wrap_a(pool, arr);
  }
  if (cJSON_IsObject(node)) {
    mongory_table *tbl = mongory_table_new(pool);
    if (!tbl) return NULL;
    for (const cJSON *child = node->child; child; child = child->next) {
      if (!child->string) continue;
      mongory_value *v = cjson_to_value(pool, child, depth + 1);
      if (v) tbl->set(tbl, child->string, v);
    }
    return mongory_value_wrap_t(pool, tbl);
  }
  return mongory_value_wrap_n(pool, NULL);
}

static cJSON *parse_json_slice(const uint8_t *data, size_t len) {
  char *buf = (char *)malloc(len + 1);
  if (!buf) return NULL;
  memcpy(buf, data, len);
  buf[len] = '\0';
  cJSON *parsed = cJSON_Parse(buf);
  free(buf);
  return parsed;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static bool initialized = false;
  if (!initialized) {
    mongory_init();
    mongory_regex_func_set(fuzz_json_regex_match);
    mongory_regex_options_func_set(fuzz_json_regex_options_match);
    initialized = true;
  }

  if (size == 0 || size > 4096) {
    return 0;
  }

  // Split input on first NUL byte into condition JSON and record JSON.
  // If no NUL byte is present, evaluate the parsed condition against itself and a default record.
  const uint8_t *sep = (const uint8_t *)memchr(data, '\0', size);
  const uint8_t *cond_bytes = data;
  size_t cond_len = sep ? (size_t)(sep - data) : size;
  const uint8_t *rec_bytes = sep ? (sep + 1) : NULL;
  size_t rec_len = sep ? (size - cond_len - 1) : 0;

  cJSON *cond_json = parse_json_slice(cond_bytes, cond_len);
  if (!cond_json) {
    return 0;
  }

  cJSON *rec_json = (rec_bytes && rec_len > 0) ? parse_json_slice(rec_bytes, rec_len) : NULL;

  mongory_memory_pool *cond_pool = mongory_memory_pool_new();
  if (!cond_pool) {
    cJSON_Delete(cond_json);
    if (rec_json) cJSON_Delete(rec_json);
    return 0;
  }

  mongory_value *cond_val = cjson_to_value(cond_pool, cond_json, 0);
  if (cond_val && cond_pool->error == NULL) {
    mongory_matcher *matcher = mongory_matcher_new(cond_pool, cond_val, NULL);
    if (matcher && cond_pool->error == NULL) {
      mongory_memory_pool *eval_pool = mongory_memory_pool_new();
      if (eval_pool) {
        (void)mongory_matcher_explain_tree(matcher, eval_pool);

        mongory_memory_pool *rec_pool = mongory_memory_pool_new();
        if (rec_pool) {
          const cJSON *target_json = rec_json ? rec_json : cond_json;
          mongory_value *rec_val = cjson_to_value(rec_pool, target_json, 0);
          if (rec_val && rec_pool->error == NULL) {
            bool matched = mongory_matcher_match(matcher, rec_val);
            mongory_value *trace = mongory_matcher_trace_tree(matcher, rec_val);
            if (trace && trace->type == MONGORY_TYPE_TABLE && trace->data.t) {
              mongory_value *res = trace->data.t->get(trace->data.t, "result");
              if (res && res->type == MONGORY_TYPE_STRING && res->data.s) {
                bool trace_matched = strcmp(res->data.s, "pass") == 0;
                if (trace_matched != matched) {
                  fprintf(stderr, "fuzz_json invariant violation: match=%d trace=%d\n", (int)matched,
                          (int)trace_matched);
                  abort();
                }
              }
            }
          }
          rec_pool->free(rec_pool);
        }
        eval_pool->free(eval_pool);
      }
    }
  }

  cond_pool->free(cond_pool);
  cJSON_Delete(cond_json);
  if (rec_json) cJSON_Delete(rec_json);
  return 0;
}

#ifdef MONGORY_FUZZ_STANDALONE
int main(void) {
  static const char *const seeds[] = {
    "{\"age\":{\"$gte\":18,\"$lt\":65}}\0{\"age\":30}",
    "{\"$and\":[{\"status\":\"active\"},{\"tags\":{\"$in\":[\"c\",\"go\"]}}]}\0{\"status\":\"active\",\"tags\":[\"c\"]}",
    "{\"$or\":[{\"a\":{\"$exists\":false}},{\"b\":{\"$present\":true}}]}\0{\"b\":\"ok\"}",
    "{\"items\":{\"$elemMatch\":{\"qty\":{\"$gt\":1},\"ok\":true}}}\0{\"items\":[{\"qty\":2,\"ok\":true}]}",
    "{\"scores\":{\"$every\":{\"$gte\":60},\"$size\":{\"$gte\":1}}}\0{\"scores\":[70,80]}",
    "{\"name\":{\"$regex\":\"foo\",\"$options\":\"i\"}}\0{\"name\":\"foobar\"}",
    "{\"$not\":{\"x\":null}}\0{\"x\":1}",
    "{\"$unknown\":123}\0{}",
    "{\"$in\":\"not-an-array\"}\0{}",
    "{\"$and\":123}\0{}",
    "{\"$or\":[]}\0{}",
    "{\"$and\":[]}\0{}",
    "[1,2,3]\0{\"a\":1}",
    "\"scalar\"\0{}",
    "{invalid json\0{}",
    "{\"a\":{\"b\":{\"c\":{\"$eq\":-0.0}}}}\0{\"a\":{\"b\":{\"c\":0}}}",
  };
  static const size_t seed_lens[] = {
    sizeof("{\"age\":{\"$gte\":18,\"$lt\":65}}\0{\"age\":30}") - 1,
    sizeof("{\"$and\":[{\"status\":\"active\"},{\"tags\":{\"$in\":[\"c\",\"go\"]}}]}\0{\"status\":\"active\",\"tags\":[\"c\"]}") - 1,
    sizeof("{\"$or\":[{\"a\":{\"$exists\":false}},{\"b\":{\"$present\":true}}]}\0{\"b\":\"ok\"}") - 1,
    sizeof("{\"items\":{\"$elemMatch\":{\"qty\":{\"$gt\":1},\"ok\":true}}}\0{\"items\":[{\"qty\":2,\"ok\":true}]}") - 1,
    sizeof("{\"scores\":{\"$every\":{\"$gte\":60},\"$size\":{\"$gte\":1}}}\0{\"scores\":[70,80]}") - 1,
    sizeof("{\"name\":{\"$regex\":\"foo\",\"$options\":\"i\"}}\0{\"name\":\"foobar\"}") - 1,
    sizeof("{\"$not\":{\"x\":null}}\0{\"x\":1}") - 1,
    sizeof("{\"$unknown\":123}\0{}") - 1,
    sizeof("{\"$in\":\"not-an-array\"}\0{}") - 1,
    sizeof("{\"$and\":123}\0{}") - 1,
    sizeof("{\"$or\":[]}\0{}") - 1,
    sizeof("{\"$and\":[]}\0{}") - 1,
    sizeof("[1,2,3]\0{\"a\":1}") - 1,
    sizeof("\"scalar\"\0{}") - 1,
    sizeof("{invalid json\0{}") - 1,
    sizeof("{\"a\":{\"b\":{\"c\":{\"$eq\":-0.0}}}}\0{\"a\":{\"b\":{\"c\":0}}}") - 1,
  };

  size_t count = sizeof(seeds) / sizeof(seeds[0]);
  for (size_t i = 0; i < count; i++) {
    LLVMFuzzerTestOneInput((const uint8_t *)seeds[i], seed_lens[i]);
  }
  mongory_cleanup();
  printf("mongory-core json fuzz smoke passed (%zu seeds)\n", count);
  return 0;
}
#elif defined(MONGORY_FUZZ_AFL)
#ifdef __AFL_FUZZ_TESTCASE_LEN
__AFL_FUZZ_INIT();
int main(void) {
#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_INIT();
#endif
  unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
  while (__AFL_LOOP(10000)) {
    int len = __AFL_FUZZ_TESTCASE_LEN;
    if (len > 0) {
      LLVMFuzzerTestOneInput((const uint8_t *)buf, (size_t)len);
    }
  }
  mongory_cleanup();
  return 0;
}
#else
int main(void) {
  uint8_t buf[4096];
  size_t len = fread(buf, 1, sizeof(buf), stdin);
  if (len > 0) {
    LLVMFuzzerTestOneInput(buf, len);
  }
  mongory_cleanup();
  return 0;
}
#endif
#endif
