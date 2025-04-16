/**
 * @file memory_allocator.c
 * @brief Implementation of thread-safe lock-free memory allocator
 */

#include "my_malloc.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/********************* Configuration *********************/

#define INITIAL_POOL_SIZE (1024 * 1024)  // 1MB initial pool size
#define POOL_GROWTH_FACTOR 2             // How much to grow pool when full
#define MAX_POOLS 16                     // Maximum number of memory pools
#define ALIGNMENT 16                     // Alignment requirement (16 bytes)
#define MIN_BLOCK_SIZE 32                // Minimum block size including header

// Magic numbers for block validation
#define MAGIC_FREE 0xDEADBEEF
#define MAGIC_ALLOCATED 0xBEEFDEAD

/********************* Data Structures *********************/

// Memory pool structure
typedef struct memory_pool {
    uint8_t* memory;             // Actual memory area
    size_t size;                 // Size of this memory pool
    _Atomic(void*) first_block;  // First block in this pool (atomic pointer)
} memory_pool;

// Block header structure
typedef struct block_header {
    _Atomic(size_t) size;                // Size of the block (including header)
    _Atomic(struct block_header*) next;  // Next block pointer (atomic)
    _Atomic(bool) is_free;               // Is this block free? (atomic)
    uint32_t magic;                      // Magic number for validation
    uint32_t pool_index;                 // Which pool this block belongs to
    void* padding;                       // Ensure proper alignment
} block_header;

/********************* Global State *********************/

// Array of memory pools
static memory_pool g_pools[MAX_POOLS];
static _Atomic(size_t) g_pool_count = 0;

// Atomic pointer to free list head
static _Atomic(block_header*) g_free_list = NULL;

/********************* Utility Functions *********************/

/**
 * @brief Align a given size up to the nearest multiple of alignment
 */
static size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Get the aligned header size
 */
static size_t header_size(void) {
    return align_up(sizeof(block_header), ALIGNMENT);
}

/**
 * @brief Initialize a new memory pool
 */
static bool initialize_new_pool(size_t min_size) {
    size_t pool_index = atomic_load(&g_pool_count);

    // Check if we've reached maximum pools
    if (pool_index >= MAX_POOLS) {
        return false;
    }

    // Determine pool size (at least min_size or grow by factor)
    size_t pool_size = INITIAL_POOL_SIZE;
    if (pool_index > 0) {
        pool_size = g_pools[pool_index - 1].size * POOL_GROWTH_FACTOR;
    }

    // Ensure the pool is large enough for the requested allocation
    if (pool_size < min_size) {
        pool_size = align_up(min_size, ALIGNMENT);
    }

    // Allocate the memory
    uint8_t* memory = (uint8_t*)malloc(pool_size);
    if (!memory) {
        return false;
    }

    // Set up the pool
    g_pools[pool_index].memory = memory;
    g_pools[pool_index].size   = pool_size;

    // Initialize the pool with a single free block
    block_header* header = (block_header*)memory;
    header->size         = pool_size;
    header->next         = NULL;
    header->is_free      = true;
    header->magic        = MAGIC_FREE;
    header->pool_index   = (uint32_t)pool_index;

    // Atomically publish the first block
    atomic_store(&g_pools[pool_index].first_block, header);

    // Atomically add to free list
    block_header* expected = atomic_load(&g_free_list);
    do {
        header->next = expected;
    } while (!atomic_compare_exchange_weak(&g_free_list, &expected, header));

    // Increment pool count
    atomic_fetch_add(&g_pool_count, 1);

    return true;
}

/**
 * @brief Validate that a pointer belongs to our memory pools
 */
static bool is_valid_ptr(void* ptr) {
    if (ptr == NULL) {
        return false;
    }

    // Check if ptr is within any of our pools
    for (size_t i = 0; i < atomic_load(&g_pool_count); i++) {
        uint8_t* pool_start = g_pools[i].memory;
        uint8_t* pool_end   = pool_start + g_pools[i].size;

        if ((uint8_t*)ptr >= pool_start && (uint8_t*)ptr < pool_end) {
            // Valid pointer must be after a header
            if ((uint8_t*)ptr - pool_start >= (uint8_t)header_size()) {
                // Check the magic number in the header
                block_header* header = (block_header*)((uint8_t*)ptr - header_size());
                return header->magic == MAGIC_ALLOCATED;
            }
        }
    }

    return false;
}

/**
 * @brief Split a block into an allocated block and a free remainder
 */
static void split_block(block_header* header, size_t size) {
    size_t original_size = atomic_load(&header->size);

    // Check if the block is large enough to be split
    if (original_size >= size + MIN_BLOCK_SIZE) {
        // Create a new block in the remaining space
        block_header* new_block = (block_header*)((uint8_t*)header + size);
        new_block->size         = original_size - size;
        new_block->is_free      = true;
        new_block->magic        = MAGIC_FREE;
        new_block->pool_index   = header->pool_index;

        // Atomically update the size of the original block
        atomic_store(&header->size, size);

        // Add the new block to the free list
        block_header* expected = atomic_load(&g_free_list);
        do {
            atomic_store(&new_block->next, expected);
        } while (!atomic_compare_exchange_weak(&g_free_list, &expected, new_block));
    }
}

/**
 * @brief Try to find and allocate from an existing free block
 */
