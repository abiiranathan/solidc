#include "../include/fast_mem.h"

#include <assert.h>
#include <limits.h>  // For CHAR_BIT
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MEMORY_SIZE
// Total memory size in bytes.
#define MEMORY_SIZE 1 * 1024 * 1024
#endif

// Use platform-recommended alignment (usually pointer size)
#define ALIGNMENT 8

// Magic numbers for block validation
#define MAGIC_FREE 0xDEADBEEF
#define MAGIC_ALLOCATED 0xBEEFDEAD

// --- Bitmap Configuration ---
#define CHUNK_SHIFT 12  // Log2(CHUNK_SIZE), e.g., 12 for 4KB chunks
#define CHUNK_SIZE (1UL << CHUNK_SHIFT)
// Ensure CHUNK_SIZE is at least large enough for a header + alignment
#if CHUNK_SIZE < (HEADER_SIZE + ALIGNMENT)
#undef CHUNK_SIZE
#define CHUNK_SIZE (HEADER_SIZE + ALIGNMENT)  // Use a minimum sensible chunk size
#warning "Defined CHUNK_SIZE was too small, adjusting."
// Recalculate shift if needed, or accept non-power-of-2 chunk size (more complex)
// For simplicity, we'll proceed assuming the initial CHUNK_SHIFT was reasonable.
#endif

#define BITS_PER_BYTE CHAR_BIT

// --- Global State ---
static uint8_t memory_raw[MEMORY_SIZE];      // Raw memory buffer
static uint8_t* memory_pool         = NULL;  // Start of allocatable pool (after bitmap)
static size_t effective_memory_size = 0;     // Size of allocatable pool
static uint8_t* bitmap              = NULL;  // Pointer to the bitmap within memory_raw
static size_t num_chunks            = 0;
static size_t bitmap_size_bytes     = 0;

// Block header structure (doubly-linked) - Size calculated later
typedef struct block_header {
    size_t size;                // Total size of the block (including header)
    struct block_header* prev;  // Pointer to the previous block in memory layout
    struct block_header* next;  // Pointer to the next block in memory layout
    uint32_t magic;             // Magic value for error detection/status
} block_header;

// Align a given size up to the nearest multiple of alignment
static size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Calculate aligned header size - Needs to be calculated *after* struct definition
#define HEADER_SIZE (align_up(sizeof(block_header), ALIGNMENT))

// --- Bitmap Helper Functions ---

// Get chunk index from a memory address (relative to memory_pool start)
static inline size_t get_chunk_index(void* ptr) {
    uintptr_t offset = (uintptr_t)ptr - (uintptr_t)memory_pool;
    return offset >> CHUNK_SHIFT;  // Equivalent to offset / CHUNK_SIZE
}

// Set bitmap bit (mark chunk as fully allocated)
static inline void bitmap_set(size_t chunk_index) {
    if (chunk_index >= num_chunks)
        return;
    bitmap[chunk_index / BITS_PER_BYTE] |= (1 << (chunk_index % BITS_PER_BYTE));
}

// Clear bitmap bit (mark chunk as potentially having free space)
static inline void bitmap_clear(size_t chunk_index) {
    if (chunk_index >= num_chunks)
        return;
    bitmap[chunk_index / BITS_PER_BYTE] &= ~(1 << (chunk_index % BITS_PER_BYTE));
}

// Test bitmap bit (1 = fully allocated, 0 = may have free space)
static inline bool bitmap_test(size_t chunk_index) {
    if (chunk_index >= num_chunks)
        return true;  // Out of bounds = considered allocated
    return (bitmap[chunk_index / BITS_PER_BYTE] >> (chunk_index % BITS_PER_BYTE)) & 1;
}

