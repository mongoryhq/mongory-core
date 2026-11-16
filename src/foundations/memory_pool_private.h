#ifndef MONGORY_FOUNDATIONS_MEMORY_POOL_PRIVATE_H
#define MONGORY_FOUNDATIONS_MEMORY_POOL_PRIVATE_H

/**
 * @file memory_pool_private.h
 * @brief Internal synchronization and atomic helpers for mongory-core.
 *
 * Keeps platform-specific primitives out of public headers while enabling
 * thread-safe double-checked lazy initialization on shared matcher pools.
 */

#include "mongory-core/foundations/memory_pool.h"

#if defined(_MSC_VER)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  define mongory_atomic_load_ptr_acquire(ptr) \
     InterlockedCompareExchangePointer((PVOID volatile *)(ptr), NULL, NULL)
#  define mongory_atomic_load_ptr_relaxed(ptr) (*(ptr))
#  define mongory_atomic_store_ptr_release(ptr, val) \
     InterlockedExchangePointer((PVOID volatile *)(ptr), (PVOID)(val))
#elif defined(__GNUC__) || defined(__clang__)
#  define mongory_atomic_load_ptr_acquire(ptr) __atomic_load_n((ptr), __ATOMIC_ACQUIRE)
#  define mongory_atomic_load_ptr_relaxed(ptr) __atomic_load_n((ptr), __ATOMIC_RELAXED)
#  define mongory_atomic_store_ptr_release(ptr, val) __atomic_store_n((ptr), (val), __ATOMIC_RELEASE)
#else
#  define mongory_atomic_load_ptr_acquire(ptr) (*(ptr))
#  define mongory_atomic_load_ptr_relaxed(ptr) (*(ptr))
#  define mongory_atomic_store_ptr_release(ptr, val) (*(ptr) = (val))
#endif

/**
 * @brief Acquires the pool's internal recursive build lock.
 * Safe no-op if `pool` is NULL or uses a custom allocator without a core ctx.
 */
void mongory_memory_pool_lock(mongory_memory_pool *pool);

/**
 * @brief Releases the pool's internal recursive build lock.
 */
void mongory_memory_pool_unlock(mongory_memory_pool *pool);

#endif /* MONGORY_FOUNDATIONS_MEMORY_POOL_PRIVATE_H */
