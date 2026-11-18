#include <mongory-core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCH_RECORDS 10000
#define BENCH_ROUNDS 200

typedef struct {
  const char *name;
  mongory_value *cond;
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
      MG_TABLE_WRAP(pool, 1, "age", MG_TABLE_WRAP(pool, 1, "$gte", mongory_value_wrap_i(pool, 18)))
    },
    {
      "or",
      MG_TABLE_WRAP(pool, 1, "$or", MG_ARRAY_WRAP(pool, 2,
        MG_TABLE_WRAP(pool, 1, "age", MG_TABLE_WRAP(pool, 1, "$gte", mongory_value_wrap_i(pool, 18))),
        MG_TABLE_WRAP(pool, 1, "status", mongory_value_wrap_s(pool, "active"))
      ))
    },
    {
      "nested_and",
      MG_TABLE_WRAP(pool, 3,
        "profile", MG_TABLE_WRAP(pool, 1, "city", mongory_value_wrap_s(pool, "taipei")),
        "status", mongory_value_wrap_s(pool, "active"),
        "age", MG_TABLE_WRAP(pool, 1, "$lt", mongory_value_wrap_i(pool, 40))
      )
    },
    {
      "in_array_field",
      MG_TABLE_WRAP(pool, 1, "tags", MG_TABLE_WRAP(pool, 1, "$in",
        MG_ARRAY_WRAP(pool, 2, mongory_value_wrap_s(pool, "go"), mongory_value_wrap_s(pool, "rust"))))
    },
    {
      "large_in_500",
      MG_TABLE_WRAP(pool, 1, "name", MG_TABLE_WRAP(pool, 1, "$in", large_in_list))
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

    int matched = 0;
    for (int i = 0; i < BENCH_RECORDS; i++) {
      if (mongory_matcher_match(matcher, records[i])) matched++;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int r = 0; r < BENCH_ROUNDS; r++) {
      int round_matched = 0;
      for (int i = 0; i < BENCH_RECORDS; i++) {
        if (mongory_matcher_match(matcher, records[i])) round_matched++;
      }
      matched = round_matched;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double total_ns = elapsed_ns(t0, t1);
    double ns_per_rec = total_ns / ((double)BENCH_ROUNDS * BENCH_RECORDS);
    double ms_per_10k = (total_ns / 1e6) / BENCH_ROUNDS;
    printf("%-16s %7.1f ns/rec  %7.3f ms/10k  (matched=%d)\n",
           cases[c].name, ns_per_rec, ms_per_10k, matched);
  }

  free(records);
  pool->free(pool);
  mongory_cleanup();
  return 0;
}
