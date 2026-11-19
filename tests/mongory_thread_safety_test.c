#include "../src/foundations/utils.h"
#include "../src/matchers/value_matcher.h"
#include "../src/test_helper/test_helper.h"
#include "mongory-core.h"
#include "unity.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
typedef HANDLE thread_t;
typedef DWORD thread_ret_t;
#  define THREAD_CALL WINAPI
#else
#  include <pthread.h>
typedef pthread_t thread_t;
typedef void *thread_ret_t;
#  define THREAD_CALL
#endif

#define NUM_THREADS 8
#define ITERATIONS_PER_THREAD 1000

void setUp(void) { setup_test_environment(); }

void tearDown(void) { teardown_test_environment(); }

static volatile int start_barrier = 0;

static void wait_for_start_gate(void) {
#if defined(_MSC_VER)
  while (InterlockedCompareExchange((LONG volatile *)&start_barrier, 0, 0) == 0) {
  }
#else
  while (__atomic_load_n(&start_barrier, __ATOMIC_ACQUIRE) == 0) {
  }
#endif
}

static void release_start_gate(void) {
#if defined(_MSC_VER)
  InterlockedExchange((LONG volatile *)&start_barrier, 1);
#else
  __atomic_store_n(&start_barrier, 1, __ATOMIC_RELEASE);
#endif
}

typedef struct worker_args {
  mongory_matcher *matcher;
  int thread_id;
  bool passed;
  int failures;
} worker_args;

static thread_ret_t THREAD_CALL concurrent_match_worker(void *arg) {
  worker_args *w = (worker_args *)arg;
  w->passed = true;
  w->failures = 0;

  mongory_memory_pool *scratch_pool = mongory_memory_pool_new();
  if (!scratch_pool) {
    w->passed = false;
    w->failures = 1;
    return 0;
  }

  wait_for_start_gate();

  for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
    // Hit both array records (triggering lazy implicit_array_matcher build on iteration 0)
    // and scalar records across all worker threads using isolated per-thread scratch pools.
    mongory_value *match_array_doc = MG_TABLE_WRAP(scratch_pool, 5,
      "tags", MG_ARRAY_WRAP(scratch_pool, 2,
        mongory_value_wrap_s(scratch_pool, "go"),
        mongory_value_wrap_s(scratch_pool, "ruby")
      ),
      "scores", MG_ARRAY_WRAP(scratch_pool, 3,
        mongory_value_wrap_i(scratch_pool, 3),
        mongory_value_wrap_i(scratch_pool, 6),
        mongory_value_wrap_i(scratch_pool, 9)
      ),
      "items", MG_ARRAY_WRAP(scratch_pool, 1,
        MG_TABLE_WRAP(scratch_pool, 2,
          "name", mongory_value_wrap_s(scratch_pool, "ok"),
          "count", mongory_value_wrap_i(scratch_pool, 3)
        )
      ),
      "score", MG_ARRAY_WRAP(scratch_pool, 2,
        mongory_value_wrap_i(scratch_pool, 12),
        mongory_value_wrap_i(scratch_pool, 15)
      ),
      "note", mongory_value_wrap_s(scratch_pool, "scalar")
    );

    if (!mongory_matcher_match(w->matcher, match_array_doc) || scratch_pool->error != NULL) {
      w->passed = false;
      w->failures++;
      scratch_pool->error = NULL;
    }

    // Scalar mismatching case
    mongory_value *mismatch_doc = MG_TABLE_WRAP(scratch_pool, 5,
      "tags", MG_ARRAY_WRAP(scratch_pool, 1, mongory_value_wrap_s(scratch_pool, "python")),
      "scores", MG_ARRAY_WRAP(scratch_pool, 2, mongory_value_wrap_i(scratch_pool, 1), mongory_value_wrap_i(scratch_pool, 9)),
      "items", mongory_value_wrap_a(scratch_pool, mongory_array_new(scratch_pool)),
      "score", mongory_value_wrap_i(scratch_pool, 5),
      "note", mongory_value_wrap_i(scratch_pool, 42)
    );

    if (mongory_matcher_match(w->matcher, mismatch_doc) || scratch_pool->error != NULL) {
      w->passed = false;
      w->failures++;
      scratch_pool->error = NULL;
    }

    // Match on scalar values for the same field matcher
    mongory_value *scalar_match_doc = MG_TABLE_WRAP(scratch_pool, 5,
      "tags", mongory_value_wrap_s(scratch_pool, "ruby"),
      "scores", mongory_value_wrap_i(scratch_pool, 6),
      "items", MG_ARRAY_WRAP(scratch_pool, 1,
        MG_TABLE_WRAP(scratch_pool, 2,
          "name", mongory_value_wrap_s(scratch_pool, "ok"),
          "count", mongory_value_wrap_i(scratch_pool, 2)
        )
      ),
      "score", mongory_value_wrap_i(scratch_pool, 20),
      "note", mongory_value_wrap_s(scratch_pool, "ok")
    );

    if (!mongory_matcher_match(w->matcher, scalar_match_doc) || scratch_pool->error != NULL) {
      w->passed = false;
      w->failures++;
      scratch_pool->error = NULL;
    }

    scratch_pool->reset(scratch_pool);
  }

  scratch_pool->free(scratch_pool);
  return 0;
}

