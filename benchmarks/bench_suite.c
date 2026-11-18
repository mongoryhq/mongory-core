#include <mongory-core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCH_RECORDS 10000
#define BENCH_ROUNDS 200

typedef bool (*plain_pred_fn)(mongory_value *record, void *ctx);

typedef struct {
  const char *name;
  mongory_value *cond;
  plain_pred_fn plain;
} bench_case;

static double elapsed_ns(struct timespec start, struct timespec end) {
  return (double)(end.tv_sec - start.tv_sec) * 1e9 + (double)(end.tv_nsec - start.tv_nsec);
}

static char *pool_strdup(mongory_memory_pool *pool, const char *s) {
  size_t len = strlen(s) + 1;
  char *copy = (char *)pool->alloc(pool, len);
  memcpy(copy, s, len);
  return copy;
}

static inline mongory_value *tbl_get(mongory_value *v, const char *k) {
  if (!v || v->type != MONGORY_TYPE_TABLE) return NULL;
  return v->data.t->get(v->data.t, (char *)k);
}

static bool plain_simple(mongory_value *rec, void *ctx) {
  (void)ctx;
  mongory_value *age = tbl_get(rec, "age");
  return age && age->type == MONGORY_TYPE_INT && age->data.i >= 18;
}

static bool plain_or(mongory_value *rec, void *ctx) {
  (void)ctx;
  if (plain_simple(rec, NULL)) return true;
  mongory_value *status = tbl_get(rec, "status");
  return status && status->type == MONGORY_TYPE_STRING && strcmp(status->data.s, "active") == 0;
}

static bool plain_nested_and(mongory_value *rec, void *ctx) {
  (void)ctx;
  mongory_value *status = tbl_get(rec, "status");
  if (!status || status->type != MONGORY_TYPE_STRING || strcmp(status->data.s, "active") != 0) return false;
  mongory_value *age = tbl_get(rec, "age");
  if (!age || age->type != MONGORY_TYPE_INT || age->data.i >= 40) return false;
  mongory_value *profile = tbl_get(rec, "profile");
  mongory_value *city = tbl_get(profile, "city");
  return city && city->type == MONGORY_TYPE_STRING && strcmp(city->data.s, "taipei") == 0;
}

static bool plain_in_array_field(mongory_value *rec, void *ctx) {
  (void)ctx;
  mongory_value *tags = tbl_get(rec, "tags");
  if (!tags || tags->type != MONGORY_TYPE_ARRAY) return false;
  mongory_array *arr = tags->data.a;
  for (size_t i = 0; i < arr->count; i++) {
    mongory_value *el = arr->get(arr, i);
    if (el && el->type == MONGORY_TYPE_STRING &&
        (strcmp(el->data.s, "go") == 0 || strcmp(el->data.s, "rust") == 0)) {
      return true;
    }
  }
  return false;
}

static bool plain_large_in_500(mongory_value *rec, void *ctx) {
  mongory_array *list = (mongory_array *)ctx;
  mongory_value *name = tbl_get(rec, "name");
  if (!name || name->type != MONGORY_TYPE_STRING) return false;
  for (size_t i = 0; i < list->count; i++) {
    mongory_value *item = list->get(list, i);
    if (item && item->type == MONGORY_TYPE_STRING && strcmp(name->data.s, item->data.s) == 0) {
      return true;
    }
  }
  return false;
}

