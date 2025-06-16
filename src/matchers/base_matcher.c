#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <mongory-core.h>
#include "base_matcher.h"

mongory_matcher* mongory_matcher_base_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = pool->alloc(pool->ctx, sizeof(mongory_matcher));
  if (matcher == NULL) return NULL;
  matcher->context.original_match = NULL;
  matcher->context.trace = NULL;
  matcher->pool = pool;
  matcher->condition = condition;
  return matcher;
}

bool mongory_matcher_always_true_match(mongory_matcher *matcher, mongory_value *value) {
  (void)matcher; // Unused parameter
  (void)value; // Unused parameter
  return true; // Always matches
}

mongory_matcher* mongory_matcher_always_true_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  matcher->match = mongory_matcher_always_true_match;
  return matcher;
}

bool mongory_matcher_always_false_match(mongory_matcher *matcher, mongory_value *value) {
  (void)matcher; // Unused parameter
  (void)value; // Unused parameter
  return false; // Never matches
}

mongory_matcher* mongory_matcher_always_false_new(mongory_memory_pool *pool, mongory_value *condition) {
  mongory_matcher *matcher = mongory_matcher_base_new(pool, condition);
  matcher->match = mongory_matcher_always_false_match;
  return matcher;
}

bool mongory_try_parse_int(const char *key, int *out) {
    if (key == NULL || *key == '\0') return false;

    char *endptr = NULL;
    errno = 0;
    long val = strtol(key, &endptr, 10);

    if (*endptr != '\0') return false;
    if (errno == ERANGE || val < INT_MIN || val > INT_MAX) return false;

    *out = (int)val;
    return true;
}