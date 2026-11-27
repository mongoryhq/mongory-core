#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mongory-core.h>

typedef struct {
  const uint8_t *data;
  size_t size;
  size_t pos;
} fuzz_stream_t;

static uint8_t fs_u8(fuzz_stream_t *s) {
  if (s->pos >= s->size) {
    return 0;
  }
  return s->data[s->pos++];
}

static int64_t fs_i64(fuzz_stream_t *s) {
  static const int64_t interesting[] = {
    0, 1, -1, 2, 18, 40, 100, -100, 1000, INT32_MAX, INT32_MIN, INT64_MAX, INT64_MIN
  };
  uint8_t sel = fs_u8(s);
  if ((sel & 1) == 0) {
    return interesting[(sel >> 1) % (sizeof(interesting) / sizeof(interesting[0]))];
  }
  return (int64_t)(int8_t)fs_u8(s);
}

static double fs_double(fuzz_stream_t *s) {
  uint8_t sel = fs_u8(s) % 8;
  switch (sel) {
    case 0: return 0.0;
    case 1: return -0.0;
    case 2: return 1.5;
    case 3: return -18.25;
    case 4: return 100.0;
    case 5: return NAN;
    case 6: return INFINITY;
    default: return -INFINITY;
  }
}

static char *fs_string(fuzz_stream_t *s, mongory_memory_pool *pool) {
  static const char *const dict[] = {
    "", "a", "b", "age", "name", "status", "tags", "active", "inactive",
    "go", "ruby", "c", "0", "-1", "01", "-0", "a.b", "a\\.b", "$unknown"
  };
  uint8_t sel = fs_u8(s);
  if (sel < 192) {
    const char *word = dict[sel % (sizeof(dict) / sizeof(dict[0]))];
    size_t len = strlen(word);
    char *copy = MG_ALLOC(pool, len + 1);
    if (!copy) return NULL;
    memcpy(copy, word, len + 1);
    return copy;
  }
  size_t len = (size_t)(fs_u8(s) % 12);
  char *buf = MG_ALLOC(pool, len + 1);
  if (!buf) return NULL;
  for (size_t i = 0; i < len; i++) {
    buf[i] = (char)('a' + (fs_u8(s) % 26));
  }
  buf[len] = '\0';
  return buf;
}

static const char *fs_key(fuzz_stream_t *s, bool allow_ops) {
  static const char *const fields[] = {
    "a", "b", "age", "status", "tags", "profile.city", "0", "-1", "01", "-0", "a\\.b"
  };
  static const char *const ops[] = {
    "$eq", "$ne", "$gt", "$gte", "$lt", "$lte",
    "$in", "$nin", "$and", "$or", "$nor", "$not",
    "$elemMatch", "$every", "$size", "$exists", "$present",
    "$regex", "$options", "$unknown"
  };
  uint8_t sel = fs_u8(s);
  if (allow_ops && (sel & 1) == 0) {
    return ops[(sel >> 1) % (sizeof(ops) / sizeof(ops[0]))];
  }
  return fields[sel % (sizeof(fields) / sizeof(fields[0]))];
}

static mongory_value *fs_value(fuzz_stream_t *s, mongory_memory_pool *pool, int depth, bool allow_ops) {
  if (depth <= 0 || s->pos >= s->size) {
    switch (fs_u8(s) % 5) {
      case 0: return mongory_value_wrap_n(pool, NULL);
      case 1: return mongory_value_wrap_b(pool, (fs_u8(s) & 1) != 0);
      case 2: return mongory_value_wrap_i(pool, fs_i64(s));
      case 3: return mongory_value_wrap_d(pool, fs_double(s));
      default: {
        char *str = fs_string(s, pool);
        return str ? mongory_value_wrap_s(pool, str) : mongory_value_wrap_n(pool, NULL);
      }
    }
  }
  uint8_t kind = fs_u8(s) % 7;
  if (kind < 5) {
    return fs_value(s, pool, 0, allow_ops);
  }
  if (kind == 5) {
    mongory_array *arr = mongory_array_new(pool);
    if (!arr) return mongory_value_wrap_n(pool, NULL);
    int count = (int)(fs_u8(s) % 4);
    for (int i = 0; i < count && s->pos < s->size; i++) {
      mongory_value *elem = fs_value(s, pool, depth - 1, allow_ops);
      if (elem) arr->push(arr, elem);
    }
    return mongory_value_wrap_a(pool, arr);
  }
  mongory_table *tbl = mongory_table_new(pool);
  if (!tbl) return mongory_value_wrap_n(pool, NULL);
  int count = (int)(fs_u8(s) % 4);
  for (int i = 0; i < count && s->pos < s->size; i++) {
    const char *key = fs_key(s, allow_ops);
    mongory_value *val = fs_value(s, pool, depth - 1, allow_ops);
    if (val) tbl->set(tbl, (char *)key, val);
  }
  return mongory_value_wrap_t(pool, tbl);
}

