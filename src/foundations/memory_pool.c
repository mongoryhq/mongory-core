/**
 * @file memory_pool.c
 * @brief Implements a memory pool for the Mongory library.
 *
 * This implementation uses a linked list of memory chunks. Allocations are
 * made from the current chunk. If the current chunk is full, a new, larger
 * chunk is allocated and added to the list. Freeing the pool deallocates all
 * chunks. It also supports tracing externally allocated memory.
 */
#if !defined(_WIN32) && (!defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L)
#  define _POSIX_C_SOURCE 200809L
#endif

#include "memory_pool_private.h"
#include <mongory-core/foundations/memory_pool.h>
#include <stdint.h> // For SIZE_MAX
#include <stdio.h>  // For NULL, though stdlib.h or stddef.h is more common
#include <stdlib.h> // For calloc, free, etc.
#include <string.h> // For memset

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
typedef CRITICAL_SECTION mongory_mutex_t;
static inline void mongory_mutex_init(mongory_mutex_t *mtx) { InitializeCriticalSection(mtx); }
static inline void mongory_mutex_lock(mongory_mutex_t *mtx) { EnterCriticalSection(mtx); }
static inline void mongory_mutex_unlock(mongory_mutex_t *mtx) { LeaveCriticalSection(mtx); }
static inline void mongory_mutex_destroy(mongory_mutex_t *mtx) { DeleteCriticalSection(mtx); }
#else
#  include <pthread.h>
typedef pthread_mutex_t mongory_mutex_t;
static inline void mongory_mutex_init(mongory_mutex_t *mtx) {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(mtx, &attr);
  pthread_mutexattr_destroy(&attr);
}
static inline void mongory_mutex_lock(mongory_mutex_t *mtx) { pthread_mutex_lock(mtx); }
static inline void mongory_mutex_unlock(mongory_mutex_t *mtx) { pthread_mutex_unlock(mtx); }
static inline void mongory_mutex_destroy(mongory_mutex_t *mtx) { pthread_mutex_destroy(mtx); }
#endif

/**
 * @def MONGORY_INITIAL_CHUNK_SIZE
 * @brief The initial size in bytes for the first memory chunk allocated by the
 * pool.
 */
#define MONGORY_INITIAL_CHUNK_SIZE 2048

/**
 * @def MONGORY_ALIGN8(size)
 * @brief Macro to align a given size to an 8-byte boundary.
 * This is useful for ensuring that memory allocations are suitably aligned
 * for various data types.
 * @param size The original size.
 * @return The size aligned up to the nearest multiple of 8.
 */
#define MONGORY_ALIGN8(size) (((size) + 7) & ~((size_t)7))

/**
 * @struct mongory_memory_node
 * @brief Represents a node in the linked list of memory chunks within the pool.
 *
 * Each node holds a pointer to a block of memory (`ptr`), its total `size`,
 * how much of it is `used`, and a pointer to the `next` node in the list.
 * This structure is also used by the tracing mechanism to track external
 * allocations.
 */
typedef struct mongory_memory_node {
  void *ptr;                        /**< Pointer to the allocated memory block. */
  size_t size;                      /**< Total size of the memory block. */
  size_t used;                      /**< Bytes used in this memory block. */
  struct mongory_memory_node *next; /**< Pointer to the next memory node. */
} mongory_memory_node;

/**
 * @struct mongory_memory_pool_ctx
 * @brief Internal context for a mongory_memory_pool.
 *
 * Stores the current `chunk_size` (which doubles on growth), pointers to
 * the `head` and `current` memory nodes for allocations, and a list (`extra`)
 * for traced external allocations.
 */
typedef struct mongory_memory_pool_ctx {
  size_t chunk_size;            /**< Current preferred size for new chunks. */
  size_t max_chunk_size;        /**< Upper cap for geometric chunk doubling. */
  size_t max_total_bytes;       /**< Max cumulative chunk bytes (0 = unlimited). */
  size_t total_bytes;           /**< Total bytes allocated across arena chunks. */
  mongory_allocator allocator;  /**< Underlying heap allocator hooks. */
  mongory_memory_node *head;    /**< Head of the list of memory chunks. */
  mongory_memory_node *current; /**< Current chunk to allocate from. */
  mongory_memory_node *extra;   /**< Head of list for externally traced memory. */
  mongory_mutex_t build_lock;   /**< Recursive lock for lazy matcher construction. */
} mongory_memory_pool_ctx;

static void *mongory_default_alloc(size_t size, void *user_data) {
  (void)user_data;
  return calloc(1, size);
}

static void mongory_default_free(void *ptr, void *user_data) {
  (void)user_data;
  free(ptr);
}

