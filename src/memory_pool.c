#include "../include/memory_pool.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>

#define CACHE_LINE_SIZE 64
#define CHUNK_MIN_SIZE 4096

// Struct to represent a memory chunk
typedef struct MemoryChunk {
    void* memory;              // Pointer to the start of the chunk
    void* free_ptr;            // Pointer to the next free block
    size_t capacity;           // Total size of the chunk
    size_t used;               // Amount of memory used in the chunk
    struct MemoryChunk* next;  // Pointer to the next chunk
} MemoryChunk __attribute__((aligned(CACHE_LINE_SIZE)));

// Struct to represent the memory pool
typedef struct MemoryPool {
    MemoryChunk* head;  // Pointer to the first chunk
    MemoryChunk* mru;   // Pointer to the most recently used chunk
    size_t chunk_size;  // Default size of new chunks
} MemoryPool __attribute__((aligned(CACHE_LINE_SIZE)));

// Align a size to the next multiple of CACHE_LINE_SIZE
static inline size_t align_size(size_t size) {
    return (size + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
}

// Create a new memory chunk
static inline MemoryChunk* chunk_create(size_t size) {
    size = align_size(size);  // Align the size
    MemoryChunk* chunk = malloc(sizeof(MemoryChunk));
    if (!chunk) {
        perror("Failed to allocate memory for chunk");
        return NULL;
    }

    chunk->memory = aligned_alloc(CACHE_LINE_SIZE, size);
    if (!chunk->memory) {
        perror("Failed to allocate aligned memory for chunk");
        free(chunk);
        return NULL;
    }

    chunk->free_ptr = chunk->memory;
    chunk->capacity = size;
    chunk->used = 0;
    chunk->next = NULL;

    return chunk;
}

// Destroy a memory chunk
static inline void chunk_destroy(MemoryChunk* chunk) {
    if (chunk) {
        free(chunk->memory);
        free(chunk);
    }
}

// Create a memory pool
MemoryPool* mpool_create(size_t chunk_size) {
    if (chunk_size < CHUNK_MIN_SIZE) {
        chunk_size = CHUNK_MIN_SIZE;  // Ensure a reasonable minimum chunk size
    }

    MemoryPool* pool = malloc(sizeof(MemoryPool));
    if (!pool) {
        perror("Failed to allocate memory for pool");
        return NULL;
    }

    pool->head = chunk_create(chunk_size);
    if (!pool->head) {
        free(pool);
        return NULL;
    }

    pool->mru = pool->head;
    pool->chunk_size = chunk_size;

    return pool;
}

// Allocate memory from the pool
void* mpool_alloc(MemoryPool* pool, size_t size) {
    if (!pool || size == 0) {
        return NULL;
    }

    size = align_size(size);

    // First, try allocating from the most recently used chunk
    MemoryChunk* chunk = pool->mru;
    if (chunk->used + size <= chunk->capacity) {
        void* block = chunk->free_ptr;
        chunk->free_ptr = (char*)chunk->free_ptr + size;
        chunk->used += size;
        return block;
    }

    // If MRU chunk doesn't have enough space, search other chunks
    chunk = pool->head;
    while (chunk) {
        if (chunk->used + size <= chunk->capacity) {
            pool->mru = chunk;  // Update MRU to this chunk
            void* block = chunk->free_ptr;
            chunk->free_ptr = (char*)chunk->free_ptr + size;
            chunk->used += size;
            return block;
        }
        chunk = chunk->next;
    }

    // If no chunk has enough space, create a new chunk
    size_t new_chunk_size = pool->chunk_size > size ? pool->chunk_size : size;
    MemoryChunk* new_chunk = chunk_create(new_chunk_size);
    if (!new_chunk) {
        return NULL;  // Allocation failed
    }

    // Add the new chunk to the pool
    new_chunk->next = pool->head;
    pool->head = new_chunk;
    pool->mru = new_chunk;

    // Allocate from the new chunk
    void* block = new_chunk->free_ptr;
    new_chunk->free_ptr = (char*)new_chunk->free_ptr + size;
    new_chunk->used += size;

    return block;
}

void mpool_reset(MemoryPool* pool) {
    if (!pool) {
        return;
    }

    // Reset all chunks to their initial state
    MemoryChunk* chunk = pool->head;
    while (chunk) {
        chunk->free_ptr = chunk->memory;
        chunk->used = 0;
        chunk = chunk->next;
    }

    // Reset most recently used chunk to the head
    pool->mru = pool->head;
}

// Destroy the memory pool
void mpool_destroy(MemoryPool* pool) {
    if (!pool)
        return;

    MemoryChunk* chunk = pool->head;
    while (chunk) {
        MemoryChunk* next = chunk->next;
        chunk_destroy(chunk);
        chunk = next;
    }

    free(pool);
}

char* mpool_copy_str(MemoryPool* pool, const char* src) {
    size_t len = strlen(src);
    char* dest = mpool_alloc(pool, len + 1);
    if (!dest)
        return NULL;

    dest[len] = '\0';
    strcpy(dest, src);
    return dest;
}
