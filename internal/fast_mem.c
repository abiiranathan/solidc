#include "../include/fast_mem.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MEMORY_SIZE
// Total memory size in bytes.
#define MEMORY_SIZE 1 * 1024 * 1024
#endif

// Use platform-recommended alignment (usually pointer size)
#define ALIGNMENT sizeof(void*)

// Magic numbers for block validation
#define MAGIC_FREE 0xDEADBEEF
#define MAGIC_ALLOCATED 0xBEEFDEAD

// Global memory pool
static uint8_t memory[MEMORY_SIZE];

// Block header structure (now doubly-linked)
typedef struct block_header {
    size_t size;                // Total size of the block (including header)
    struct block_header* prev;  // Pointer to the previous block in memory layout
    struct block_header* next;  // Pointer to the next block in memory layout
    // is_free removed - we can infer from magic or potentially keep it
    // bool is_free;
    uint32_t magic;  // Magic value for error detection/status
} block_header;

// Align a given size up to the nearest multiple of alignment
static size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Calculate aligned header size
#define HEADER_SIZE (align_up(sizeof(block_header), ALIGNMENT))

// --- Helper Functions ---

// Get payload pointer from header
static inline void* header_to_payload(block_header* header) {
    return (void*)((uint8_t*)header + HEADER_SIZE);
}

// Get header pointer from payload
static inline block_header* payload_to_header(void* ptr) {
    return (block_header*)((uint8_t*)ptr - HEADER_SIZE);
}

// Check if a block is free based on magic number
static inline bool is_block_free(block_header* header) {
    // Check pointer validity before dereferencing if necessary,
    // but usually called on known valid headers within list.
    return header && header->magic == MAGIC_FREE;
}

// Initialize the memory pool with a single free block
__attribute__((constructor)) static void initialize_memory() {
    // Ensure memory size is at least enough for one header
    if (MEMORY_SIZE < HEADER_SIZE) {
        fprintf(stderr, "Error: MEMORY_SIZE is too small.\n");
        exit(EXIT_FAILURE);
    }
    block_header* header = (block_header*)memory;
    header->size         = MEMORY_SIZE;
    header->prev         = NULL;  // First block
    header->next         = NULL;  // Only block initially
    header->magic        = MAGIC_FREE;
}

// Split a block if the remainder is large enough for a new block.
// 'header' points to the block to split.
// 'required_size' is the total size needed for the allocation (header + payload).
static void split_block_if_possible(block_header* header, size_t required_size) {
    // Calculate remaining size
    size_t remaining_size = header->size - required_size;

    // Check if the remaining space is large enough for a new block
    // (header + minimum payload alignment).
    if (remaining_size >= HEADER_SIZE + ALIGNMENT) {
        // Create the new free block header at the end of the allocated portion
        block_header* new_free_block = (block_header*)((uint8_t*)header + required_size);
        new_free_block->size         = remaining_size;
        new_free_block->magic        = MAGIC_FREE;    // Mark as free
        new_free_block->prev         = header;        // Link back to the allocated block
        new_free_block->next         = header->next;  // Link forward to the original next block

        // Update the next block's prev pointer if it exists
        if (header->next) {
            header->next->prev = new_free_block;
        }

        // Update the original header (now the allocated block)
        header->size = required_size;
        header->next = new_free_block;  // Point to the new free block
                                        // header->magic is set by the caller (FMALLOC)
    }
    // Else: Not enough space to split, allocate the whole block.
    // The original header's size remains unchanged, encompassing the "wasted" space.
}

// Coalesce a free block 'header' with its next neighbor if it's also free.
// Returns true if coalescing occurred.
static bool coalesce_with_next(block_header* header) {
    if (!header || !header->next || !is_block_free(header) || !is_block_free(header->next)) {
        return false;
    }

    block_header* next_block = header->next;
    // printf("Coalescing %p (size %zu) with next %p (size %zu)\n", (void*)header, header->size,
    // (void*)next_block, next_block->size);

    header->size += next_block->size;  // Combine sizes
    header->next = next_block->next;   // Link past the merged block

    // Update the block after the merged one, if it exists
    if (header->next) {
        header->next->prev = header;
    }

    // Optional: Clear merged block's header for debugging? Not strictly needed.
    // memset(next_block, 0xAA, sizeof(block_header));

    return true;
}

