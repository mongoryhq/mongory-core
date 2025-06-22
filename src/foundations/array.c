#include "array_private.h"
#include <mongory-core/foundations/array.h>
#include <mongory-core/foundations/memory_pool.h>
#include <mongory-core/foundations/value.h>
#define MONGORY_ARRAY_INIT_SIZE 4

bool mongory_array_resize(mongory_array *self, size_t size) {
  mongory_array_private *internal = (mongory_array_private *)self;
  mongory_value **new_items =
      self->pool->alloc(self->pool->ctx, sizeof(mongory_value *) * size);
  if (!new_items) {
    return false;
  }

  for (size_t i = 0; i < self->count; i++) {
    new_items[i] = internal->items[i];
  }

  internal->items = new_items;
  internal->capacity = size;
  return true;
}

static inline bool mongory_array_grow_if_needed(mongory_array *self,
                                                size_t size) {
  mongory_array_private *internal = (mongory_array_private *)self;
  if (size < internal->capacity) {
    return true;
  }

  size_t new_capacity = internal->capacity * 2;
  while (new_capacity <= size) {
    new_capacity *= 2;
  }

  return mongory_array_resize(self, new_capacity);
}

static inline bool mongory_array_each(mongory_array *self, void *acc,
                                      mongory_array_callback_func func) {
  mongory_array_private *internal = (mongory_array_private *)self;
  for (size_t i = 0; i < self->count; i++) {
    mongory_value *item = internal->items[i];
    if (!func(item, acc)) {
      return false;
    }
  }

  return true;
}

static inline bool mongory_array_push(mongory_array *self,
                                      mongory_value *value) {
  mongory_array_private *internal = (mongory_array_private *)self;
  if (!mongory_array_grow_if_needed(self, self->count)) {
    return false;
  }

  internal->items[self->count++] = value;
  return true;
}

static inline mongory_value *mongory_array_get(mongory_array *self,
                                               size_t index) {
  mongory_array_private *internal = (mongory_array_private *)self;
  if (index >= self->count) {
    return NULL;
  }
  return internal->items[index];
}

static inline bool mongory_array_set(mongory_array *self, size_t index,
                                     mongory_value *value) {
  mongory_array_private *internal = (mongory_array_private *)self;
  if (!mongory_array_grow_if_needed(self, index + 1)) {
    return false;
  }

  if (index >= self->count) {
    for (size_t i = self->count; i < index; i++) {
      internal->items[i] = NULL;
    }
    self->count = index + 1;
  }

  internal->items[index] = value;
  return true;
}

mongory_array *mongory_array_new(mongory_memory_pool *pool) {
  mongory_value **items =
      pool->alloc(pool->ctx, sizeof(mongory_value *) * MONGORY_ARRAY_INIT_SIZE);
  if (!items) {
    return NULL;
  }

  mongory_array_private *internal =
      pool->alloc(pool->ctx, sizeof(mongory_array_private));
  if (!internal) {
    return NULL;
  }

  internal->base.pool = pool;
  internal->items = items;
  internal->capacity = MONGORY_ARRAY_INIT_SIZE;
  internal->base.count = 0;
  internal->base.each = mongory_array_each;
  internal->base.get = mongory_array_get;
  internal->base.push = mongory_array_push;
  internal->base.set = mongory_array_set;

  return &internal->base;
}
