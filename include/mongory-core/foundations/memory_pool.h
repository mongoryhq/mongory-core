#ifndef MONGORY_MEMORY_POOL
#define MONGORY_MEMORY_POOL

/**
 * @file memory_pool.h
 * @brief Defines the mongory_memory_pool structure and its associated
 * operations.
 *
 * A memory pool is used for efficient memory management within the Mongory
 * library. It allows for allocating blocks of memory that can be freed
 * all at once when the pool itself is destroyed. This can reduce the overhead
 * of individual allocations and deallocations and help prevent memory leaks.
 */

#include "mongory-core/foundations/error.h"
#include <stdbool.h>
#include <stdlib.h> // For size_t

// Forward declaration of the memory pool structure.
typedef struct mongory_memory_pool mongory_memory_pool;

/** @name Memory Allocation Macros
 *  @brief Convenience macros for simplified memory allocation from a pool.
 *  @{
 */
/** @def MG_ALLOC(p, n)
 *  @brief Allocates a raw block of memory of size `n` from pool `p`.
 *  @param p Pointer to the `mongory_memory_pool`.
 *  @param n The number of bytes to allocate.
 *  @return A `void*` pointer to the allocated memory.
 */
#define MG_ALLOC(p, n) (p->alloc(p, n))

/** @def MG_ALLOC_PTR(p, t)
 *  @brief Allocates memory for a single instance of type `t` from pool `p` and returns a pointer of type `t*`.
 *  @param p Pointer to the `mongory_memory_pool`.
 *  @param t The type of the object to allocate (e.g., `my_struct`).
 *  @return A pointer of type `t*` to the allocated memory.
 */
#define MG_ALLOC_PTR(p, t) ((t *)MG_ALLOC(p, sizeof(t)))

/** @def MG_ALLOC_OBJ(p, t)
 *  @brief Deprecated. Use `MG_ALLOC_PTR` instead. Allocates memory for a single instance of type `t`.
 *  @param p Pointer to the `mongory_memory_pool`.
 *  @param t The type of the object to allocate.
 *  @return A pointer of type `t` (often used for pointer types) to the allocated memory.
 */
#define MG_ALLOC_OBJ(p, t) ((t)MG_ALLOC(p, sizeof(t)))

/** @def MG_ALLOC_ARY(p, t, n)
 *  @brief Allocates memory for an array of `n` elements of type `t` from pool `p`.
 *  @param p Pointer to the `mongory_memory_pool`.
 *  @param t The type of the array elements.
 *  @param n The number of elements in the array.
 *  @return A pointer of type `t*` to the beginning of the allocated array memory.
 */
#define MG_ALLOC_ARY(p, t, n) ((t *)MG_ALLOC(p, sizeof(t) * (n)))
/** @} */

/**
 * @struct mongory_memory_pool
 * @brief Represents a memory pool for managing memory allocations.
 *
 * The pool uses a context (`ctx`) to store its internal state. The default
 * implementation uses a linked list of memory chunks. The struct is designed
 * as an interface, allowing users to provide their own memory management
 * strategy by populating the function pointers (`alloc`, `free`, etc.) with
 * custom logic.
 *
 * @section custom_pool_example Custom Memory Pool Example
 * For memory-sensitive environments, you can implement your own pool. For
 * example, a simple bump allocator using a static buffer:
 *
 * @code
 * #include <string.h> // for memcpy
 *
 * // Custom context for the bump allocator
 * typedef struct {
 *     char* buffer;
 *     size_t capacity;
 *     size_t offset;
 * } bump_alloc_ctx;
 *
 * // Custom allocation function
 * void* bump_alloc(mongory_memory_pool* pool, size_t size) {
 *     bump_alloc_ctx* ctx = (bump_alloc_ctx*)pool->ctx;
 *     if (ctx->offset + size > ctx->capacity) {
 *         return NULL; // Out of memory
 *     }
 *     void* ptr = ctx->buffer + ctx->offset;
 *     ctx->offset += size;
 *     return ptr;
 * }
 *
 * // Custom free function (for a bump allocator, this might do nothing or reset)
 * void bump_free(mongory_memory_pool* pool) {
 *     // In this simple case, we free the entire pool, including the context
 *     // and the buffer itself, which were allocated externally.
 *     free(pool->ctx);
 *     free(pool);
 * }
 *
 * // Create and initialize the custom pool
 * char my_static_buffer[1024];
 * bump_alloc_ctx my_ctx = { .buffer = my_static_buffer, .capacity = 1024, .offset = 0 };
 *
 * mongory_memory_pool my_pool = {
 *     .alloc = bump_alloc,
 *     .free = bump_free,
 *     .trace = NULL, // Not implemented for this simple example
 *     .reset = NULL, // Not implemented
 *     .ctx = &my_ctx,
 *     .error = NULL
 * };
 *
 * // Now, `&my_pool` can be passed to any mongory function.
 * mongory_value *v = mongory_value_wrap_i(&my_pool, 123);
 * @endcode
 */
struct mongory_memory_pool {
  /**
   * @brief Allocates a block of memory from the pool.
   * @param pool A pointer to the pool itself.
   * @param size The number of bytes to allocate.
   * @return void* A pointer to the allocated memory block, or NULL on failure.
   * Memory allocated this way should be suitably aligned for any fundamental
   * type.
   */
  void *(*alloc)(mongory_memory_pool *pool, size_t size);

  /**
   * @brief Associates an externally allocated memory block with the pool.
   *
   * This function is a hint to the memory pool that a piece of memory,
   * allocated by external code (e.g., a third-party library), should be
   * considered part of the pool's managed set. A sophisticated pool
   * implementation could use this for garbage collection or tracking. The
   * default pool implementation does not use this.
   *
   * @param pool A pointer to the pool itself.
   * @param ptr A pointer to the memory block to trace.
   * @param size The size of the memory block.
   */
  void (*trace)(mongory_memory_pool *pool, void *ptr, size_t size);

  /**
   * @brief Resets the memory pool to its initial state, freeing all allocated
   * memory but keeping the pool itself alive.
   *
   * For the default implementation, this frees all memory chunks but allows
   * the pool to be used for new allocations. For a custom bump allocator,
   * this might simply reset the offset to zero.
   *
   * @param pool A pointer to the pool itself.
   */
  void (*reset)(mongory_memory_pool *pool);

  /**
   * @brief Frees the entire memory pool, including all memory blocks allocated
   * from it and any traced memory blocks (depending on implementation).
   * @param self A pointer to the mongory_memory_pool instance to be freed.
   */
  void (*free)(mongory_memory_pool *self);

  void *ctx;            /**< Pointer to the internal context/state of the memory
                           pool. This is managed by the pool implementation. */
  mongory_error *error; /**< Pointer to a mongory_error structure. If an
                           operation fails (e.g., allocation), this may be set
                           to describe the error. The memory for this error
                           struct itself is typically allocated from the pool or
                           is a static error object. */
};

/**
 * @brief Creates a new mongory_memory_pool instance.
 *
 * Initializes the pool structure and its internal context, preparing it for
 * allocations.
 *
 * @return mongory_memory_pool* A pointer to the newly created memory pool, or
 * NULL if creation fails (e.g., initial memory allocation for the pool's
 * context failed).
 */
mongory_memory_pool *mongory_memory_pool_new();

#endif /* MONGORY_MEMORY_POOL */