// Validate that a pointer seems to be within our managed memory payload area.
// This is a basic check. More robust checks could involve iterating the list,
// but that defeats the purpose of speed.
static bool is_valid_payload_ptr(void* ptr) {
    if (ptr == NULL)
        return false;

    uintptr_t ptr_addr      = (uintptr_t)ptr;
    uintptr_t mem_start     = (uintptr_t)memory;
    uintptr_t payload_start = mem_start + HEADER_SIZE;
    uintptr_t mem_end       = mem_start + MEMORY_SIZE;

    // Check if ptr is within the possible payload range
    if (ptr_addr < payload_start || ptr_addr >= mem_end) {
        return false;
    }

    // Check alignment (payload start should be aligned)
    if ((ptr_addr - mem_start - HEADER_SIZE) % ALIGNMENT != 0) {
        // This might indicate pointer arithmetic error or corruption
        // Or points inside a payload, not at its start.
        return false;  // Or issue a warning
    }

    // More robust check (optional, slower): Check header magic
    // block_header* header = payload_to_header(ptr);
    // return header->magic == MAGIC_ALLOCATED; // Check if actually allocated

    return true;  // Passes basic range and alignment checks
}

// --- Public API ---

void* FMALLOC(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // Align the requested payload size.
    size_t aligned_payload_size = align_up(size, ALIGNMENT);
    // Total size needed = header size + aligned payload size.
    size_t total_required_size = HEADER_SIZE + aligned_payload_size;

    // Ensure total size doesn't overflow and is reasonable
    if (total_required_size < HEADER_SIZE + ALIGNMENT || total_required_size > MEMORY_SIZE) {
        // Overflow or request larger than total memory
        return NULL;
    }

    // --- First-Fit Search Strategy ---
    block_header* current = (block_header*)memory;
    while (current) {
        // Check if block is free and large enough
        if (is_block_free(current) && current->size >= total_required_size) {
            // Found a suitable block.
            // Split if necessary and possible.
            split_block_if_possible(current, total_required_size);

            // Mark the block as allocated
            current->magic = MAGIC_ALLOCATED;

            // Return pointer to the payload area.
            return header_to_payload(current);
        }
        // Move to the next block in the list
        current = current->next;
    }

    // No suitable free block found.
    // Potential place to trigger a full memory compaction/coalesce if desired,
    // but we aim for fast FFREE.
    return NULL;
}

void FFREE(void* ptr) {
    // 1. Basic Pointer Validation
    if (!is_valid_payload_ptr(ptr)) {
        // Optional: Add logging here for invalid frees
        // fprintf(stderr, "Warning: Attempt to FFREE potentially invalid pointer %p\n", ptr);
        return;
    }

    // 2. Get Header
    block_header* header = payload_to_header(ptr);

    // 3. Check for Double Free / Corruption
    // If it's already free, or doesn't have the allocated magic, bail.
    if (header->magic != MAGIC_ALLOCATED) {
        // Optional: Add logging for double free or corruption
        // fprintf(stderr, "Warning: Double free or corruption detected for pointer %p (header %p, magic
        // 0x%x)\n", ptr, (void*)header, header->magic);
        return;
    }

    // 4. Mark as Free
    header->magic = MAGIC_FREE;

    // --- Immediate Coalescing ---
    // We perform coalescing in a specific order to simplify pointer updates.
    // Order:
    //  a. Coalesce forward (merge with NEXT block if free).
    //  b. Coalesce backward (merge with PREV block if free).

    // a. Try to coalesce with the NEXT block
    coalesce_with_next(header);  // Merges header->next into header if both are free

    // b. Try to coalesce with the PREVIOUS block
    // Note: 'header' might now be larger if forward coalesce happened.
    // We access 'header->prev' which points to the physically preceding block.
    if (header->prev && is_block_free(header->prev)) {
        // The previous block is free, merge 'header' into 'header->prev'.
        // The 'coalesce_with_next' function can be reused by calling it on the previous block.
        coalesce_with_next(header->prev);
    }

    // Coalescing is now O(1) - only checks immediate neighbors.
}