static bool spawn_and_join_workers(thread_ret_t(THREAD_CALL *fn)(void *), worker_args *args, int count) {
  thread_t threads[NUM_THREADS];
#if defined(_WIN32)
  for (int i = 0; i < count; i++) {
    threads[i] = CreateThread(NULL, 0, fn, &args[i], 0, NULL);
    if (!threads[i]) return false;
  }
  release_start_gate();
  WaitForMultipleObjects((DWORD)count, threads, TRUE, INFINITE);
  for (int i = 0; i < count; i++) {
    CloseHandle(threads[i]);
  }
#else
  for (int i = 0; i < count; i++) {
    if (pthread_create(&threads[i], NULL, fn, &args[i]) != 0) return false;
  }
  release_start_gate();
  for (int i = 0; i < count; i++) {
    pthread_join(threads[i], NULL);
  }
#endif
  return true;
}

void test_concurrent_shared_matcher_with_lazy_array_build(void) {
  mongory_memory_pool *pool = get_test_pool();
  mongory_value *condition = MG_TABLE_WRAP(pool, 5,
    "tags", mongory_value_wrap_s(pool, "ruby"),
    "scores", MG_TABLE_WRAP(pool, 2,
      "$gt", mongory_value_wrap_i(pool, 5),
      "$lt", mongory_value_wrap_i(pool, 8)
    ),
    "items", MG_TABLE_WRAP(pool, 3,
      "name", mongory_value_wrap_s(pool, "ok"),
      "count", MG_TABLE_WRAP(pool, 1, "$gte", mongory_value_wrap_i(pool, 2)),
      "$size", MG_TABLE_WRAP(pool, 1, "$gte", mongory_value_wrap_i(pool, 1))
    ),
    "score", MG_TABLE_WRAP(pool, 1,
      "$not", MG_TABLE_WRAP(pool, 1, "$lt", mongory_value_wrap_i(pool, 10))
    ),
    "note", MG_TABLE_WRAP(pool, 1, "$in", MG_ARRAY_WRAP(pool, 2,
      mongory_value_wrap_s(pool, "scalar"),
      mongory_value_wrap_s(pool, "ok")
    ))
  );

  mongory_matcher *shared_matcher = mongory_matcher_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(shared_matcher);
  TEST_ASSERT_NULL(pool->error);

  start_barrier = 0;
  worker_args args[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; i++) {
    args[i].matcher = shared_matcher;
    args[i].thread_id = i;
    args[i].passed = false;
    args[i].failures = 0;
  }

  TEST_ASSERT_TRUE(spawn_and_join_workers(concurrent_match_worker, args, NUM_THREADS));

  for (int i = 0; i < NUM_THREADS; i++) {
    TEST_ASSERT_TRUE_MESSAGE(args[i].passed, "Worker encountered match mismatch or pool error");
    TEST_ASSERT_EQUAL_INT(0, args[i].failures);
  }
  TEST_ASSERT_NULL(pool->error);
}