// Find the first chunk index >= start_chunk_index with its bit clear (0)
// Returns num_chunks if no suitable chunk is found.
static size_t bitmap_scan(size_t start_chunk_index) {
    size_t i = start_chunk_index;
    // Optimize: Quickly skip full bytes in the bitmap
    while (i < num_chunks && (i % BITS_PER_BYTE != 0)) {
        if (!bitmap_test(i))
            return i;
        i++;
    }
    while (i < num_chunks) {
        size_t byte_index = i / BITS_PER_BYTE;
        if (bitmap[byte_index] != 0xFF) {  // If byte is not all 1s
            // Scan bits within this byte
            for (size_t bit_offset = (i % BITS_PER_BYTE); bit_offset < BITS_PER_BYTE; ++bit_offset) {
                if (i + bit_offset - (i % BITS_PER_BYTE) < num_chunks &&
                    !bitmap_test(i + bit_offset - (i % BITS_PER_BYTE))) {
                    return i + bit_offset - (i % BITS_PER_BYTE);
                }
            }
            // Should not happen if byte != 0xFF, but safety check
            i = (byte_index + 1) * BITS_PER_BYTE;
        } else {
            // Skip this whole byte
            i = (byte_index + 1) * BITS_PER_BYTE;
        }
    }
    return num_chunks;  // Not found
}

// --- Core Allocator Helper Functions ---

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
    return header && header->magic == MAGIC_FREE;
}

// Check if a block header pointer itself is within the valid pool range
static inline bool is_valid_header_ptr(block_header* hdr) {
    return hdr && (uint8_t*)hdr >= memory_pool && (uint8_t*)hdr < (memory_pool + effective_memory_size);
}

