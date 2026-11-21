#include "../src/test_helper/test_helper.h"
#include <mongory-core.h>
#include <mongory-core/adapters/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void setUp(void) {}
void tearDown(void) {}

#define JSON_BENCH_RECORDS 10000
#define JSON_BENCH_ROUNDS 20

static double elapsed_ms(struct timespec a, struct timespec b) {
  return (double)(b.tv_sec - a.tv_sec) * 1e3 + (double)(b.tv_nsec - a.tv_nsec) / 1e6;
}

int main(void) {
  mongory_init();

  // Realistic wide JSON documents (20+ fields) where the query only inspects 2 fields.
  char **docs = (char **)malloc(sizeof(char *) * JSON_BENCH_RECORDS);
  size_t *lens = (size_t *)malloc(sizeof(size_t) * JSON_BENCH_RECORDS);
  for (int i = 0; i < JSON_BENCH_RECORDS; i++) {
    char buf[1024];
    int n = snprintf(
        buf, sizeof(buf),
        "{\"id\":%d,\"status\":\"%s\",\"age\":%d,\"email\":\"user%d@example.com\","
        "\"name\":\"User Number %d\",\"country\":\"TW\",\"currency\":\"TWD\","
        "\"tier\":\"%s\",\"score\":%d,\"verified\":%s,"
        "\"profile\":{\"city\":\"%s\",\"zip\":\"100\",\"bio\":\"some longer profile description text here\"},"
        "\"tags\":[\"ruby\",\"go\",\"c\",\"python\",\"ts\"],"
        "\"meta\":{\"source\":\"web\",\"campaign\":\"spring\",\"v\":2,\"flags\":[1,2,3,4]}}",
        i, (i % 3 == 0) ? "active" : "inactive", 10 + (i % 50), i, i,
        (i % 5 == 0) ? "vip" : "standard", (i * 7) % 100, (i % 2 == 0) ? "true" : "false",
        (i % 2 == 0) ? "taipei" : "tokyo");
    docs[i] = (char *)malloc((size_t)n + 1);
    memcpy(docs[i], buf, (size_t)n + 1);
    lens[i] = (size_t)n;
  }

  mongory_memory_pool *cond_pool = mongory_memory_pool_new();
  const char *cond = "{\"status\":\"active\",\"age\":{\"$gte\":30}}";
  mongory_matcher *matcher = mongory_matcher_new_from_json(cond_pool, cond, strlen(cond), NULL);
  mongory_json_match_ctx *ctx = mongory_json_match_ctx_new();

  // 1. Full deep convert (JSON -> full mongory_table -> match)
  struct timespec t0, t1;
  int deep_matched = 0;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (int r = 0; r < JSON_BENCH_ROUNDS; r++) {
    deep_matched = 0;
    for (int i = 0; i < JSON_BENCH_RECORDS; i++) {
      mongory_memory_pool *p = mongory_memory_pool_new();
      mongory_value *val = json_string_to_mongory_value(p, docs[i]);
      if (val && mongory_matcher_match(matcher, val)) {
        deep_matched++;
      }
      p->free(p);
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double deep_ms = elapsed_ms(t0, t1);

  // 2. Shallow wrapper with reusable ctx (yyjson + lazy wrap, zero per-record malloc)
  int shallow_matched = 0;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (int r = 0; r < JSON_BENCH_ROUNDS; r++) {
    shallow_matched = 0;
    for (int i = 0; i < JSON_BENCH_RECORDS; i++) {
      if (mongory_matcher_match_json_with_ctx(matcher, ctx, docs[i], lens[i])) {
        shallow_matched++;
      }
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double shallow_ms = elapsed_ms(t0, t1);

  double total_records = (double)JSON_BENCH_RECORDS * JSON_BENCH_ROUNDS;
  printf("Raw JSON filter benchmark (%d wide records x %d rounds):\n", JSON_BENCH_RECORDS, JSON_BENCH_ROUNDS);
  printf("  deep_convert + match : %7.2f ms (%6.1f ns/rec, matched=%d)\n",
         deep_ms, deep_ms * 1e6 / total_records, deep_matched);
  printf("  shallow_json_ctx     : %7.2f ms (%6.1f ns/rec, matched=%d)\n",
         shallow_ms, shallow_ms * 1e6 / total_records, shallow_matched);
  printf("  speedup              : %.2fx faster\n", deep_ms / shallow_ms);

  mongory_json_match_ctx_free(ctx);
  cond_pool->free(cond_pool);
  for (int i = 0; i < JSON_BENCH_RECORDS; i++) free(docs[i]);
  free(docs);
  free(lens);
  mongory_cleanup();
  return (deep_matched == shallow_matched) ? 0 : 1;
}
