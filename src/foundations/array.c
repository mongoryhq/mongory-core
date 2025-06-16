#include <mongory-core/foundations/value.h>
#include <mongory-core/foundations/array.h>
#include <mongory-core/foundations/memory_pool.h>
#define MONGORY_ARRAY_INIT_SIZE 4

bool mongory_array_resize(mongory_array *self, size_t size) {
  mongory_value **new_items = self->pool->alloc(self->pool->ctx, sizeof(mongory_value *) * size);
  if (!new_items) {
    return false;
  }

  for (size_t i = 0; i < self->count; i++) {
    new_items[i] = self->items[i];
  }

  self->items = new_items;
  self->capacity = size;
  return true;
}

static inline bool mongory_array_grow_if_needed(mongory_array *self, size_t size) {
  if (size < self->capacity) {
    return true;
  }

  size_t new_capacity = self->capacity * 2;
  while (new_capacity <= size) {
    new_capacity *= 2;
  }

  return mongory_array_resize(self, new_capacity);
}

bool mongory_array_each(mongory_array *self, void *acc, mongory_array_callback_func func) {
  for (size_t i = 0; i < self->count; i++) {
    mongory_value *item = self->items[i];
    if (!func(item, acc)) {
      return false;
    }
  }

  return true;
}

bool mongory_array_push(mongory_array *self, mongory_value *value) {
  if (!mongory_array_grow_if_needed(self, self->count)) {
    return false;
  }

  self->items[self->count++] = value;
  return true;
}

mongory_value* mongory_array_get(mongory_array *self, size_t index) {
  if (index >= self->count) {
    return NULL;
  }
  return self->items[index];
}

bool mongory_array_set(mongory_array *self, size_t index, mongory_value *value) {
  if (!mongory_array_grow_if_needed(self, index + 1)) {
    return false;
  }

  if (index >= self->count) {
    for (size_t i = self->count; i < index; i++) {
      self->items[i] = NULL;
    }
    self->count = index + 1;
  }

  self->items[index] = value;
  return true;
}

mongory_array* mongory_array_new(mongory_memory_pool *pool) {
  mongory_value **items = pool->alloc(pool->ctx, sizeof(mongory_value *) * MONGORY_ARRAY_INIT_SIZE);
  if (!items) {
    return NULL;
  }

  mongory_array *array = pool->alloc(pool->ctx, sizeof(mongory_array));
  if (!array) {
    return NULL;
  }

  array->pool = pool;
  array->items = items;
  array->capacity = MONGORY_ARRAY_INIT_SIZE;
  array->count = 0;
  array->each = mongory_array_each;
  array->get = mongory_array_get;
  array->push = mongory_array_push;
  array->set = mongory_array_set;

  return array;
}
