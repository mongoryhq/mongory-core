#include <stdio.h>
#include <string.h>
#include <mongory-core/foundations/memory_pool.h>

#define MONGORY_INITIAL_CHUNK_SIZE 256
#define MONGORY_ALIGN8(size) (((size) + 7) & ~((size_t)7))

typedef struct mongory_memory_node {
  void *ptr;
  size_t size;
  size_t used;
  struct mongory_memory_node *next;
} mongory_memory_node;
 
typedef struct mongory_memory_pool_ctx {
  size_t chunk_size;
  mongory_memory_node *head;
  mongory_memory_node *current;
  mongory_memory_node *extra;
} mongory_memory_pool_ctx;

static inline mongory_memory_node* mongory_memory_chunk_new(size_t chunk_size) {
  mongory_memory_node *node = calloc(1, sizeof(mongory_memory_node));
  if (!node) {
    return NULL;
  }

  void *mem = calloc(1, chunk_size);
  if (!mem) {
    free(node);
    return NULL;
  }

  node->ptr = mem;
  node->size = chunk_size;
  node->used = 0;
  node->next = NULL;

  return node;
}

static inline bool mongory_memory_pool_grow(mongory_memory_pool_ctx *ctx, size_t request_size) {
  ctx->chunk_size *= 2;
  while (request_size > ctx->chunk_size) {
    ctx->chunk_size *= 2;
  }

  mongory_memory_node *new_chunk = mongory_memory_chunk_new(ctx->chunk_size);
  if (!new_chunk) {
    return false;
  }
  ctx->current->next = new_chunk;
  ctx->current = new_chunk;

  return true;
}

static inline void* mongory_memory_pool_alloc(void *ctx, size_t size) {
  mongory_memory_pool_ctx *pool_ctx = (mongory_memory_pool_ctx *)ctx;
  size = MONGORY_ALIGN8(size);
  size_t balance = pool_ctx->current->size - pool_ctx->current->used;
  if (size > balance && !mongory_memory_pool_grow(pool_ctx, size)) {
    return NULL;
  }

  void *ptr = (char *)pool_ctx->current->ptr + pool_ctx->current->used;
  pool_ctx->current->used += size;

  return ptr;
}

static inline void mongory_memory_pool_node_list_free(mongory_memory_node *head) {
  mongory_memory_node *node = head;
  while (node) {
    mongory_memory_node *next = node->next;
    if (node->ptr) {
      memset(node->ptr, 0, node->size); // Clear memory for safety
      free(node->ptr);
    }
    memset(node, 0, sizeof(mongory_memory_node)); // Clear the node structure
    free(node);
    node = next;
  }
}

static inline void mongory_memory_pool_destroy(mongory_memory_pool *pool) {
  mongory_memory_pool_ctx *ctx = (mongory_memory_pool_ctx *)pool->ctx;
  mongory_memory_pool_node_list_free(ctx->head);
  mongory_memory_pool_node_list_free(ctx->extra);

  memset(ctx, 0, sizeof(mongory_memory_pool_ctx)); // Clear the context structure
  free(ctx);
  memset(pool, 0, sizeof(mongory_memory_pool)); // Clear the pool structure
  free(pool);
}

static inline void mongory_memory_pool_trace(void *ctx, void *ptr, size_t size) {
  mongory_memory_pool_ctx *pool_ctx = (mongory_memory_pool_ctx *)ctx;
  mongory_memory_node *extra_alloc_tracer = calloc(1, sizeof(mongory_memory_node));
  if (!extra_alloc_tracer) {
    return; // Handle allocation failure
  }
  extra_alloc_tracer->ptr = ptr;
  extra_alloc_tracer->size = size;
  extra_alloc_tracer->next = pool_ctx->extra;
  pool_ctx->extra = extra_alloc_tracer;
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

  mongory_memory_node *first_chunk = mongory_memory_chunk_new(MONGORY_INITIAL_CHUNK_SIZE);
  if (!first_chunk) {
    free(ctx);
    free(pool);
    return NULL;
  }

  ctx->chunk_size = MONGORY_INITIAL_CHUNK_SIZE;
  ctx->head = first_chunk;
  ctx->current = first_chunk;
  ctx->extra = NULL;

  pool->ctx = ctx;
  pool->alloc = mongory_memory_pool_alloc;
  pool->free = mongory_memory_pool_destroy;
  pool->trace = mongory_memory_pool_trace;
  pool->error = NULL;

  return pool;
}
