#ifndef MONGORY_ITER
#define MONGORY_ITER
#include <stdbool.h>
#include <stdlib.h>
#include <mongory-core/foundations/memory_pool.h>

struct mongory_iterable;
typedef struct mongory_iterable mongory_iterable;
typedef bool (*mongory_iterable_callback_func)(void *item, void *acc);
typedef bool (*mongory_iterable_func)(mongory_iterable *self, void *acc, mongory_iterable_callback_func *callback);

struct mongory_iterable {
  mongory_memory_pool *pool;
  size_t capacity;
  size_t count;
  void **items;
};

mongory_iterable* mongory_iterable_new(mongory_memory_pool *pool);
bool mongory_iterable_push(mongory_iterable *iter, void *item);
void* mongory_iterable_get(mongory_iterable *iter, size_t index);
bool mongory_iterable_set(mongory_iterable *iter, size_t index, void *item);
bool mongory_iterable_each(mongory_iterable *self, void *acc, mongory_iterable_callback_func func);
bool mongory_iterable_resize(mongory_iterable *iter, size_t size);

#endif