#include <limits.h>
#include <mongory-core.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

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

static mongory_value *yyjson_to_value(mongory_memory_pool *pool, yyjson_val *node, int depth) {
  if (!node || depth > FUZZ_JSON_MAX_DEPTH) {
    return mongory_value_wrap_n(pool, NULL);
  }
  switch (yyjson_get_type(node)) {
  case YYJSON_TYPE_NULL:
    return mongory_value_wrap_n(pool, NULL);
  case YYJSON_TYPE_BOOL:
    return mongory_value_wrap_b(pool, yyjson_get_bool(node));
  case YYJSON_TYPE_NUM:
    if (yyjson_is_sint(node)) {
      return mongory_value_wrap_i(pool, yyjson_get_sint(node));
    }
    if (yyjson_is_uint(node)) {
      uint64_t u = yyjson_get_uint(node);
      if (u <= (uint64_t)INT64_MAX) {
        return mongory_value_wrap_i(pool, (int64_t)u);
      }
      return mongory_value_wrap_d(pool, (double)u);
    }
    return mongory_value_wrap_d(pool, yyjson_get_real(node));
  case YYJSON_TYPE_STR:
    return mongory_value_wrap_s(pool, (char *)(yyjson_get_str(node) ? yyjson_get_str(node) : ""));
  case YYJSON_TYPE_ARR: {
    mongory_array *arr = mongory_array_new(pool);
    if (!arr) return NULL;
    size_t idx, max;
    yyjson_val *child;
    yyjson_arr_foreach(node, idx, max, child) {
      mongory_value *v = yyjson_to_value(pool, child, depth + 1);
      if (v) arr->push(arr, v);
    }
    return mongory_value_wrap_a(pool, arr);
  }
  case YYJSON_TYPE_OBJ: {
    mongory_table *tbl = mongory_table_new(pool);
    if (!tbl) return NULL;
    size_t idx, max;
    yyjson_val *key, *child;
    yyjson_obj_foreach(node, idx, max, key, child) {
      const char *k = yyjson_get_str(key);
      if (!k) continue;
      mongory_value *v = yyjson_to_value(pool, child, depth + 1);
      if (v) tbl->set(tbl, (char *)k, v);
    }
    return mongory_value_wrap_t(pool, tbl);
  }
  default:
    return mongory_value_wrap_n(pool, NULL);
  }
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
  // If no NUL byte is present, evaluate the parsed condition against itself.
  const uint8_t *sep = (const uint8_t *)memchr(data, '\0', size);
  const uint8_t *cond_bytes = data;
  size_t cond_len = sep ? (size_t)(sep - data) : size;
  const uint8_t *rec_bytes = sep ? (sep + 1) : NULL;
  size_t rec_len = sep ? (size - cond_len - 1) : 0;

  yyjson_doc *cond_doc = yyjson_read((const char *)cond_bytes, cond_len, 0);
  if (!cond_doc) {
    return 0;
  }

  yyjson_doc *rec_doc = (rec_bytes && rec_len > 0) ? yyjson_read((const char *)rec_bytes, rec_len, 0) : NULL;

  mongory_memory_pool *cond_pool = mongory_memory_pool_new();
  if (!cond_pool) {
    yyjson_doc_free(cond_doc);
    if (rec_doc) yyjson_doc_free(rec_doc);
    return 0;
  }

  yyjson_val *cond_root = yyjson_doc_get_root(cond_doc);
  mongory_value *cond_val = yyjson_to_value(cond_pool, cond_root, 0);
  if (cond_val && cond_pool->error == NULL) {
    mongory_matcher *matcher = mongory_matcher_new(cond_pool, cond_val, NULL);
    if (matcher && cond_pool->error == NULL) {
      mongory_memory_pool *eval_pool = mongory_memory_pool_new();
      if (eval_pool) {
        (void)mongory_matcher_explain_tree(matcher, eval_pool);

        mongory_memory_pool *rec_pool = mongory_memory_pool_new();
        if (rec_pool) {
          yyjson_val *target_root = rec_doc ? yyjson_doc_get_root(rec_doc) : cond_root;
          mongory_value *rec_val = yyjson_to_value(rec_pool, target_root, 0);
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
  yyjson_doc_free(cond_doc);
  if (rec_doc) yyjson_doc_free(rec_doc);
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