static mongory_memory_pool *shared_matcher_pool_for_regex_test = NULL;

static bool scratch_allocating_regex_func(mongory_memory_pool *pool, mongory_value *pattern, mongory_value *value) {
  if (!pool || pool == shared_matcher_pool_for_regex_test) {
    return false; // Regex callback must receive the per-thread scratch pool, not shared matcher->pool.
  }
  char *buf = (char *)MG_ALLOC(pool, 32);
  if (!buf || !pattern || !value || pattern->type != MONGORY_TYPE_STRING || value->type != MONGORY_TYPE_STRING) {
    return false;
  }
  snprintf(buf, 32, "%s", pattern->data.s);
  return strstr(value->data.s, buf) != NULL;
}

static thread_ret_t THREAD_CALL concurrent_regex_worker(void *arg) {
  worker_args *w = (worker_args *)arg;
  w->passed = true;
  w->failures = 0;

  mongory_memory_pool *scratch_pool = mongory_memory_pool_new();
  if (!scratch_pool) {
    w->passed = false;
    w->failures = 1;
    return 0;
  }

  wait_for_start_gate();

  for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
    mongory_value *match_doc = MG_TABLE_WRAP(scratch_pool, 1,
      "tags", MG_ARRAY_WRAP(scratch_pool, 2,
        mongory_value_wrap_s(scratch_pool, "golang"),
        mongory_value_wrap_s(scratch_pool, "jruby")
      )
    );
    if (!mongory_matcher_match(w->matcher, match_doc)) {
      w->passed = false;
      w->failures++;
    }

    mongory_value *miss_doc = MG_TABLE_WRAP(scratch_pool, 1,
      "tags", MG_ARRAY_WRAP(scratch_pool, 1, mongory_value_wrap_s(scratch_pool, "python"))
    );
    if (mongory_matcher_match(w->matcher, miss_doc)) {
      w->passed = false;
      w->failures++;
    }

    scratch_pool->reset(scratch_pool);
  }

  scratch_pool->free(scratch_pool);
  return 0;
}

void test_concurrent_shared_regex_matcher_uses_scratch_pool(void) {
  mongory_memory_pool *pool = get_test_pool();
  shared_matcher_pool_for_regex_test = pool;
  mongory_regex_func_set(scratch_allocating_regex_func);

  mongory_value *condition = MG_TABLE_WRAP(pool, 1,
    "tags", MG_TABLE_WRAP(pool, 1, "$regex", mongory_value_wrap_s(pool, "ruby"))
  );
  mongory_matcher *shared_matcher = mongory_matcher_new(pool, condition, NULL);
  TEST_ASSERT_NOT_NULL(shared_matcher);

  start_barrier = 0;
  worker_args args[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; i++) {
    args[i].matcher = shared_matcher;
    args[i].thread_id = i;
    args[i].passed = false;
    args[i].failures = 0;
  }

  TEST_ASSERT_TRUE(spawn_and_join_workers(concurrent_regex_worker, args, NUM_THREADS));

  for (int i = 0; i < NUM_THREADS; i++) {
    TEST_ASSERT_TRUE(args[i].passed);
    TEST_ASSERT_EQUAL_INT(0, args[i].failures);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_concurrent_shared_matcher_with_lazy_array_build);
  RUN_TEST(test_concurrent_shared_regex_matcher_uses_scratch_pool);
  return UNITY_END();
}
