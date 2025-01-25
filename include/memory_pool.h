#ifndef LOCK_FREE_MEMORY_POOL_H
#define LOCK_FREE_MEMORY_POOL_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEMORY_POOL_BLOCK_SIZE 4096
#define CACHE_LINE_SIZE 64

typedef struct Batch {
    struct Batch* next;  // Pointer to the next batch
    void* memory;        // Allocated memory for this batch
    char padding[CACHE_LINE_SIZE - sizeof(struct Batch*) - sizeof(void*)];
} Batch __attribute__((aligned(CACHE_LINE_SIZE)));

typedef struct MemoryBlock {
    struct MemoryBlock* next;  // Pointer to the next block
    char* data;                // Data pointer (should be closer to used)
    atomic_size_t used;        // Used memory (frequently updated, align with data)
    char padding[CACHE_LINE_SIZE - sizeof(struct MemoryBlock*) - sizeof(char*) -
                 sizeof(atomic_size_t)];
} MemoryBlock __attribute__((aligned(CACHE_LINE_SIZE)));

typedef struct MemoryPool {
    atomic_uintptr_t current_block;  // Atomic pointer to the current block
    atomic_uintptr_t free_list;      // Atomic pointer to the free list
    size_t block_size;               // Size of each block (read-only, infrequent)
    Batch* batches;                  // List of all allocated batches
    char padding[CACHE_LINE_SIZE - sizeof(atomic_uintptr_t) * 2 - sizeof(size_t) - sizeof(Batch*)];
} MemoryPool __attribute__((aligned(CACHE_LINE_SIZE)));

// Create a new memory pool with block_size.
// If block_size is 0, a default of MEMORY_POOL_BLOCK_SIZE is used.
MemoryPool* mpool_create(size_t block_size);

// Allocate memory from the pool
void* mpool_alloc(MemoryPool* pool, size_t size);

// Destroy the memory pool.
void mpool_destroy(MemoryPool* pool);

#endif /* LOCK_FREE_MEMORY_POOL_H */
