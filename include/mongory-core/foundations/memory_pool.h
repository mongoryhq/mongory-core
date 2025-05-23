#ifndef MONGORY_MEMORY_POOL
#define MONGORY_MEMORY_POOL

#include <stdbool.h>
#include <stdlib.h>

struct mongory_memory_chunk;
typedef struct mongory_memory_chunk mongory_memory_chunk;

typedef struct mongory_memory_pool_ctx {
  size_t chunk_size;
  mongory_memory_chunk *head;
  mongory_memory_chunk *current;
} mongory_memory_pool_ctx;

typedef struct mongory_memory_pool {
  void *(*alloc)(void *ctx, size_t size);
  void (*free)(struct mongory_memory_pool *self);
  void *ctx;
} mongory_memory_pool;

mongory_memory_pool* mongory_memory_pool_new();

#endif