#include <stdio.h>
#include <string.h>
#include <mongory-core/foundations/memory_pool.h>

#define MONGORY_INITIAL_CHUNK_SIZE 1024
#define MONGORY_ALIGN8(size) (((size) + 7) & ~((size_t)7))

typedef struct mongory_memory_chunk {
  void *start;
  size_t capacity;
  size_t used;
  struct mongory_memory_chunk *next;
} mongory_memory_chunk;

typedef struct mongory_memory_pool_ctx {
  size_t chunk_size;
  mongory_memory_chunk *head;
  mongory_memory_chunk *current;
} mongory_memory_pool_ctx;

mongory_memory_chunk* mongory_memory_chunk_new(size_t chunk_size) {
  mongory_memory_chunk *chunk = calloc(1, sizeof(mongory_memory_chunk));
  if (!chunk) {
    return NULL;
  }

  void *mem = calloc(1, chunk_size);
  if (!mem) {
    free(chunk);
    return NULL;
  }

  chunk->start = mem;
  chunk->capacity = chunk_size;
  chunk->used = 0;
  chunk->next = NULL;

  return chunk;
}

bool mongory_memory_pool_add_chunk(mongory_memory_pool_ctx *ctx, size_t request_size) {
  ctx->chunk_size *= 2;
  while (request_size > ctx->chunk_size) {
    ctx->chunk_size *= 2;
  }

  mongory_memory_chunk *new_chunk = mongory_memory_chunk_new(ctx->chunk_size);
  if (!new_chunk) {
    return false;
  }
  ctx->current->next = new_chunk;
  ctx->current = new_chunk;

  return true;
}

void* mongory_memory_pool_alloc(void *ctx, size_t size) {
  mongory_memory_pool_ctx *pool_ctx = (mongory_memory_pool_ctx *)ctx;
  size = MONGORY_ALIGN8(size);
  size_t balance = pool_ctx->current->capacity - pool_ctx->current->used;
  if (size > balance && !mongory_memory_pool_add_chunk(pool_ctx, size)) {
    return NULL;
  }

  void *ptr = (char *)pool_ctx->current->start + pool_ctx->current->used;
  pool_ctx->current->used += size;

  return ptr;
}

void mongory_memory_pool_destroy(mongory_memory_pool *pool) {
  mongory_memory_pool_ctx *ctx = (mongory_memory_pool_ctx *)pool->ctx;
  mongory_memory_chunk *chunk = ctx->head;
  while (chunk) {
    mongory_memory_chunk *next = chunk->next;
    if (chunk->start) {
      memset(chunk->start, 0, chunk->capacity); // Clear memory for safety
      free(chunk->start);
    }
    memset(chunk, 0, sizeof(mongory_memory_chunk)); // Clear the chunk structure
    free(chunk);
    chunk = next;
  }
  memset(ctx, 0, sizeof(mongory_memory_pool_ctx)); // Clear the context structure
  free(ctx);
  memset(pool, 0, sizeof(mongory_memory_pool)); // Clear the pool structure
  free(pool);
}

mongory_memory_pool* mongory_memory_pool_new() {
  mongory_memory_pool *pool = calloc(1, sizeof(mongory_memory_pool));
  if (!pool) {
    return NULL;
  }

  mongory_memory_pool_ctx *ctx = calloc(1, sizeof(mongory_memory_pool_ctx));
  if (!ctx) {
    free(pool);
    return NULL;
  }

  mongory_memory_chunk *first_chunk = mongory_memory_chunk_new(MONGORY_INITIAL_CHUNK_SIZE);
  if (!first_chunk) {
    free(ctx);
    free(pool);
    return NULL;
  }

  ctx->chunk_size = MONGORY_INITIAL_CHUNK_SIZE;
  ctx->head = first_chunk;
  ctx->current = first_chunk;

  pool->ctx = ctx;
  pool->alloc = mongory_memory_pool_alloc;
  pool->free = mongory_memory_pool_destroy;
  pool->error = NULL;

  return pool;
}