int main(void) {
  mongory_init();
  mongory_memory_pool *pool = mongory_memory_pool_new();

  mongory_value **records = (mongory_value **)malloc(sizeof(mongory_value *) * BENCH_RECORDS);
  for (int i = 0; i < BENCH_RECORDS; i++) {
    mongory_value *profile = MG_TABLE_WRAP(pool, 1, "city", mongory_value_wrap_s(pool, (i % 2 == 0) ? "taipei" : "tokyo"));
    mongory_value *tags = (i % 4 == 0)
      ? MG_ARRAY_WRAP(pool, 2, mongory_value_wrap_s(pool, "go"), mongory_value_wrap_s(pool, "ts"))
      : MG_ARRAY_WRAP(pool, 2, mongory_value_wrap_s(pool, "ruby"), mongory_value_wrap_s(pool, "c"));

    char name_buf[32];
    if (i % 5 == 0) {
      snprintf(name_buf, sizeof(name_buf), "user%c", "abcdef"[i % 6]);
    } else {
      snprintf(name_buf, sizeof(name_buf), "other_%d", i);
    }

    records[i] = MG_TABLE_WRAP(pool, 6,
      "id", mongory_value_wrap_i(pool, i),
      "name", mongory_value_wrap_s(pool, pool_strdup(pool, name_buf)),
      "age", mongory_value_wrap_i(pool, 10 + (i % 50)),
      "status", mongory_value_wrap_s(pool, (i % 3 == 0) ? "active" : "inactive"),
      "tags", tags,
      "profile", profile
    );
  }

  mongory_array *large_in_arr = mongory_array_new(pool);
  for (int i = 0; i < 500; i++) {
    char item[32];
    snprintf(item, sizeof(item), "user_%d", i);
    large_in_arr->push(large_in_arr, mongory_value_wrap_s(pool, pool_strdup(pool, item)));
  }
  mongory_value *large_in_list = mongory_value_wrap_a(pool, large_in_arr);

  bench_case cases[] = {
    {
      "simple",
      MG_TABLE_WRAP(pool, 1, "age", MG_TABLE_WRAP(pool, 1, "$gte", mongory_value_wrap_i(pool, 18))),
      plain_simple
    },
    {
      "or",
      MG_TABLE_WRAP(pool, 1, "$or", MG_ARRAY_WRAP(pool, 2,
        MG_TABLE_WRAP(pool, 1, "age", MG_TABLE_WRAP(pool, 1, "$gte", mongory_value_wrap_i(pool, 18))),
        MG_TABLE_WRAP(pool, 1, "status", mongory_value_wrap_s(pool, "active"))
      )),
      plain_or
    },
    {
      "nested_and",
      MG_TABLE_WRAP(pool, 3,
        "profile", MG_TABLE_WRAP(pool, 1, "city", mongory_value_wrap_s(pool, "taipei")),
        "status", mongory_value_wrap_s(pool, "active"),
        "age", MG_TABLE_WRAP(pool, 1, "$lt", mongory_value_wrap_i(pool, 40))
      ),
      plain_nested_and
    },
    {
      "in_array_field",
      MG_TABLE_WRAP(pool, 1, "tags", MG_TABLE_WRAP(pool, 1, "$in",
        MG_ARRAY_WRAP(pool, 2, mongory_value_wrap_s(pool, "go"), mongory_value_wrap_s(pool, "rust")))),
      plain_in_array_field
    },
    {
      "large_in_500",
      MG_TABLE_WRAP(pool, 1, "name", MG_TABLE_WRAP(pool, 1, "$in", large_in_list)),
      plain_large_in_500
    }
  };

  printf("mongory-core benchmark: %d records x %d rounds\n", BENCH_RECORDS, BENCH_ROUNDS);
  int num_cases = (int)(sizeof(cases) / sizeof(cases[0]));
  for (int c = 0; c < num_cases; c++) {
    mongory_matcher *matcher = mongory_matcher_new(pool, cases[c].cond, NULL);
    if (!matcher) {
      fprintf(stderr, "Failed to compile case %s\n", cases[c].name);
      return 1;
    }

    // Warmup
    for (int i = 0; i < BENCH_RECORDS; i++) {
      cases[c].plain(records[i], large_in_arr);
      mongory_matcher_match(matcher, records[i]);
    }

    struct timespec p0, p1, m0, m1;
    clock_gettime(CLOCK_MONOTONIC, &p0);
    int plain_matched = 0;
    for (int r = 0; r < BENCH_ROUNDS; r++) {
      int round_matched = 0;
      for (int i = 0; i < BENCH_RECORDS; i++) {
        if (cases[c].plain(records[i], large_in_arr)) round_matched++;
      }
      plain_matched = round_matched;
    }
    clock_gettime(CLOCK_MONOTONIC, &p1);

    clock_gettime(CLOCK_MONOTONIC, &m0);
    int matched = 0;
    for (int r = 0; r < BENCH_ROUNDS; r++) {
      int round_matched = 0;
      for (int i = 0; i < BENCH_RECORDS; i++) {
        if (mongory_matcher_match(matcher, records[i])) round_matched++;
      }
      matched = round_matched;
    }
    clock_gettime(CLOCK_MONOTONIC, &m1);

    double plain_ns = elapsed_ns(p0, p1) / ((double)BENCH_ROUNDS * BENCH_RECORDS);
    double mongory_ns = elapsed_ns(m0, m1) / ((double)BENCH_ROUNDS * BENCH_RECORDS);
    printf("%-16s plain=%6.1f ns/rec  mongory=%6.1f ns/rec  (ratio=%.2fx, matched=%d/%d)\n",
           cases[c].name, plain_ns, mongory_ns, mongory_ns / plain_ns, matched, plain_matched);
  }

  free(records);
  pool->free(pool);
  mongory_cleanup();
  return 0;
}