void* FCALLOC(size_t nmemb, size_t size) {
    // Check for overflow before multiplication
    size_t total_size;
    if (nmemb == 0 || size == 0) {
        total_size = 0;  // Standard behavior: allocate 0 bytes or return NULL later
    } else if (__builtin_mul_overflow(nmemb, size, &total_size)) {
        // Overflow occurred
        return NULL;
    }

    // If total_size is 0 after check, malloc(0) behavior is implementation-defined.
    // Some return NULL, some return a unique pointer. We'll aim for NULL if size is 0.
    // If nmemb or size was 0, total_size is 0.
    if (total_size == 0) {
        // Consistent approach: Call FMALLOC(0) which should return NULL per our logic.
        return FMALLOC(0);
        // Or explicitly return NULL here if desired.
        // return NULL;
    }

    void* ptr = FMALLOC(total_size);
    if (ptr) {
        // Memory allocated successfully, zero it out.
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void* FREALLOC(void* ptr, size_t size) {
    // Standard realloc behavior:
    // realloc(NULL, size) is equivalent to malloc(size)
    if (ptr == NULL) {
        return FMALLOC(size);
    }

    // realloc(ptr, 0) is equivalent to free(ptr) and returns NULL
    if (size == 0) {
        FFREE(ptr);
        return NULL;
    }

    // --- Get header and current size ---
    // Basic validation first (can enhance later if needed)
    if (!is_valid_payload_ptr(ptr))
        return NULL;  // Invalid ptr for realloc
    block_header* header = payload_to_header(ptr);
    if (header->magic != MAGIC_ALLOCATED)
        return NULL;  // Realloc on non-allocated block

    size_t current_payload_size     = header->size - HEADER_SIZE;
    size_t aligned_new_payload_size = align_up(size, ALIGNMENT);
    size_t total_required_size      = HEADER_SIZE + aligned_new_payload_size;

    // --- Optimization 1: Shrinking in place ---
    if (total_required_size <= header->size) {
        // The new size fits within the current block.
        // We can potentially split the remaining space into a new free block.
        split_block_if_possible(header, total_required_size);
        // We don't need to move data when shrinking.
        return ptr;  // Return the original pointer
    }

    // --- Optimization 2: Expanding in place (if next block is free) ---
    // Check if the next block exists, is free, and provides enough combined space.
    if (header->next && is_block_free(header->next) &&
        (header->size + header->next->size >= total_required_size)) {
        // Yes, we can expand by consuming the next block.
        block_header* next_block = header->next;

        // Merge the next block into the current one *before* splitting
        header->size += next_block->size;  // Absorb size
        header->next = next_block->next;   // Link past the absorbed block
        if (header->next) {
            header->next->prev = header;  // Update next block's prev pointer
        }
        // Now header has the combined size. Split if necessary.
        split_block_if_possible(header, total_required_size);
        // Header magic remains MAGIC_ALLOCATED. Data is already in place.
        return ptr;  // Return the original pointer
    }

    // --- Fallback: Allocate new block, copy data, free old block ---
    void* new_ptr = FMALLOC(size);  // Request original size, FMALLOC handles alignment
    if (new_ptr == NULL) {
        return NULL;  // Allocation failed
    }

    // Copy data from the old block to the new block.
    // Copy the minimum of the old payload size and the requested new size.
    size_t copy_size = (current_payload_size < size) ? current_payload_size : size;
    memcpy(new_ptr, ptr, copy_size);

    // Free the original block.
    FFREE(ptr);

    return new_ptr;
}

// Print the state of the memory blocks (updated for doubly-linked list).
void FDEBUG_MEMORY() {
    block_header* current = (block_header*)memory;
    printf("Memory state (Total Size: %d, Header Size: %zu):\n", MEMORY_SIZE, HEADER_SIZE);
    int i = 0;
    while (current) {
        printf(" [%d] Block @ %p: size = %-6zu, magic = 0x%x (%s), prev = %-10p, next = %p\n", i++,
               (void*)current, current->size, current->magic,
               (current->magic == MAGIC_ALLOCATED ? "ALLOC"
                                                  : (current->magic == MAGIC_FREE ? "FREE " : "?????")),
               (void*)current->prev, (void*)current->next);

        // Sanity checks (optional)
        if (current->next && (uintptr_t)current->next != (uintptr_t)current + current->size) {
            printf("     ERROR: current + size != next pointer! (%p != %p)\n",
                   (void*)((uintptr_t)current + current->size), (void*)current->next);
        }
        if (current->next && current->next->prev != current) {
            printf("     ERROR: next->prev != current pointer! (%p != %p)\n", (void*)current->next->prev,
                   (void*)current);
        }
        if (current->prev && current->prev->next != current) {
            printf("     ERROR: prev->next != current pointer! (%p != %p)\n", (void*)current->prev->next,
                   (void*)current);
        }

        current = current->next;
    }
    printf("---- End of Memory State ----\n\n");
}