static inline void *mongory_allocator_zalloc(const mongory_allocator *alloc, size_t size) {
  if (!alloc || !alloc->alloc) {
    return NULL;
  }
  void *ptr = alloc->alloc(size, alloc->user_data);
  if (ptr && alloc->alloc != mongory_default_alloc) {
    memset(ptr, 0, size);
  }
  return ptr;
}

static inline void mongory_allocator_free(const mongory_allocator *alloc, void *ptr) {
  if (ptr && alloc && alloc->free) {
    alloc->free(ptr, alloc->user_data);
  }
}

/**
 * @brief Allocates a new memory chunk (node and its associated memory block).
 *
 * Uses zero-initialized allocation from `allocator`.
 *
 * @param allocator Allocator hooks to use.
 * @param chunk_size The size of the memory block to allocate for this chunk.
 * @return mongory_memory_node* Pointer to the new memory node, or NULL on
 * failure.
 */
static inline mongory_memory_node *mongory_memory_chunk_new(const mongory_allocator *allocator, size_t chunk_size) {
  mongory_memory_node *node = (mongory_memory_node *)mongory_allocator_zalloc(allocator, sizeof(mongory_memory_node));
  if (!node) {
    return NULL; // Failed to allocate node structure.
  }

  void *mem = mongory_allocator_zalloc(allocator, chunk_size);
  if (!mem) {
    mongory_allocator_free(allocator, node); // Clean up allocated node structure.
    return NULL; // Failed to allocate memory block for the chunk.
  }

  node->ptr = mem;
  node->size = chunk_size;
  node->used = 0;
  node->next = NULL;

  return node;
}

/**
 * @brief Grows the memory pool by advancing to a reusable chunk or adding a new chunk.
 *
 * Geometric growth is capped at `ctx->max_chunk_size` and total cumulative bytes
 * are bounded by `ctx->max_total_bytes` when non-zero.
 *
 * @param ctx Pointer to the memory pool's context.
 * @param request_size The minimum size required from the new chunk for an
 * upcoming allocation.
 * @return true if growth was successful, false otherwise.
 */
static inline bool mongory_memory_pool_grow(mongory_memory_pool_ctx *ctx, size_t request_size) {
  // Reuse already-allocated chunks after reset() if one has enough capacity.
  while (ctx->current->next) {
    ctx->current = ctx->current->next;
    if (ctx->current->size - ctx->current->used >= request_size) {
      return true;
    }
  }

  size_t next_chunk_size = ctx->chunk_size;
  size_t cap = ctx->max_chunk_size > 0 ? ctx->max_chunk_size : MONGORY_DEFAULT_MAX_CHUNK_SIZE;
  if (next_chunk_size < cap) {
    if (next_chunk_size > cap / 2) {
      next_chunk_size = cap;
    } else {
      next_chunk_size *= 2;
    }
  }
  size_t alloc_size = next_chunk_size >= request_size ? next_chunk_size : request_size;

  if (ctx->max_total_bytes > 0) {
    if (alloc_size > ctx->max_total_bytes || ctx->total_bytes > ctx->max_total_bytes - alloc_size) {
      // Try exact request_size if geometric growth overshot the budget.
      if (request_size <= ctx->max_total_bytes && ctx->total_bytes <= ctx->max_total_bytes - request_size) {
        alloc_size = request_size;
      } else {
        return false;
      }
    }
  }

  mongory_memory_node *new_chunk = mongory_memory_chunk_new(&ctx->allocator, alloc_size);
  if (!new_chunk) {
    return false; // Failed to create a new chunk.
  }

  ctx->chunk_size = next_chunk_size;
  ctx->total_bytes += alloc_size;

  // Link the new chunk and update the current pointer.
  ctx->current->next = new_chunk;
  ctx->current = new_chunk;

  return true;
}

