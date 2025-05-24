
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_SIZE 1024  // Total memory size in bytes
#define ALIGNMENT 8       // Alignment requirement (e.g., 8 bytes)

// Magic numbers for block validation
#define MAGIC_FREE 0xDEADBEEF
#define MAGIC_ALLOCATED 0xBEEFDEAD

// Global memory pool
static uint8_t memory[MEMORY_SIZE];

// Block header structure
typedef struct block_header {
    size_t size;                // Total size of the block (including header)
    struct block_header* next;  // Pointer to the next block in our list
    bool is_free;               // Flag to indicate if this block is free
    uint32_t magic;             // Magic value for error detection
} block_header;

// Align a given size up to the nearest multiple of alignment
static size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Returns the aligned header size (so header itself is aligned)
static size_t header_size(void) {
    return align_up(sizeof(block_header), ALIGNMENT);
}

// Initialize the memory pool with a single free block
static void initialize_memory() {
    block_header* header = (block_header*)memory;
    header->size         = MEMORY_SIZE;
    header->next         = NULL;
    header->is_free      = true;
    header->magic        = MAGIC_FREE;
}

// Split a block into an allocated block of 'size' bytes and a free remainder.
// 'size' should be the total size (header + payload) needed.
static void split_block(block_header* header, size_t size) {
    // Check if the remaining space is enough for a new block header plus minimal
    // payload.
    if (header->size >= size + header_size() + ALIGNMENT) {
        block_header* new_block = (block_header*)((uint8_t*)header + size);
        new_block->size         = header->size - size;
        new_block->next         = header->next;
        new_block->is_free      = true;
        new_block->magic        = MAGIC_FREE;
        header->size            = size;
        header->next            = new_block;
    }
}

// Coalesce adjacent free blocks into one large free block
static void coalesce_blocks() {
    block_header* current = (block_header*)memory;
    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            current->size += current->next->size;
            current->next = current->next->next;
            // The magic remains as MAGIC_FREE.
        } else {
            current = current->next;
        }
    }
}

// Validate that a pointer belongs to our memory pool.
static bool is_valid_ptr(void* ptr) {
    if (ptr == NULL)
        return false;

    uint8_t* byte_ptr = (uint8_t*)ptr;
    return (byte_ptr >= memory + header_size() && byte_ptr < memory + MEMORY_SIZE);
}

// Simple malloc implementation with alignment and block splitting.
void* my_malloc(size_t size) {
    if (size == 0 || size > MEMORY_SIZE - header_size()) {
        return NULL;
    }

    // Align the requested payload size.
    size_t aligned_size = align_up(size, ALIGNMENT);
    // Total size = header size + payload size.
    size_t total_size = header_size() + aligned_size;

    block_header* current = (block_header*)memory;
    while (current) {
        if (current->is_free && current->size >= total_size) {
            // Found a free block large enough.
            split_block(current, total_size);
            current->is_free = false;
            current->magic   = MAGIC_ALLOCATED;
            // Return pointer to the payload, which is after the header.
            return (void*)((uint8_t*)current + header_size());
        }
        current = current->next;
    }

    // No free block found.
    return NULL;
}

// Simple free implementation with pointer validation and coalescing.
void my_free(void* ptr) {
    if (!is_valid_ptr(ptr)) {
        fprintf(stderr, "my_free: invalid pointer %p\n", ptr);
        return;
    }

    // Retrieve the header by subtracting the header size.
    block_header* header = (block_header*)((uint8_t*)ptr - header_size());

    // Check for double free or corruption.
    if (header->magic != MAGIC_ALLOCATED) {
        fprintf(stderr, "my_free: pointer %p was not allocated or already freed\n", ptr);
        return;
    }

    header->is_free = true;
    header->magic   = MAGIC_FREE;

    // Optionally, clear the memory in the payload (for debugging/security)
    memset(ptr, 0, header->size - header_size());

    // Coalesce adjacent free blocks.
    coalesce_blocks();
}

// Additional helper: calloc implementation.
void* my_calloc(size_t nmemb, size_t size) {
    // Check for overflow.
    if (nmemb != 0 && size > (size_t)-1 / nmemb)
        return NULL;

    size_t total = nmemb * size;
    void* ptr    = my_malloc(total);

    if (ptr)
        memset(ptr, 0, total);
    return ptr;
}

// Additional helper: realloc implementation.
void* my_realloc(void* ptr, size_t size) {
    // If ptr is NULL, behave like malloc.
    if (ptr == NULL)
        return my_malloc(size);

    // If size is 0, free the block and return NULL.
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    // Retrieve the original block header.
    block_header* header    = (block_header*)((uint8_t*)ptr - header_size());
    size_t old_payload_size = header->size - header_size();
    size_t aligned_size     = align_up(size, ALIGNMENT);
    size_t total_size       = header_size() + aligned_size;

    // If the current block is large enough, we can re-use it.
    if (header->size >= total_size) {
        // Optionally, split the block if there is significant extra space.
        split_block(header, total_size);
        return ptr;
    }

    // Otherwise, allocate a new block.
    void* new_ptr = my_malloc(size);
    if (new_ptr) {
        // Copy the smaller of the two sizes.
        size_t copy_size = old_payload_size < size ? old_payload_size : size;
        memcpy(new_ptr, ptr, copy_size);
        my_free(ptr);
    }
    return new_ptr;
}

// Debug helper: print the state of the memory blocks.
void print_memory_state() {
    block_header* current = (block_header*)memory;
    printf("Memory state:\n");
    while (current) {
        printf(" Block @ %p: size = %zu, %s, magic = 0x%x, next = %p\n",
               (void*)current,
               current->size,
               current->is_free ? "free" : "allocated",
               current->magic,
               (void*)current->next);
        current = current->next;
    }
    printf("\n");
}

// Example usage
int main() {
    initialize_memory();

    // Allocate an array of 10 integers.
    int* arr = (int*)my_malloc(10 * sizeof(int));
    assert(arr != NULL);

    // Use the allocated memory.
    for (int i = 0; i < 10; i++) {
        arr[i] = i;
    }

    // Print the array.
    for (int i = 0; i < 10; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    // Print memory state after allocation.
    print_memory_state();

    // Reallocate the array to 20 integers.
    arr = (int*)my_realloc(arr, 20 * sizeof(int));
    assert(arr != NULL);
    for (int i = 10; i < 20; i++) {
        arr[i] = i;
    }

    // Print the updated array.
    for (int i = 0; i < 20; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    // Print memory state after reallocation.
    print_memory_state();

    // Free the memory.
    my_free(arr);
    print_memory_state();

    // Example calloc usage.
    char* buffer = (char*)my_calloc(50, sizeof(char));
    if (buffer) {
        snprintf(buffer, 50, "Hello, custom allocator!");
        printf("%s\n", buffer);
        my_free(buffer);
    }

    return 0;
}