// Initialize the memory pool, bitmap, and the first free block
__attribute__((constructor)) static void initialize_memory() {
    // 1. Calculate bitmap size
    num_chunks        = (MEMORY_SIZE + CHUNK_SIZE - 1) >> CHUNK_SHIFT;  // Ceiling division
    bitmap_size_bytes = (num_chunks + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
    bitmap_size_bytes = align_up(bitmap_size_bytes, ALIGNMENT);  // Align bitmap size

    // 2. Check if memory is large enough for bitmap and at least one header
    if (MEMORY_SIZE <= bitmap_size_bytes + HEADER_SIZE) {
        fprintf(stderr,
                "Error: MEMORY_SIZE (%d) too small for bitmap (%zu) and header (%zu).\n",
                MEMORY_SIZE,
                bitmap_size_bytes,
                HEADER_SIZE);
        exit(EXIT_FAILURE);
    }

    // 3. Assign pointers
    bitmap                = (uint8_t*)memory_raw;
    memory_pool           = memory_raw + bitmap_size_bytes;
    effective_memory_size = MEMORY_SIZE - bitmap_size_bytes;

    // 4. Initialize Bitmap: Mark all chunks initially as potentially free (0)
    // We'll refine this after creating the first block.
    memset(bitmap, 0, bitmap_size_bytes);

    // 5. Initialize the first block covering the entire effective memory
    block_header* header = (block_header*)memory_pool;
    header->size         = effective_memory_size;
    header->prev         = NULL;  // First block
    header->next         = NULL;  // Only block initially
    header->magic        = MAGIC_FREE;

    // 6. Update bitmap for the initial free block
    // Clear bits for all chunks covered by this initial large block
    size_t start_chunk = get_chunk_index(header);
    // The end address is header + size. The last byte is at header + size - 1.
    size_t end_chunk = get_chunk_index((uint8_t*)header + header->size - 1);
    for (size_t i = start_chunk; i <= end_chunk && i < num_chunks; ++i) {
        bitmap_clear(i);  // Mark as potentially free
    }
    // Any chunks *beyond* the initial block remain implicitly "allocated" (or unused)
    // due to the initial memset(0) potentially being overwritten if the pool doesn't
    // perfectly align with chunk boundaries. It's safer to mark them allocated.
    for (size_t i = end_chunk + 1; i < num_chunks; ++i) {
        bitmap_set(i);
    }

    // printf("Memory Initialized: Pool @ %p, Size %zu, Bitmap Size %zu, Chunks %zu\n",
    //        (void*)memory_pool, effective_memory_size, bitmap_size_bytes, num_chunks);
}

// Mark chunks covered by a block as potentially free
static void update_bitmap_for_free_block(block_header* header) {
    if (!is_block_free(header))
        return;  // Only for free blocks

    void* block_start = (void*)header;
    void* block_end   = (uint8_t*)header + header->size - 1;  // Last byte

    size_t start_chunk = get_chunk_index(block_start);
    size_t end_chunk   = get_chunk_index(block_end);

    for (size_t i = start_chunk; i <= end_chunk && i < num_chunks; ++i) {
        bitmap_clear(i);  // Mark chunk i as potentially having free space
    }
}

// Check if a chunk contains ONLY allocated blocks. If so, mark it full (set bit).
// This is EXPENSIVE - avoid calling frequently. Maybe only during debugging or specific scenarios.
/*
static void check_and_mark_chunk_full(size_t chunk_index) {
    if (chunk_index >= num_chunks) return;

    uint8_t* chunk_start = memory_pool + (chunk_index << CHUNK_SHIFT);
    uint8_t* chunk_end = chunk_start + CHUNK_SIZE;
    if (chunk_end > memory_pool + effective_memory_size) {
        chunk_end = memory_pool + effective_memory_size;
    }

    // Find the first block that starts in or overlaps with this chunk
    block_header* current = (block_header*)memory_pool;
    while (current && (uint8_t*)current + current->size <= chunk_start) {
        current = current->next; // Skip blocks entirely before the chunk
    }

    bool found_free = false;
    while (current && (uint8_t*)current < chunk_end) {
        if (is_block_free(current)) {
            found_free = true;
            break; // Found a free block, chunk is not full
        }
        current = current->next;
    }

    if (!found_free) {
        bitmap_set(chunk_index); // No free blocks found in the chunk range
    } else {
         bitmap_clear(chunk_index); // Ensure it's marked as having free space
    }
}
*/

// Split a block if the remainder is large enough for a new block.
static void split_block_if_possible(block_header* header, size_t required_size) {
    size_t remaining_size = header->size - required_size;

    if (remaining_size >= HEADER_SIZE + ALIGNMENT) {
        block_header* new_free_block = (block_header*)((uint8_t*)header + required_size);
        new_free_block->size         = remaining_size;
        new_free_block->magic        = MAGIC_FREE;
        new_free_block->prev         = header;
        new_free_block->next         = header->next;

        if (header->next) {
            header->next->prev = new_free_block;
        }

        header->size = required_size;
        header->next = new_free_block;

        // Update bitmap for the newly created free block
        update_bitmap_for_free_block(new_free_block);
    }
    // Else: Allocate the whole block. Bitmap update happens in FFREE if/when freed.
    // If we were implementing the "mark as full" logic, we'd potentially call
    // check_and_mark_chunk_full() here for the chunk(s) the allocated block occupies.
}

// Coalesce a free block 'header' with its next neighbor if it's also free.
static bool coalesce_with_next(block_header* header) {
    // Ensure headers are valid before dereferencing members like 'is_free' (magic) or 'next'
    if (!is_valid_header_ptr(header) || !is_block_free(header) || !is_valid_header_ptr(header->next)) {
        return false;
    }
    // Now safe to check next block's status
    if (!is_block_free(header->next)) {
        return false;
    }

    block_header* next_block = header->next;
    header->size += next_block->size;
    header->next = next_block->next;
    if (header->next) {
        if (is_valid_header_ptr(header->next)) {  // Check validity before accessing prev
            header->next->prev = header;
        } else {
            // This indicates list corruption if header->next was not NULL
            // Handle error or log? For now, assume valid list structure
            fprintf(stderr,
                    "Warning: Corrupted list detected during coalesce (next block invalid header %p)\n",
                    (void*)header->next);
            header->next = NULL;  // Try to recover by terminating list here? Risky.
        }
    }

    // Bitmap update is handled by the caller (FFREE) after all coalescing.
    return true;
}

// Validate payload pointer (basic range and alignment)
static bool is_valid_payload_ptr(void* ptr) {
    if (ptr == NULL)
        return false;

    uintptr_t ptr_addr           = (uintptr_t)ptr;
    uintptr_t pool_start         = (uintptr_t)memory_pool;
    uintptr_t payload_area_start = pool_start + HEADER_SIZE;  // First possible payload addr
    uintptr_t pool_end           = pool_start + effective_memory_size;

    if (ptr_addr < payload_area_start || ptr_addr >= pool_end) {
        return false;  // Out of range
    }

    // Check alignment relative to the start of the payload area
    if ((ptr_addr - pool_start - HEADER_SIZE) % ALIGNMENT != 0) {
        // Misaligned pointer, cannot be a valid start of our payload
        return false;
    }

    return true;
}

// --- Public API ---

void* FMALLOC(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t aligned_payload_size = align_up(size, ALIGNMENT);
    size_t total_required_size  = HEADER_SIZE + aligned_payload_size;

    if (total_required_size < HEADER_SIZE + ALIGNMENT || total_required_size > effective_memory_size) {
        return NULL;  // Overflow or request too large
    }

    // --- Bitmap-Accelerated Search ---
    size_t current_chunk_index      = 0;
    block_header* search_start_node = (block_header*)memory_pool;

    while (current_chunk_index < num_chunks) {
        // Find the next potentially free chunk using the bitmap
        size_t free_chunk_index = bitmap_scan(current_chunk_index);

        if (free_chunk_index >= num_chunks) {
            return NULL;  // No potentially free chunks found
        }

        // Calculate the memory address for the start of this chunk
        uint8_t* chunk_start_addr = memory_pool + (free_chunk_index << CHUNK_SHIFT);

        // Advance the linked-list search pointer ('current') efficiently
        // Skip nodes that end before this chunk starts
        block_header* current = search_start_node;
        while (current && ((uint8_t*)current + current->size <= chunk_start_addr)) {
            current = current->next;
        }
        // 'current' now points to the first block that *might* overlap
        // or start within the promising chunk. If current is NULL, something is wrong or we reached the end.

        // --- Linked List Search within Promising Chunks ---
        while (current) {
            // Optimization: If current block starts beyond the *next* promising chunk,
            // break inner loop and find next chunk via bitmap.
            size_t block_start_chunk = get_chunk_index(current);
            if (block_start_chunk > free_chunk_index && bitmap_test(free_chunk_index)) {
                // The previous chunk was scanned and found nothing suitable,
                // and it's now known to be full (or wasn't promising to begin with).
                // Move to the chunk this block is actually in.
                // This check helps skip over large allocated blocks faster.
                // free_chunk_index = block_start_chunk; // Update our idea of where we are
                break;  // Break inner while, outer loop will use bitmap_scan from current_chunk_index
            }

            // Check if the block is suitable
            if (is_block_free(current) && current->size >= total_required_size) {
                // Found a suitable block.
                split_block_if_possible(current, total_required_size);
                current->magic = MAGIC_ALLOCATED;

                // OPTIONAL: Check if this allocation made the chunk(s) full.
                // This is complex/slow, so we usually skip it and rely on FFREE
                // to clear bits when space becomes available again.
                // size_t start_idx = get_chunk_index(current);
                // size_t end_idx = get_chunk_index((uint8_t*)current + current->size - 1);
                // for(size_t i = start_idx; i <= end_idx; ++i) check_and_mark_chunk_full(i);

                return header_to_payload(current);
            }

            // Move to the next block in the list
            current = current->next;
            if (current && (uintptr_t)current >= (uintptr_t)chunk_start_addr + CHUNK_SIZE) {
                // We have scanned past the end of the current promising chunk.
                // Break the inner loop to find the next promising chunk via bitmap.
                search_start_node = current;  // Start next list scan from here
                break;
            }
        }  // End of linked list scan within/past the chunk

        // If we exited the inner loop because current is NULL, we are done searching.
        if (!current) {
            return NULL;
        }

        // If we exited because we passed the chunk, update chunk index and continue outer loop
        current_chunk_index = get_chunk_index((void*)current);  // Start scan from the chunk 'current' is in

    }  // End of bitmap scan loop

    return NULL;  // No suitable block found
}

void FFREE(void* ptr) {
    if (!is_valid_payload_ptr(ptr)) {
        // fprintf(stderr, "Warning: Attempt to FFREE invalid pointer %p\n", ptr);
        return;
    }

    block_header* header = payload_to_header(ptr);

    if (header->magic != MAGIC_ALLOCATED) {
        // fprintf(stderr, "Warning: Double free or corruption detected for pointer %p (header %p, magic 0x%x)\n", ptr,
        // (void*)header, header->magic);
        return;
    }

    header->magic = MAGIC_FREE;

    // --- Immediate Coalescing ---
    coalesce_with_next(header);  // Try merge forward

    block_header* block_to_update_bitmap = header;  // Start with current header

    // Try merge backward
    if (is_valid_header_ptr(header->prev) && is_block_free(header->prev)) {
        // Previous block becomes the owner of the merged block
        block_to_update_bitmap = header->prev;
        coalesce_with_next(header->prev);  // Merge header into header->prev
    }

    // --- Update Bitmap ---
    // Update bitmap based on the final state of the free block (block_to_update_bitmap)
    update_bitmap_for_free_block(block_to_update_bitmap);
}

void* FCALLOC(size_t nmemb, size_t size) {
    size_t total_size;
    if (nmemb == 0 || size == 0) {
        total_size = 0;
    } else if (__builtin_mul_overflow(nmemb, size, &total_size)) {
        return NULL;  // Overflow
    }

    if (total_size == 0) {
        return FMALLOC(0);  // Should return NULL per our FMALLOC logic
    }

    void* ptr = FMALLOC(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);  // Zero out the allocated memory
    }
    return ptr;
}

