#include "memory_pool.h"

#define MONGORY_INITIAL_CHUNK_SIZE 1024
#define MONGORY_ALIGN8(size) (((size) + 7) & ~((size_t)7))

struct mongory_memory_chunk {
  void *start;
  size_t capacity;
  size_t used;
  struct mongory_memory_chunk *next;
};

mongory_memory_chunk* mongory_memory_chunk_new(mongory_memory_pool *self) {
  void *mem = malloc(self->chunk_size);
  if (!mem) {
    return NULL;
  }

  mongory_memory_chunk *chunk = malloc(sizeof(mongory_memory_chunk));
  if (!chunk) {
    free(mem);
    return NULL;
  }

  chunk->start = mem;
  chunk->capacity = self->chunk_size;
  chunk->used = 0;
  chunk->next = NULL;

  return chunk;
}

bool mongory_memory_pool_add_chunk(mongory_memory_pool *self, size_t request_size) {
  self->chunk_size *= 2;
  while (request_size > self->chunk_size) {
    self->chunk_size *= 2;
  }

  mongory_memory_chunk *new_chunk = mongory_memory_chunk_new(self);
  if (!new_chunk) {
    return false;
  }
  self->current->next = new_chunk;
  self->current = new_chunk;

  return true;
}

void* mongory_memory_pool_alloc(mongory_memory_pool *self, size_t size) {
  size = MONGORY_ALIGN8(size);
  size_t balance = self->current->capacity - self->current->used;

  if (size > balance && !mongory_memory_pool_add_chunk(self, size)) {
    return NULL;
  }

  void *ptr = (char *)self->current->start + self->current->used;
  self->current->used += size;

  return ptr;
}

void mongory_memory_pool_destroy(mongory_memory_pool *self) {
  mongory_memory_chunk *chunk = self->head;
  while (chunk) {
    mongory_memory_chunk *next = chunk->next;
    free(chunk->start);
    free(chunk);
    chunk = next;
  }
  free(self);
}

mongory_memory_pool* mongory_memory_pool_new() {
  mongory_memory_pool *pool = malloc(sizeof(mongory_memory_pool));
  if (!pool) {
    return NULL;
  }
  pool->alloc = mongory_memory_pool_alloc;
  pool->free = mongory_memory_pool_destroy;
  pool->chunk_size = MONGORY_INITIAL_CHUNK_SIZE;
  mongory_memory_chunk *first_chunk = mongory_memory_chunk_new(pool);
  if (!first_chunk) {
    free(pool);
    return NULL;
  }
  pool->head = first_chunk;
  pool->current = first_chunk;

  return pool;
}
