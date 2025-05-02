#ifndef MONGORY_MEMORY_POOL
#define MONGORY_MEMORY_POOL

#include <stdbool.h>
#include <stdlib.h>

struct mongory_memory_pool;
typedef struct mongory_memory_pool mongory_memory_pool;

struct mongory_memory_chunk;
typedef struct mongory_memory_chunk mongory_memory_chunk;

typedef void* (*mongory_memory_pool_alloc_func)(mongory_memory_pool *self, size_t size);
typedef void (*mongory_memory_pool_destroy_func)(mongory_memory_pool *self);

struct mongory_memory_pool {
  mongory_memory_chunk *head;
  mongory_memory_chunk *current;
  size_t chunk_size;
  mongory_memory_pool_alloc_func alloc;
  mongory_memory_pool_destroy_func free;
};

mongory_memory_pool* mongory_memory_pool_new();

#endif