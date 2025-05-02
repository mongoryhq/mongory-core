#include <mongory-core/foundations/iterable.h>
#define MONGORY_ITERABLE_INIT_SIZE 4

mongory_iterable* mongory_iterable_new(mongory_memory_pool *pool) {
  void **items = pool->alloc(pool, sizeof(void *) * MONGORY_ITERABLE_INIT_SIZE);
  if (!items) {
    return NULL;
  }
  mongory_iterable *iter = pool->alloc(pool, sizeof(mongory_iterable));
  if (!iter) {
    return NULL;
  }

  iter->items = items;
  iter->capacity = MONGORY_ITERABLE_INIT_SIZE;
  iter->count = 0;

  return iter;
}

inline bool mongory_iterable_grow_if_needed(mongory_iterable *iter, mongory_memory_pool *pool, size_t size) {
  if (size < iter->capacity) {
    return true;
  }

  size_t new_capacity = iter->capacity * 2;
  while (new_capacity <= size) {
    new_capacity *= 2;
  }
  void **new_items = pool->alloc(pool, sizeof(void *) * new_capacity);
  if (!new_items) {
    return false;
  }

  for (size_t i = 0; i < iter->count; ++i) {
    new_items[i] = iter->items[i];
  }

  iter->items = new_items;
  iter->capacity = new_capacity;
  return true;
}

bool mongory_iterable_push(mongory_iterable *iter, mongory_memory_pool *pool, void *item) {
  if (!mongory_iterable_grow_if_needed(iter, pool, iter->count)) {
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

bool mongory_iterable_set(mongory_iterable *iter, mongory_memory_pool *pool, size_t index, void *item) {
  if (!mongory_iterable_grow_if_needed(iter, pool, index + 1)) {
    return false;
  }

  if (index >= iter->count) {
    for (size_t i = iter->count; i < index; ++i) {
      iter->items[i] = NULL;
    }
    iter->count = index + 1;
  }

  iter->items[index] = item;
  return true;
}
