#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/fast_mem.h"

#define ALIGNMENT sizeof(void*)

// Magic numbers for block validation
#define MAGIC_FREE 0xDEADBEEF
#define MAGIC_ALLOCATED 0xBEEFDEAD

// Free list bins for small sizes: 16, 32, 64, 128, 256, 512, 1024, and "large"
#define NUM_BINS 8
static const size_t bin_sizes[NUM_BINS - 1] = {16, 32, 64, 128, 256, 512, 1024};

// Global memory pool and mutex
static uint8_t memory[MEMORY_SIZE];
static pthread_mutex_t memory_lock = PTHREAD_MUTEX_INITIALIZER;

// Block header structure (doubly-linked)
typedef struct block_header {
    size_t size;                // Total size of the block (including header)
    struct block_header* prev;  // Previous block in memory layout
    struct block_header* next;  // Next block in memory layout
    uint32_t magic;             // Magic value for error detection/status
} block_header;

// Align a size up to the nearest multiple of alignment
static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

#define HEADER_SIZE (align_up(sizeof(block_header), ALIGNMENT))

// Free lists for each bin
static block_header* free_lists[NUM_BINS];

// --- Helper Functions ---

static inline void* header_to_payload(block_header* header) {
    return (void*)((uint8_t*)header + HEADER_SIZE);
}

static inline block_header* payload_to_header(void* ptr) {
    return (block_header*)((uint8_t*)ptr - HEADER_SIZE);
}

static inline bool is_block_free(block_header* header) {
    return header && header->magic == MAGIC_FREE;
}

// Get bin index for a size
static inline int size_to_bin(size_t total_size) {
    for (int i = 0; i < NUM_BINS - 1; i++) {
        if (total_size <= align_up(bin_sizes[i] + HEADER_SIZE, ALIGNMENT))
            return i;
    }
    return NUM_BINS - 1;  // Large blocks
}

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
    }
}

static bool coalesce_with_next(block_header* header) {
    if (!header || !header->next || !is_block_free(header) || !is_block_free(header->next)) {
        return false;
    }
    block_header* next_block = header->next;
    header->size += next_block->size;
    header->next = next_block->next;
    if (header->next) {
        header->next->prev = header;
    }
    return true;
}

static bool is_valid_payload_ptr(void* ptr) {
    if (ptr == NULL)
        return false;
    uintptr_t ptr_addr      = (uintptr_t)ptr;
    uintptr_t mem_start     = (uintptr_t)memory;
    uintptr_t payload_start = mem_start + HEADER_SIZE;
    uintptr_t mem_end       = mem_start + MEMORY_SIZE;

    if (ptr_addr < payload_start || ptr_addr >= mem_end)
        return false;
    if ((ptr_addr - mem_start - HEADER_SIZE) % ALIGNMENT != 0)
        return false;
    return true;
}

// --- Initialization ---