static void* allocate_from_free_list(size_t total_size) {
    block_header* prev    = NULL;
    block_header* current = atomic_load(&g_free_list);

    while (current != NULL) {
        block_header* next = atomic_load(&current->next);

        // Check if this block is free and large enough
        if (atomic_load(&current->is_free) && atomic_load(&current->size) >= total_size) {
            // Remove from free list
            if (prev) {
                atomic_store(&prev->next, next);
            } else {
                // Attempting to update head of free list
                if (!atomic_compare_exchange_strong(&g_free_list, &current, next)) {
                    // List changed, restart search
                    return allocate_from_free_list(total_size);
                }
            }

            // Split the block if needed
            split_block(current, total_size);

            // Mark as allocated
            atomic_store(&current->is_free, false);
            current->magic = MAGIC_ALLOCATED;

            // Return pointer to the payload
            return (void*)((uint8_t*)current + header_size());
        }

        prev    = current;
        current = next;
    }

    return NULL;  // No suitable block found
}

/**
 * @brief Ensure memory pools are initialized
 */
static void ensure_initialized(void) {
    if (atomic_load(&g_pool_count) == 0) {
        initialize_new_pool(INITIAL_POOL_SIZE);
    }
}

/********************* API Implementation *********************/

void* my_malloc(size_t size) {
    // Handle zero-size allocation
    if (size == 0) {
        return NULL;
    }

    // Ensure we have at least one pool
    ensure_initialized();

    // Calculate total size needed (header + aligned payload)
    size_t aligned_size = align_up(size, ALIGNMENT);
    size_t total_size   = header_size() + aligned_size;

    // Try to allocate from existing free blocks
    void* result = allocate_from_free_list(total_size);
    if (result) {
        return result;
    }

    // Need to create a new pool
    if (initialize_new_pool(total_size)) {
        // Try again with the new pool
        return allocate_from_free_list(total_size);
    }

    // Allocation failed
    return NULL;
}

void my_free(void* ptr) {
    if (!is_valid_ptr(ptr)) {
        fprintf(stderr, "my_free: invalid pointer %p\n", ptr);
        return;
    }

    // Get the block header
    block_header* header = (block_header*)((uint8_t*)ptr - header_size());

    // Mark as free
    bool was_allocated = !atomic_exchange(&header->is_free, true);
    if (!was_allocated) {
        fprintf(stderr, "my_free: double free detected on pointer %p\n", ptr);
        return;
    }

    header->magic = MAGIC_FREE;

    // Clear the memory (optional, for security)
    size_t payload_size = atomic_load(&header->size) - header_size();
    memset(ptr, 0, payload_size);

    // Add to free list
    block_header* expected = atomic_load(&g_free_list);
    do {
        atomic_store(&header->next, expected);
    } while (!atomic_compare_exchange_weak(&g_free_list, &expected, header));

    // Note: We don't coalesce blocks here as it would require locking
    // Instead, we could run a periodic coalescing pass or implement
    // a more sophisticated lock-free coalescing algorithm
}

void* my_calloc(size_t nmemb, size_t size) {
    // Check for overflow
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        return NULL;
    }

    size_t total = nmemb * size;
    void* ptr    = my_malloc(total);

    if (ptr) {
        memset(ptr, 0, total);
    }

    return ptr;
}

void* my_realloc(void* ptr, size_t size) {
    // If ptr is NULL, behave like malloc
    if (ptr == NULL) {
        return my_malloc(size);
    }

    // If size is 0, behave like free
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    // Check if ptr is valid
    if (!is_valid_ptr(ptr)) {
        fprintf(stderr, "my_realloc: invalid pointer %p\n", ptr);
        return NULL;
    }

    // Get the block header
    block_header* header = (block_header*)((uint8_t*)ptr - header_size());

    // Calculate new sizes
    size_t aligned_size         = align_up(size, ALIGNMENT);
    size_t new_total_size       = header_size() + aligned_size;
    size_t current_size         = atomic_load(&header->size);
    size_t current_payload_size = current_size - header_size();

    // If current block is large enough, we can use it
    if (current_size >= new_total_size) {
        // Optionally split the block if there's enough extra space
        split_block(header, new_total_size);
        return ptr;
    }

    // Need to allocate a new block
    void* new_ptr = my_malloc(size);
    if (new_ptr) {
        // Copy the old data
        size_t copy_size = (current_payload_size < size) ? current_payload_size : size;
        memcpy(new_ptr, ptr, copy_size);
        my_free(ptr);
    }

    return new_ptr;
}

void print_memory_state(void) {
    printf("=== Memory Allocator State ===\n");

    // Print pool information
    size_t pool_count = atomic_load(&g_pool_count);
    printf("Memory Pools: %zu\n", pool_count);

    for (size_t i = 0; i < pool_count; i++) {
        printf("Pool %zu: %p, Size: %zu bytes\n", i, (void*)g_pools[i].memory, g_pools[i].size);

        // Print blocks in this pool
        block_header* current = (block_header*)g_pools[i].memory;
        while (current) {
            printf("  Block @ %p: size = %zu, %s, magic = 0x%x\n",
                   (void*)current,
                   atomic_load(&current->size),
                   atomic_load(&current->is_free) ? "free" : "allocated",
                   current->magic);

            // Calculate next physical block
            uint8_t* next_addr = (uint8_t*)current + atomic_load(&current->size);
            if (next_addr >= g_pools[i].memory + g_pools[i].size) {
                break;  // End of pool
            }
            current = (block_header*)next_addr;
        }
    }

    // Print free list
    printf("\nFree List:\n");
    block_header* free_block = atomic_load(&g_free_list);
    while (free_block) {
        printf("  Free Block @ %p: size = %zu\n", (void*)free_block, atomic_load(&free_block->size));
        free_block = atomic_load(&free_block->next);
    }

    printf("===========================\n\n");
}
