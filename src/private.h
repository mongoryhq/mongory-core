#ifndef MONGORY_ITER
#define MONGORY_ITER
#include <stdbool.h>
#include <stdlib.h>

struct mongory_iterable;
typedef struct mongory_iterable mongory_iterable;
typedef bool (*mongory_iterable_callback_func)(void *item, void *acc);
typedef bool (*mongory_iterable_func)(mongory_iterable *self, void *acc, mongory_iterable_callback_func *callback);

struct mongory_iterable {
  size_t capacity;
  size_t count;
  void **items;
  mongory_iterable_func each;
};

inline bool mongory_iterable_each(mongory_iterable *self, void *acc, mongory_iterable_callback_func func) {
  for (size_t i = 0; i < self->count; i++) {
    void *item = self->items[i];
    if (!func(item, acc)) {
      return false;
    }
  }

  return true;
};

#endif