#ifndef MONGORY_ARRAY_PRIVATE_H
#define MONGORY_ARRAY_PRIVATE_H
#include <stdbool.h>
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include "mongory-core/foundations/array.h"

bool mongory_array_resize(mongory_array *self, size_t size); // resize array

typedef struct mongory_array_private {
  mongory_array base; // public array
  mongory_value **items; // items pointer
  size_t capacity; // capacity
} mongory_array_private;

#endif // MONGORY_ARRAY_PRIVATE_H