/**
 * @brief Allocates memory from the pool. Implements `pool->alloc`.
 *
 * Allocates `size` bytes (aligned to 8 bytes) from the `current` chunk.
 * If the current chunk doesn't have enough space, `mongory_memory_pool_grow`
 * is called to add a new chunk.
 *
 * @param pool Pointer to the `mongory_memory_pool`.
 * @param size The number of bytes to allocate.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
static inline void *mongory_memory_pool_alloc(mongory_memory_pool *pool, size_t size) {
  if (!pool || !pool->ctx) {
    return NULL;
  }
  if (size > SIZE_MAX - 7) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return NULL;
  }
  mongory_memory_pool_ctx *pool_ctx = (mongory_memory_pool_ctx *)pool->ctx;
  size = MONGORY_ALIGN8(size); // Ensure 8-byte alignment.

  size_t balance = pool_ctx->current->size - pool_ctx->current->used;
  if (size > balance) {
    // Not enough space in current chunk, try to grow.
    if (!mongory_memory_pool_grow(pool_ctx, size)) {
      pool->error = &MONGORY_ALLOC_ERROR;
      return NULL; // Growth failed.
    }
    // After successful growth, pool_ctx->current points to a chunk with >= size bytes.
  }

  // Allocate from the current chunk.
  void *ptr = (char *)pool_ctx->current->ptr + pool_ctx->current->used;
  pool_ctx->current->used += size;

  return ptr;
}

static inline void mongory_memory_pool_reset(mongory_memory_pool *pool) {
  if (!pool || !pool->ctx) {
    return;
  }
  mongory_memory_pool_ctx *pool_ctx = (mongory_memory_pool_ctx *)pool->ctx;
  pool_ctx->current = pool_ctx->head;
  mongory_memory_node *node = pool_ctx->head;
  while (node) {
    node->used = 0;
    node = node->next;
  }
}

/**
 * @brief Frees a linked list of memory nodes.
 *
 * Iterates through the list, freeing the memory block (`node->ptr`) and then
 * the node structure itself for each node. Memory blocks are zeroed out before
 * freeing for security/safety.
 *
 * @param head Pointer to the head of the memory node list to free.
 * @param node_alloc Allocator for the node structs themselves.
 * @param data_alloc Allocator for `node->ptr` (may differ for traced external pointers).
 */
static inline void mongory_memory_pool_node_list_free(mongory_memory_node *head,
                                                      const mongory_allocator *node_alloc,
                                                      const mongory_allocator *data_alloc) {
  mongory_memory_node *node = head;
  while (node) {
    mongory_memory_node *next = node->next;
    if (node->ptr) {
      memset(node->ptr, 0, node->size); // Clear memory for safety.
      if (data_alloc) {
        mongory_allocator_free(data_alloc, node->ptr);
      } else {
        free(node->ptr);
      }
    }
    memset(node, 0,
           sizeof(mongory_memory_node)); // Clear the node structure itself.
    mongory_allocator_free(node_alloc, node); // Free the node structure.
    node = next;
  }
}

/**
 * @brief Destroys the memory pool and frees all associated memory.
 * Implements `pool->free`.
 *
 * Frees all memory chunks allocated by the pool (`ctx->head` list) and all
 * externally traced memory blocks (`ctx->extra` list). Then frees the pool
 * context and the pool structure itself.
 *
 * @param pool Pointer to the `mongory_memory_pool` to destroy.
 */
static inline void mongory_memory_pool_destroy(mongory_memory_pool *pool) {
  if (!pool)
    return;
  mongory_memory_pool_ctx *ctx = (mongory_memory_pool_ctx *)pool->ctx;
  mongory_allocator alloc = {
    .alloc = mongory_default_alloc,
    .free = mongory_default_free,
    .user_data = NULL,
  };

  if (ctx) {
    alloc = ctx->allocator;
    // Free the main list of memory chunks.
    mongory_memory_pool_node_list_free(ctx->head, &alloc, &alloc);
    // Free the list of externally traced memory chunks (external pointers freed with stdlib free).
    mongory_memory_pool_node_list_free(ctx->extra, &alloc, NULL);
    mongory_mutex_destroy(&ctx->build_lock);

    memset(ctx, 0,
           sizeof(mongory_memory_pool_ctx)); // Clear the context structure.
    mongory_allocator_free(&alloc, ctx);     // Free the context structure.
  }

  memset(pool, 0, sizeof(mongory_memory_pool)); // Clear the pool structure.
  mongory_allocator_free(&alloc, pool);         // Free the pool structure.
}

/**
 * @brief Traces an externally allocated piece of memory. Implements
 * `pool->trace`.
 *
 * Creates a new `mongory_memory_node` to track the external memory block and
 * adds it to the `extra` list in the pool's context. This memory will be
 * freed when `mongory_memory_pool_destroy` is called if this trace function
 * is used.
 *
 * @param pool Pointer to the `mongory_memory_pool`.
 * @param ptr Pointer to the externally allocated memory.
 * @param size Size of the externally allocated memory.
 */
static inline void mongory_memory_pool_trace(mongory_memory_pool *pool, void *ptr, size_t size) {
  if (!pool || !pool->ctx || !ptr) {
    return;
  }
  mongory_memory_pool_ctx *pool_ctx = (mongory_memory_pool_ctx *)pool->ctx;

  // Create a new node to trace this external allocation.
  mongory_memory_node *extra_alloc_tracer =
      (mongory_memory_node *)mongory_allocator_zalloc(&pool_ctx->allocator, sizeof(mongory_memory_node));
  if (!extra_alloc_tracer) {
    pool->error = &MONGORY_ALLOC_ERROR;
    return;
  }
  extra_alloc_tracer->ptr = ptr;              // This is the externally allocated memory.
  extra_alloc_tracer->size = size;            // Its size.
  extra_alloc_tracer->used = size;            // Mark as fully "used" in context of tracing.
  extra_alloc_tracer->next = pool_ctx->extra; // Prepend to extra list.
  pool_ctx->extra = extra_alloc_tracer;
}

