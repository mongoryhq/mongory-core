#ifndef MONGORY_MEMORY_POOL
#define MONGORY_MEMORY_POOL
#include <stdbool.h>
#include <stdlib.h>
#include "mongory-core/foundations/error.h"

typedef struct mongory_memory_pool {
  void *(*alloc)(void *ctx, size_t size); // alloc function
  void (*trace)(void *ctx, void *ptr, size_t size); // trace function
  void (*free)(struct mongory_memory_pool *self); // free function
  void *ctx; // context
  mongory_error *error; // error
} mongory_memory_pool;

mongory_memory_pool* mongory_memory_pool_new(); // create new memory pool

#endif