void* FREALLOC(void* ptr, size_t size) {
    if (ptr == NULL) {
        return FMALLOC(size);
    }

    // Validate pointer *before* trying to access header
    if (!is_valid_payload_ptr(ptr)) {
        fprintf(stderr, "ERROR: FREALLOC called with invalid pointer %p\n", ptr);
        return NULL;  // Or handle error differently
    }
    block_header* header = payload_to_header(ptr);
    if (header->magic != MAGIC_ALLOCATED) {
        fprintf(stderr,
                "ERROR: FREALLOC called on non-allocated or corrupted block %p (magic: 0x%x)\n",
                (void*)header,
                header->magic);
        return NULL;
    }

    if (size == 0) {
        FFREE(ptr);
        return NULL;
    }

    size_t aligned_new_payload_size = align_up(size, ALIGNMENT);
    size_t total_required_size      = HEADER_SIZE + aligned_new_payload_size;
    size_t current_payload_size     = header->size - HEADER_SIZE;

    // --- Optimization 1: Shrinking in place ---
    if (total_required_size <= header->size) {
        split_block_if_possible(header, total_required_size);
        // No data move needed. Bitmap updated by split_block_if_possible if needed.
        return ptr;
    }

    // --- Optimization 2: Expanding in place (using next block) ---
    if (header->next && is_block_free(header->next) && is_valid_header_ptr(header->next) &&  // Sanity check next ptr
        (header->size + header->next->size >= total_required_size)) {
        block_header* next_block = header->next;
        size_t combined_size     = header->size + next_block->size;  // Store before modifying list

        // Temporarily store next->next info
        block_header* original_next_next      = next_block->next;
        block_header* original_next_next_prev = header;  // We become the new prev

        // Merge next block into current
        header->size = combined_size;       // Update size first
        header->next = original_next_next;  // Link past the merged block
        if (header->next) {
            if (is_valid_header_ptr(header->next)) {
                header->next->prev = original_next_next_prev;  // Update next block's prev
            } else {
                fprintf(stderr,
                        "Warning: Corrupted list detected during FREALLOC expand (next->next invalid %p)\n",
                        (void*)header->next);
                header->next = NULL;
            }
        }

        // Split the newly enlarged block if necessary
        split_block_if_possible(header, total_required_size);
        // Bitmap for the *newly created* free block (if any) was updated in split.
        // The chunks previously occupied by the original next_block are now part
        // of the allocated block 'header'. We don't explicitly mark them full here.
        return ptr;
    }

    // --- Fallback: Allocate, Copy, Free ---
    void* new_ptr = FMALLOC(size);
    if (new_ptr == NULL) {
        return NULL;  // Allocation failed
    }

    size_t copy_size = (current_payload_size < size) ? current_payload_size : size;
    memcpy(new_ptr, ptr, copy_size);

    FFREE(ptr);  // Free the old block (this updates the bitmap)

    return new_ptr;
}

