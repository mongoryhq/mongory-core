#include "iterable.h"
#include <mongory-core/foundations/value.h>
#include <mongory-core/foundations/array.h>
#include <mongory-core/foundations/memory_pool.h>

bool mongory_array_each(mongory_array *self, void *acc, mongory_array_callback_func func) {
  return mongory_iterable_each((mongory_iterable *)self->base, acc, (mongory_iterable_callback_func)func);
}

bool mongory_array_push(mongory_array *self, mongory_value *value) {
  bool result = mongory_iterable_push((mongory_iterable *)self->base, value);
  self->count = ((mongory_iterable *)self->base)->count;
  self->capacity = ((mongory_iterable *)self->base)->capacity;
  return result;
}

mongory_value* mongory_array_get(mongory_array *self, size_t index) {
  return (mongory_value *)mongory_iterable_get((mongory_iterable *)self->base, index);
}

bool mongory_array_set(mongory_array *self, size_t index, mongory_value *value) {
  bool result = mongory_iterable_set((mongory_iterable *)self->base, index, value);
  self->count = ((mongory_iterable *)self->base)->count;
  self->capacity = ((mongory_iterable *)self->base)->capacity;
  return result;
}

mongory_array* mongory_array_new(mongory_memory_pool *pool) {
  mongory_iterable *iter = mongory_iterable_new(pool);
  if (!iter) {
    return NULL;
  }

  mongory_array *array = pool->alloc(pool->ctx, sizeof(mongory_array));
  if (!array) {
    return NULL;
  }

  array->pool = pool;
  array->base = iter;
  array->items = (mongory_value **)iter->items;
  array->capacity = iter->capacity;
  array->count = iter->count;
  array->each = mongory_array_each;
  array->get = mongory_array_get;
  array->push = mongory_array_push;
  array->set = mongory_array_set;

  return array;
}
