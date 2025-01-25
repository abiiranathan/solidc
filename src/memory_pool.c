
#include "../include/memory_pool.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MemoryBlock* volatile block;
    atomic_flag lock;
} ThreadLocalStorage __attribute__((aligned(CACHE_LINE_SIZE)));

static _Thread_local ThreadLocalStorage tls = {};

// Allocate a batch of memory blocks with a given block size.
// The blocks are linked together and the memory is aligned to the cache line size.
static MemoryBlock* allocate_block_batch(MemoryPool* pool, size_t block_size, size_t batch_size) {
    size_t total_size = sizeof(MemoryBlock) * batch_size + block_size * batch_size;
    total_size = (total_size + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);

    void* memory = aligned_alloc(CACHE_LINE_SIZE, total_size);
    if (!memory) {
        return nullptr;
    }

    // Create a new batch entry
    Batch* batch = malloc(sizeof(Batch));
    if (!batch) {
        free(memory);
        return nullptr;
    }

    batch->memory = memory;
    batch->next = pool->batches;
    pool->batches = batch;

    // Initialize blocks within the batch
    MemoryBlock* head = (MemoryBlock*)memory;
    char* data_area = (char*)memory + sizeof(MemoryBlock) * batch_size;

    for (size_t i = 0; i < batch_size; i++) {
        MemoryBlock* current = &head[i];
        current->data = data_area + i * block_size;
        current->used = 0;
        current->next = (i < batch_size - 1) ? &head[i + 1] : NULL;
    }

    return head;
}

MemoryPool* mpool_create(size_t block_size) {
    block_size = block_size ? block_size : MEMORY_POOL_BLOCK_SIZE;

    MemoryPool* pool = malloc(sizeof(MemoryPool));
    if (!pool) {
        return nullptr;
    }

    atomic_init(&pool->current_block, (uintptr_t)NULL);
    atomic_init(&pool->free_list, (uintptr_t)NULL);
    pool->block_size = block_size;
    pool->batches = nullptr;

    // Allocate the initial batch
    MemoryBlock* initial_block = allocate_block_batch(pool, block_size, 8);
    if (!initial_block) {
        free(pool);
        return nullptr;
    }

    atomic_store(&pool->current_block, (uintptr_t)initial_block);
    return pool;
}

void* mpool_alloc(MemoryPool* pool, size_t size) {
    size = (size + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);  // Align size

    // Try thread-local allocation first
    ThreadLocalStorage* tls_ptr = &tls;

    if (tls_ptr->block) {
        if (tls_ptr->block->used + size <= pool->block_size) {
            size_t offset = tls_ptr->block->used;
            tls_ptr->block->used += size;
            return tls_ptr->block->data + offset;
        }
    }

    // Prefetch the global free list to reduce cache misses
    uintptr_t free_list = atomic_load(&pool->free_list);

    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    __builtin_prefetch((void*)free_list);

    // Batch allocation from global free list
    MemoryBlock* batch_head = nullptr;
    do {
        if (!free_list) {
            batch_head = allocate_block_batch(pool, pool->block_size, 8);  // Fetch a batch
            if (!batch_head)
                return NULL;
            break;
        }

        // NOLINTNEXTLINE(performance-no-int-to-ptr): uintptr_t is used for atomic operations
        batch_head = (MemoryBlock*)free_list;
    } while (
        !atomic_compare_exchange_weak(&pool->free_list, &free_list, (uintptr_t)batch_head->next));

    // Use the first block from the batch
    MemoryBlock* new_block = batch_head;
    new_block->used = size;

    // Store the rest of the batch in thread-local storage
    tls_ptr->block = batch_head->next;
    return new_block->data;
}

void mpool_destroy(MemoryPool* pool) {
    // Free all batches
    Batch* batch = pool->batches;
    while (batch) {
        Batch* next = batch->next;
        free(batch->memory);
        free(batch);
        batch = next;
    }
    free(pool);
}