// Print memory state (includes bitmap info)
void FDEBUG_MEMORY() {
    block_header* current = (block_header*)memory_pool;
    printf("--- Memory State (Pool @ %p, Size %zu, Bitmap Size %zu, Chunks %zu) ---\n",
           (void*)memory_pool,
           effective_memory_size,
           bitmap_size_bytes,
           num_chunks);

    // Print Bitmap
    printf("Bitmap (%p, %zu bytes): ", (void*)bitmap, bitmap_size_bytes);
    for (size_t i = 0; i < bitmap_size_bytes; ++i) {
        printf("%02X ", bitmap[i]);
        if ((i + 1) % 16 == 0 && i + 1 < bitmap_size_bytes)
            printf("\n Bitmap cont'd: ");
    }
    printf("\n (0=MayHaveFree, 1=Full)\n");

    // Print Blocks
    printf("Blocks (Header Size: %zu):\n", HEADER_SIZE);
    int i                        = 0;
    uintptr_t expected_next_addr = (uintptr_t)memory_pool;
    while (current && is_valid_header_ptr(current)) {
        uintptr_t current_addr = (uintptr_t)current;
        // Check for physical continuity
        if (current_addr != expected_next_addr) {
            printf("   *** GAP DETECTED! Expected header at %p, but found at %p ***\n",
                   (void*)expected_next_addr,
                   (void*)current);
        }

        size_t start_chunk = get_chunk_index(current);
        size_t end_chunk   = get_chunk_index((uint8_t*)current + current->size - 1);

        printf(" [%d] Block @ %p: size = %-6zu, magic = 0x%x (%s), prev = %-10p, next = %p [Chunks %zu-%zu]\n",
               i++,
               (void*)current,
               current->size,
               current->magic,
               (current->magic == MAGIC_ALLOCATED ? "ALLOC" : (current->magic == MAGIC_FREE ? "FREE " : "?????")),
               (void*)current->prev,
               (void*)current->next,
               start_chunk,
               end_chunk);

        // Sanity checks (optional)
        if (current->next && (uintptr_t)current->next != (uintptr_t)current + current->size) {
            printf("     ERROR: Block end != next pointer! (%p != %p)\n",
                   (void*)((uintptr_t)current + current->size),
                   (void*)current->next);
        }
        if (current->next && is_valid_header_ptr(current->next) && current->next->prev != current) {
            printf(
                "     ERROR: next->prev != current pointer! (%p != %p)\n", (void*)current->next->prev, (void*)current);
        }
        if (current->prev && is_valid_header_ptr(current->prev) && current->prev->next != current) {
            printf(
                "     ERROR: prev->next != current pointer! (%p != %p)\n", (void*)current->prev->next, (void*)current);
        }
        if (current->size == 0) {
            printf("    ERROR: Block size is zero!\n");
            break;  // Stop to prevent infinite loop
        }

        expected_next_addr = current_addr + current->size;
        current            = current->next;

        // Prevent runaway loop if list is corrupted
        if (i > (int)(effective_memory_size / HEADER_SIZE) + 10) {  // Heuristic limit
            printf("   *** ERROR: Potentially corrupted list, too many blocks found. Aborting debug print. ***\n");
            break;
        }
    }
    if (expected_next_addr != (uintptr_t)memory_pool + effective_memory_size) {
        printf("   *** ERROR: End of last block (%p) does not match end of memory pool (%p)! ***\n",
               (void*)expected_next_addr,
               (void*)(memory_pool + effective_memory_size));
    }

    printf("--- End of Memory State ---\n\n");
}
