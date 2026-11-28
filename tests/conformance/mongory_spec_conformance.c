/**
 * @file mongory_spec_conformance.c
 * @brief Runs mongory-spec fixture files against mongory-core.
 *
 * Usage: mongory_spec_conformance <fixture.json> [<fixture.json> ...]
 *
 * Each fixture file is a JSON array of suites:
 *   { "description": ..., "condition": {...}, "records": [{ "data": ..., "expected": bool }] }
 *   { "description": ..., "condition": {...}, "error": true }
 *
 * This runner acts as a minimal language binding on top of mongory-core:
 * - Condition normalization (binding responsibility per mongory-spec README):
 *   dotted field paths ("a.b.c") are expanded into nested objects and escaped
 *   dots ("a\.b") are unescaped, recursively, with deep-merge of overlapping
 *   keys. This mirrors mongory-rb's ConditionConverter / KeyConverter.
 * - Regex engine: `$regex` is backed by PCRE2 (mongory-core is regex agnostic).
 */
#define PCRE2_CODE_UNIT_WIDTH 8

#include "../../src/test_helper/test_helper.h"
#include <mongory-core.h>
#include <pcre2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int total_assertions = 0;
static int total_failures = 0;

// Unity hooks required by test_helper's link dependency; unused by this runner.
void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// Binding: regex adapter (PCRE2)
// ============================================================================

static bool spec_regex_match_with_flags(mongory_value *pattern, uint32_t flags, mongory_value *value) {
  if (!pattern || pattern->type != MONGORY_TYPE_STRING || !value || value->type != MONGORY_TYPE_STRING) {
    return false;
  }
  int error_code = 0;
  PCRE2_SIZE error_offset = 0;
  pcre2_code *re = pcre2_compile((PCRE2_SPTR)pattern->data.s, PCRE2_ZERO_TERMINATED, PCRE2_UTF | flags, &error_code,
                                 &error_offset, NULL);
  if (!re) {
    return false;
  }
  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);
  pcre2_match_context *mctx = pcre2_match_context_create(NULL);
  if (mctx) {
    // Guard against catastrophic backtracking (ReDoS) on untrusted patterns/inputs.
    pcre2_set_match_limit(mctx, 100000);
    pcre2_set_depth_limit(mctx, 1000);
  }
  int rc = pcre2_match(re, (PCRE2_SPTR)value->data.s, strlen(value->data.s), 0, 0, match_data, mctx);
  if (mctx) {
    pcre2_match_context_free(mctx);
  }
  pcre2_match_data_free(match_data);
  pcre2_code_free(re);
  return rc >= 0;
}

static bool spec_regex_match(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value) {
  (void)pool;
  return spec_regex_match_with_flags(pattern, 0, value);
}

// `options` is validated by core: only i, m, s.
static bool spec_regex_options_match(mongory_memory_pool *pool, mongory_value *pattern, const char *options,
                                     mongory_value *value) {
  (void)pool;
  uint32_t flags = 0;
  for (const char *flag = options; *flag; flag++) {
    if (*flag == 'i') flags |= PCRE2_CASELESS;
    if (*flag == 'm') flags |= PCRE2_MULTILINE;
    if (*flag == 's') flags |= PCRE2_DOTALL;
  }
  return spec_regex_match_with_flags(pattern, flags, value);
}

// ============================================================================
// Fixture runner
// ============================================================================

static char *read_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *buffer = calloc(1, (size_t)size + 1);
  if (!buffer) {
    fclose(file);
    return NULL;
  }
  size_t read_size = fread(buffer, 1, (size_t)size, file);
  buffer[read_size] = '\0';
  fclose(file);
  return buffer;
}

static char *stringify(mongory_value *value, mongory_memory_pool *pool) {
  if (!value) {
    return "(missing)";
  }
  return value->to_str(value, pool);
}

static void report_failure(const char *file, const char *description, const char *message) {
  total_failures++;
  fprintf(stderr, "FAIL %s :: %s\n  %s\n", file, description, message);
}

typedef struct record_context {
  const char *file;
  const char *description;
  mongory_matcher *matcher;
  mongory_memory_pool *pool;
} record_context;