static mongory_value *fs_root_table(fuzz_stream_t *s, mongory_memory_pool *pool, int depth, bool allow_ops) {
  mongory_table *tbl = mongory_table_new(pool);
  if (!tbl) return NULL;
  int count = (int)(fs_u8(s) % 4);
  for (int i = 0; i < count && s->pos < s->size; i++) {
    const char *key = fs_key(s, allow_ops);
    mongory_value *val = fs_value(s, pool, depth - 1, allow_ops);
    if (val) tbl->set(tbl, (char *)key, val);
  }
  return mongory_value_wrap_t(pool, tbl);
}

static bool fuzz_regex_match(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value) {
  (void)pool;
  if (!pattern || pattern->type != MONGORY_TYPE_STRING || !pattern->data.s) return false;
  if (!value || value->type != MONGORY_TYPE_STRING || !value->data.s) return false;
  return strstr(value->data.s, pattern->data.s) != NULL;
}

static bool fuzz_regex_options_match(mongory_memory_pool *pool, mongory_value *pattern, const char *options,
                                     mongory_value *value) {
  (void)options;
  return fuzz_regex_match(pool, pattern, value);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static bool initialized = false;
  if (!initialized) {
    mongory_init();
    mongory_regex_func_set(fuzz_regex_match);
    mongory_regex_options_func_set(fuzz_regex_options_match);
    initialized = true;
  }

  fuzz_stream_t stream = { .data = data, .size = size, .pos = 0 };
  mongory_memory_pool *cond_pool = mongory_memory_pool_new();
  if (!cond_pool) {
    return 0;
  }

  mongory_value *cond = fs_root_table(&stream, cond_pool, 3, true);
  if (!cond || cond_pool->error != NULL) {
    cond_pool->free(cond_pool);
    return 0;
  }

  mongory_matcher *matcher = mongory_matcher_new(cond_pool, cond, NULL);
  if (matcher && cond_pool->error == NULL) {
    mongory_memory_pool *eval_pool = mongory_memory_pool_new();
    if (eval_pool) {
      // Explain tree must never crash
      (void)mongory_matcher_explain_tree(matcher, eval_pool);

      // Evaluate against 2 generated records (table or arbitrary value)
      for (int r = 0; r < 2; r++) {
        mongory_memory_pool *rec_pool = mongory_memory_pool_new();
        if (!rec_pool) break;
        mongory_value *rec = (r == 0)
          ? fs_root_table(&stream, rec_pool, 3, false)
          : fs_value(&stream, rec_pool, 2, false);
        if (rec && rec_pool->error == NULL) {
          bool matched = mongory_matcher_match(matcher, rec);
          mongory_value *trace = mongory_matcher_trace_tree(matcher, rec);
          if (trace && trace->type == MONGORY_TYPE_TABLE && trace->data.t) {
            mongory_value *res = trace->data.t->get(trace->data.t, "result");
            if (res && res->type == MONGORY_TYPE_STRING && res->data.s) {
              bool trace_matched = strcmp(res->data.s, "pass") == 0;
              if (trace_matched != matched) {
                fprintf(stderr, "fuzz invariant violation: match=%d trace=%d\n", (int)matched, (int)trace_matched);
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

  cond_pool->free(cond_pool);
  return 0;
}

#ifdef MONGORY_FUZZ_STANDALONE
static uint64_t xorshift64(uint64_t *state) {
  uint64_t x = *state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  *state = x;
  return x;
}

int main(void) {
  const char *env_iters = getenv("MONGORY_FUZZ_ITERS");
  int iters = env_iters ? atoi(env_iters) : 5000;
  if (iters <= 0) iters = 5000;

  uint64_t seed = 0x9e3779b97f4a7c15ULL;
  uint8_t buf[128];
  for (int i = 0; i < iters; i++) {
    size_t len = (size_t)(xorshift64(&seed) % sizeof(buf)) + 1;
    for (size_t j = 0; j < len; j++) {
      buf[j] = (uint8_t)(xorshift64(&seed) & 0xff);
    }
    LLVMFuzzerTestOneInput(buf, len);
  }
  mongory_cleanup();
  printf("mongory-core fuzz smoke passed (%d iterations)\n", iters);
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
