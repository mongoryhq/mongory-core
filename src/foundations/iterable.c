#include "iterable.h"
#define MONGORY_ITERABLE_INIT_SIZE 4

mongory_iterable* mongory_iterable_new(mongory_memory_pool *pool) {
  void **items = pool->alloc(pool->ctx, sizeof(void *) * MONGORY_ITERABLE_INIT_SIZE);
  if (!items) {
    return NULL;
  }
  mongory_iterable *iter = pool->alloc(pool->ctx, sizeof(mongory_iterable));
  if (!iter) {
    return NULL;
  }

  iter->pool = pool;
  iter->items = items;
  iter->capacity = MONGORY_ITERABLE_INIT_SIZE;
  iter->count = 0;

  return iter;
}

bool mongory_iterable_each(mongory_iterable *self, void *acc, mongory_iterable_callback_func func) {
  for (size_t i = 0; i < self->count; i++) {
    void *item = self->items[i];
    if (!func(item, acc)) {
      return false;
    }
  }

  return true;
};

static inline bool mongory_iterable_grow_if_needed(mongory_iterable *iter, size_t size) {
  if (size < iter->capacity) {
    return true;
  }

  size_t new_capacity = iter->capacity * 2;
  while (new_capacity <= size) {
    new_capacity *= 2;
  }

  return mongory_iterable_resize(iter, new_capacity);
}

bool mongory_iterable_resize(mongory_iterable *iter, size_t size) {
  void **new_items = iter->pool->alloc(iter->pool->ctx, sizeof(void *) * size);
  if (!new_items) {
    return false;
  }

  for (size_t i = 0; i < iter->count; ++i) {
    new_items[i] = iter->items[i];
  }

  iter->items = new_items;
  iter->capacity = size;
  return true;
}

bool mongory_iterable_push(mongory_iterable *iter, void *item) {
  if (!mongory_iterable_grow_if_needed(iter, iter->count)) {
    return false;
  }

  iter->items[iter->count++] = item;
  return true;
}

void* mongory_iterable_get(mongory_iterable *iter, size_t index) {
  if (index >= iter->count) {
    return NULL;
  }
  return iter->items[index];
}

bool mongory_iterable_set(mongory_iterable *iter, size_t index, void *item) {
  if (!mongory_iterable_grow_if_needed(iter, index + 1)) {
    return false;
  }

  if (index >= iter->count) {
    for (size_t i = iter->count; i < index; i++) {
      iter->items[i] = NULL;
    }
    iter->count = index + 1;
  }

  iter->items[index] = item;
  return true;
}
