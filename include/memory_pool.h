#ifndef LOCK_FREE_MEMORY_POOL_H
#define LOCK_FREE_MEMORY_POOL_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEMORY_POOL_BLOCK_SIZE 4096
#define CACHE_LINE_SIZE 64

typedef struct Batch {
    struct Batch* next;  // Next batch in the list
    void* memory;        // Pointer to the allocated batch memory
} Batch;

typedef struct MemoryBlock {
    struct MemoryBlock* next;  // Next block in the pool
    atomic_size_t used;        // Used memory in the block
    char* data;                // Flexible array member
} MemoryBlock __attribute__((aligned(CACHE_LINE_SIZE)));

typedef struct MemoryPool {
    atomic_uintptr_t current_block;  // Atomic pointer to current global block
    size_t block_size;               // Size of each block
    atomic_uintptr_t free_list;      // Atomic pointer to the free list
    Batch* batches;                  // List of all allocated batches
} MemoryPool __attribute__((aligned(CACHE_LINE_SIZE)));

// Create a new memory pool
MemoryPool* memory_pool_create(size_t initial_size);

// Allocate memory from the pool
void* memory_pool_alloc(MemoryPool* pool, size_t size);

// Destroy the memory pool
void memory_pool_destroy(MemoryPool* pool);

#endif /* LOCK_FREE_MEMORY_POOL_H */