static bool run_record(mongory_value *record, void *acc) {
  record_context *ctx = (record_context *)acc;
  total_assertions++;

  if (!record || record->type != MONGORY_TYPE_TABLE) {
    report_failure(ctx->file, ctx->description, "record is not an object");
    return true;
  }
  mongory_value *data = record->data.t->get(record->data.t, "data");
  mongory_value *expected = record->data.t->get(record->data.t, "expected");
  if (!data || !expected || expected->type != MONGORY_TYPE_BOOL) {
    report_failure(ctx->file, ctx->description, "record requires `data` and boolean `expected`");
    return true;
  }

  bool actual = mongory_matcher_match(ctx->matcher, data);
  if (actual != expected->data.b) {
    char message[2048];
    snprintf(message, sizeof(message), "data=%s expected=%s got=%s", stringify(data, ctx->pool),
             expected->data.b ? "true" : "false", actual ? "true" : "false");
    report_failure(ctx->file, ctx->description, message);
  }
  return true;
}

typedef struct suite_context {
  const char *file;
} suite_context;

static bool run_suite(mongory_value *suite, void *acc) {
  suite_context *ctx = (suite_context *)acc;
  if (!suite || suite->type != MONGORY_TYPE_TABLE) {
    report_failure(ctx->file, "(unknown)", "suite is not an object");
    return true;
  }

  mongory_table *t = suite->data.t;
  mongory_value *description_value = t->get(t, "description");
  mongory_value *condition = t->get(t, "condition");
  mongory_value *records = t->get(t, "records");
  mongory_value *error_flag = t->get(t, "error");
  const char *description =
      (description_value && description_value->type == MONGORY_TYPE_STRING) ? description_value->data.s : "(no description)";
  bool expect_error = error_flag && error_flag->type == MONGORY_TYPE_BOOL && error_flag->data.b;

  if (!condition || condition->type != MONGORY_TYPE_TABLE) {
    report_failure(ctx->file, description, "`condition` must be an object");
    return true;
  }

  mongory_memory_pool *matcher_pool = mongory_memory_pool_new();
  mongory_matcher *matcher = mongory_matcher_new(matcher_pool, condition, NULL);
  bool build_failed = (matcher == NULL || matcher_pool->error != NULL);

  if (expect_error) {
    total_assertions++;
    if (!build_failed) {
      report_failure(ctx->file, description, "expected condition to be rejected, but matcher was built");
    }
  } else if (build_failed) {
    total_assertions++;
    char message[512];
    snprintf(message, sizeof(message), "failed to build matcher: %s",
             (matcher_pool->error && matcher_pool->error->message) ? matcher_pool->error->message : "(unknown error)");
    report_failure(ctx->file, description, message);
  } else if (!records || records->type != MONGORY_TYPE_ARRAY) {
    report_failure(ctx->file, description, "`records` must be an array");
  } else {
    record_context rctx = {ctx->file, description, matcher, matcher_pool};
    records->data.a->each(records->data.a, &rctx, run_record);
  }

  matcher_pool->free(matcher_pool);
  return true;
}

static void run_file(const char *path) {
  char *json = read_file(path);
  if (!json) {
    report_failure(path, "(file)", "unable to read fixture file");
    return;
  }

  mongory_memory_pool *file_pool = mongory_memory_pool_new();
  mongory_value *parsed = json_string_to_mongory_value(file_pool, json);
  free(json);

  if (!parsed || parsed->type != MONGORY_TYPE_ARRAY) {
    report_failure(path, "(file)", "fixture root must be a JSON array");
  } else {
    suite_context ctx = {path};
    parsed->data.a->each(parsed->data.a, &ctx, run_suite);
  }
  file_pool->free(file_pool);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <fixture.json> [<fixture.json> ...]\n", argv[0]);
    return 2;
  }

  mongory_init();
  mongory_regex_func_set(spec_regex_match);
  mongory_regex_options_func_set(spec_regex_options_match);

  for (int i = 1; i < argc; i++) {
    run_file(argv[i]);
  }

  mongory_cleanup();

  printf("mongory-spec conformance: %d assertions, %d failures\n", total_assertions, total_failures);
  return total_failures == 0 ? 0 : 1;
}