__attribute__((constructor)) static void initialize_memory() {
    if (MEMORY_SIZE < HEADER_SIZE) {
        fprintf(stderr, "Error: MEMORY_SIZE too small.\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "Memory size: %d\n", MEMORY_SIZE);
    block_header* header = (block_header*)memory;
    header->size         = MEMORY_SIZE;
    header->prev         = NULL;
    header->next         = NULL;
    header->magic        = MAGIC_FREE;

    // Initialize free lists
    for (int i = 0; i < NUM_BINS; i++) {
        free_lists[i] = NULL;
    }
    free_lists[NUM_BINS - 1] = header;  // Start with entire memory in "large" bin
}

// --- Public API ---

void* FMALLOC(size_t size) {
    if (size == 0)
        return NULL;

    pthread_mutex_lock(&memory_lock);

    size_t aligned_payload_size = align_up(size, ALIGNMENT);
    size_t total_required_size  = HEADER_SIZE + aligned_payload_size;

    if (total_required_size < HEADER_SIZE || total_required_size > MEMORY_SIZE) {
        pthread_mutex_unlock(&memory_lock);
        return NULL;
    }

    // Try free list first
    int bin = size_to_bin(total_required_size);
    if (bin < NUM_BINS - 1 && free_lists[bin]) {
        block_header* header = free_lists[bin];
        free_lists[bin]      = header->next;
        if (free_lists[bin])
            free_lists[bin]->prev = NULL;
        header->magic = MAGIC_ALLOCATED;
        void* result  = header_to_payload(header);
        pthread_mutex_unlock(&memory_lock);
        return result;
    }

    // Fallback to first-fit in large bin or full list
    block_header* current = (block_header*)memory;
    while (current) {
        if (is_block_free(current) && current->size >= total_required_size) {
            split_block_if_possible(current, total_required_size);
            current->magic = MAGIC_ALLOCATED;
            void* result   = header_to_payload(current);
            pthread_mutex_unlock(&memory_lock);
            return result;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&memory_lock);
    return NULL;
}

void FFREE(void* ptr) {
    pthread_mutex_lock(&memory_lock);

    if (!is_valid_payload_ptr(ptr)) {
        pthread_mutex_unlock(&memory_lock);
        return;
    }

    block_header* header = payload_to_header(ptr);
    if (header->magic != MAGIC_ALLOCATED) {
        pthread_mutex_unlock(&memory_lock);
        return;
    }

    header->magic = MAGIC_FREE;

    // Add to appropriate free list if small, else coalesce
    int bin = size_to_bin(header->size);
    if (bin < NUM_BINS - 1) {
        header->next = free_lists[bin];
        header->prev = NULL;
        if (free_lists[bin])
            free_lists[bin]->prev = header;
        free_lists[bin] = header;
    } else {
        coalesce_with_next(header);
        if (header->prev && is_block_free(header->prev)) {
            coalesce_with_next(header->prev);
        }
    }

    pthread_mutex_unlock(&memory_lock);
}

void* FCALLOC(size_t nmemb, size_t size) {
    size_t total_size;
    if (nmemb == 0 || size == 0)
        total_size = 0;
    else if (__builtin_mul_overflow(nmemb, size, &total_size)) {
        return NULL;
    }

    void* ptr = FMALLOC(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void* FREALLOC(void* ptr, size_t size) {
    pthread_mutex_lock(&memory_lock);

    if (ptr == NULL) {
        void* result = FMALLOC(size);
        pthread_mutex_unlock(&memory_lock);
        return result;
    }
    if (size == 0) {
        FFREE(ptr);
        pthread_mutex_unlock(&memory_lock);
        return NULL;
    }

    if (!is_valid_payload_ptr(ptr)) {
        pthread_mutex_unlock(&memory_lock);
        return NULL;
    }
    block_header* header = payload_to_header(ptr);
    if (header->magic != MAGIC_ALLOCATED) {
        pthread_mutex_unlock(&memory_lock);
        return NULL;
    }

    size_t current_payload_size     = header->size - HEADER_SIZE;
    size_t aligned_new_payload_size = align_up(size, ALIGNMENT);
    size_t total_required_size      = HEADER_SIZE + aligned_new_payload_size;

    if (total_required_size <= header->size) {
        split_block_if_possible(header, total_required_size);
        void* result = header_to_payload(header);
        pthread_mutex_unlock(&memory_lock);
        return result;
    }

    if (header->next && is_block_free(header->next) &&
        (header->size + header->next->size >= total_required_size)) {
        block_header* next_block = header->next;
        header->size += next_block->size;
        header->next = next_block->next;
        if (header->next)
            header->next->prev = header;
        split_block_if_possible(header, total_required_size);
        void* result = header_to_payload(header);
        pthread_mutex_unlock(&memory_lock);
        return result;
    }

    void* new_ptr = FMALLOC(size);
    if (new_ptr == NULL) {
        pthread_mutex_unlock(&memory_lock);
        return NULL;
    }

    size_t copy_size = (current_payload_size < size) ? current_payload_size : size;
    memcpy(new_ptr, ptr, copy_size);
    FFREE(ptr);  // Note: FFREE already unlocks, so we unlock here first
    pthread_mutex_unlock(&memory_lock);
    return new_ptr;
}

void FDEBUG_MEMORY() {
    pthread_mutex_lock(&memory_lock);

    block_header* current = (block_header*)memory;
    printf("Memory state (Total Size: %d, Header Size: %zu):\n", MEMORY_SIZE, HEADER_SIZE);
    int i = 0;
    while (current) {
        printf(" [%d] Block @ %p: size = %-6zu, magic = 0x%x (%s), prev = %-10p, next = %p\n", i++,
               (void*)current, current->size, current->magic,
               (current->magic == MAGIC_ALLOCATED ? "ALLOC"
                                                  : (current->magic == MAGIC_FREE ? "FREE " : "?????")),
               (void*)current->prev, (void*)current->next);
        current = current->next;
    }

    printf("Free Lists:\n");
    for (int bin = 0; bin < NUM_BINS; bin++) {
        printf("  Bin %d (up to %zu bytes): ", bin, bin < NUM_BINS - 1 ? bin_sizes[bin] : MEMORY_SIZE);
        current = free_lists[bin];
        while (current) {
            printf("%p -> ", (void*)current);
            current = current->next;
        }
        printf("NULL\n");
    }
    printf("---- End of Memory State ----\n\n");

    pthread_mutex_unlock(&memory_lock);
}

// Optional: Simple test main
#ifdef TEST_MAIN
int main() {
    void* p1 = FMALLOC(32);
    void* p2 = FMALLOC(64);
    FDEBUG_MEMORY();
    FFREE(p1);
    FDEBUG_MEMORY();
    void* p3 = FMALLOC(16);
    FDEBUG_MEMORY();
    FFREE(p2);
    FFREE(p3);
    FDEBUG_MEMORY();
    return 0;
}
#endif
