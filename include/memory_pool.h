#ifndef LOCK_FREE_MEMORY_POOL_H
#define LOCK_FREE_MEMORY_POOL_H

#include <stddef.h>

// MemoryPool opaque struct.
typedef struct MemoryPool MemoryPool;

// Create a new memory pool with block_size.
// If block_size is 0, a default of MEMORY_POOL_BLOCK_SIZE is used.
MemoryPool* mpool_create(size_t block_size);

// Allocate memory from the pool
void* mpool_alloc(MemoryPool* pool, size_t size);

// Destroy the memory pool.
void mpool_destroy(MemoryPool* pool);

#endif /* LOCK_FREE_MEMORY_POOL_H */