mongory_memory_pool *mongory_memory_pool_new_with_allocator(const mongory_allocator *allocator) {
  mongory_allocator active_alloc = {
    .alloc = mongory_default_alloc,
    .free = mongory_default_free,
    .user_data = NULL,
  };
  if (allocator != NULL) {
    if (!allocator->alloc || !allocator->free) {
      return NULL;
    }
    active_alloc = *allocator;
  }

  // Allocate the main pool structure.
  mongory_memory_pool *pool = (mongory_memory_pool *)mongory_allocator_zalloc(&active_alloc, sizeof(mongory_memory_pool));
  if (!pool) {
    return NULL;
  }

  // Allocate the internal context for the pool.
  mongory_memory_pool_ctx *ctx =
      (mongory_memory_pool_ctx *)mongory_allocator_zalloc(&active_alloc, sizeof(mongory_memory_pool_ctx));
  if (!ctx) {
    mongory_allocator_free(&active_alloc, pool); // Clean up partially allocated pool.
    return NULL;
  }

  // Allocate the first memory chunk.
  mongory_memory_node *first_chunk = mongory_memory_chunk_new(&active_alloc, MONGORY_INITIAL_CHUNK_SIZE);
  if (!first_chunk) {
    mongory_allocator_free(&active_alloc, ctx);  // Clean up context.
    mongory_allocator_free(&active_alloc, pool); // Clean up pool structure.
    return NULL;
  }

  // Initialize context fields.
  ctx->chunk_size = MONGORY_INITIAL_CHUNK_SIZE;
  ctx->max_chunk_size = MONGORY_DEFAULT_MAX_CHUNK_SIZE;
  ctx->max_total_bytes = 0;
  ctx->total_bytes = MONGORY_INITIAL_CHUNK_SIZE;
  ctx->allocator = active_alloc;
  ctx->head = first_chunk;
  ctx->current = first_chunk;
  ctx->extra = NULL; // No extra traced allocations initially.
  mongory_mutex_init(&ctx->build_lock);

  // Initialize pool fields.
  pool->ctx = ctx;
  pool->alloc = mongory_memory_pool_alloc;
  pool->reset = mongory_memory_pool_reset;
  pool->free = mongory_memory_pool_destroy;
  pool->trace = mongory_memory_pool_trace;
  pool->error = NULL; // No error initially.

  return pool;
}

/**
 * @brief Creates and initializes a new memory pool.
 *
 * Allocates the `mongory_memory_pool` structure, its internal
 * `mongory_memory_pool_ctx`, and the first memory chunk. Sets up the function
 * pointers for `alloc`, `free`, and `trace`.
 *
 * @return mongory_memory_pool* Pointer to the new pool, or NULL on failure.
 */
mongory_memory_pool *mongory_memory_pool_new() {
  return mongory_memory_pool_new_with_allocator(NULL);
}

void mongory_memory_pool_set_limits(mongory_memory_pool *pool, size_t max_chunk_size, size_t max_total_bytes) {
  if (!pool || !pool->ctx || pool->alloc != mongory_memory_pool_alloc) {
    return;
  }
  mongory_memory_pool_ctx *ctx = (mongory_memory_pool_ctx *)pool->ctx;
  ctx->max_chunk_size = max_chunk_size > 0 ? max_chunk_size : MONGORY_DEFAULT_MAX_CHUNK_SIZE;
  ctx->max_total_bytes = max_total_bytes;
}

size_t mongory_memory_pool_total_bytes(const mongory_memory_pool *pool) {
  if (!pool || !pool->ctx || pool->alloc != mongory_memory_pool_alloc) {
    return 0;
  }
  const mongory_memory_pool_ctx *ctx = (const mongory_memory_pool_ctx *)pool->ctx;
  return ctx->total_bytes;
}

void mongory_memory_pool_lock(mongory_memory_pool *pool) {
  if (!pool || !pool->ctx || pool->alloc != mongory_memory_pool_alloc) {
    return;
  }
  mongory_memory_pool_ctx *ctx = (mongory_memory_pool_ctx *)pool->ctx;
  mongory_mutex_lock(&ctx->build_lock);
}

void mongory_memory_pool_unlock(mongory_memory_pool *pool) {
  if (!pool || !pool->ctx || pool->alloc != mongory_memory_pool_alloc) {
    return;
  }
  mongory_memory_pool_ctx *ctx = (mongory_memory_pool_ctx *)pool->ctx;
  mongory_mutex_unlock(&ctx->build_lock);
}
