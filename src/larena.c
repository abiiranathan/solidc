#include "../include/larena.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform-specific headers for aligned allocation
#ifdef _WIN32
#include <malloc.h>  // For _aligned_malloc/_aligned_free
#elif defined(__APPLE__)
#include <stdlib.h>  // For aligned_alloc (macOS 10.15+) or fallback
#else
#include <stdlib.h>  // For aligned_alloc (C11) or posix_memalign
#endif

// Cache line size (64 bytes on most modern CPUs)
#define CACHE_LINE_SIZE 64

#ifndef ARENA_ALIGNMENT
#define ARENA_ALIGNMENT 16
#endif

// Compiler hints for branch prediction and optimization
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define NOINLINE __attribute__((noinline))
#define ALWAYS_INLINE __attribute__((always_inline)) inline

// Error logging that can be disabled in production
#ifdef NDEBUG
#define LOG_ERROR(fmt, ...) ((void)0)
#else
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

typedef struct LArena {
    char* memory;
    size_t allocated;
    size_t size;
} LArena;

// Prefetch hints
#define PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
#define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)

ALWAYS_INLINE size_t align_up(size_t size, size_t align) {
    assert((align & (align - 1)) == 0);  // Power of 2 check
    return (size + align - 1) & ~(align - 1);
}

// Cross-platform aligned memory allocation
static void* aligned_alloc_cross_platform(size_t alignment, size_t size) {
#ifdef _WIN32
    // Windows: use _aligned_malloc
    return _aligned_malloc(size, alignment);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    // C11 aligned_alloc (requires size to be multiple of alignment)
    size_t aligned_size = align_up(size, alignment);
    return aligned_alloc(alignment, aligned_size);
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    // POSIX posix_memalign
    void* ptr;
    if (posix_memalign(&ptr, alignment, size) == 0) {
        return ptr;
    }
    return NULL;
#else
    // Fallback: manual alignment using malloc
    // Allocate extra space for alignment + pointer storage
    size_t total_size = size + alignment + sizeof(void*);
    void* raw_ptr     = malloc(total_size);
    if (!raw_ptr) {
        return NULL;
    }

    // Calculate aligned address
    uintptr_t raw_addr     = (uintptr_t)raw_ptr;
    uintptr_t aligned_addr = align_up(raw_addr + sizeof(void*), alignment);
    void* aligned_ptr      = (void*)aligned_addr;

    // Store original pointer just before the aligned memory
    ((void**)aligned_ptr)[-1] = raw_ptr;

    return aligned_ptr;
#endif
}

// Cross-platform aligned memory deallocation
static void aligned_free_cross_platform(void* ptr) {
    if (!ptr)
        return;

#ifdef _WIN32
    // Windows: use _aligned_free
    _aligned_free(ptr);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    // C11 aligned_alloc uses regular free
    free(ptr);
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    // POSIX posix_memalign uses regular free
    free(ptr);
#else
    // Fallback: retrieve original pointer and free it
    void* raw_ptr = ((void**)ptr)[-1];
    free(raw_ptr);
#endif
}

// Create arena with optimized alignment and prefetching
NOINLINE LArena* larena_create(size_t size) {
    // Align size to cache lines and add one extra cache line for prefetching
    size = align_up(size, CACHE_LINE_SIZE) + CACHE_LINE_SIZE;

    LArena* arena = malloc(sizeof(LArena));
    if (UNLIKELY(!arena)) {
        LOG_ERROR("Failed to allocate arena struct");
        return NULL;
    }

    // Allocate with cache line alignment using cross-platform function
    arena->memory = (char*)aligned_alloc_cross_platform(CACHE_LINE_SIZE, size);
    if (UNLIKELY(!arena->memory)) {
        LOG_ERROR("Aligned allocation failed");
        free(arena);
        return NULL;
    }

    // Prefetch the first cache line
    PREFETCH_WRITE(arena->memory);

    arena->size      = size;
    arena->allocated = 0;
    return arena;
}

// Pure C optimized allocation with aligned access
ALWAYS_INLINE void* larena_alloc_aligned(LArena* arena, size_t size, size_t align) {
    if (UNLIKELY(!arena || (align & (align - 1)))) {
        return NULL;
    }

    // Align the current allocation pointer
    size_t offset    = align_up(arena->allocated, align);
    size_t new_alloc = offset + size;

    if (UNLIKELY(new_alloc > arena->size)) {
        return NULL;
    }

    void* ptr        = arena->memory + offset;
    arena->allocated = new_alloc;

    // Prefetch next cache line for sequential access patterns
    if (size >= CACHE_LINE_SIZE) {
        PREFETCH_WRITE((char*)ptr + CACHE_LINE_SIZE);
    }

    return ptr;
}

// Pure C optimized allocation (replaces assembly version)
ALWAYS_INLINE void* larena_alloc_optimized(LArena* arena, size_t size) {
    if (UNLIKELY(!arena)) {
        return NULL;
    }

    // Align to 16 bytes
    size_t offset    = align_up(arena->allocated, ARENA_ALIGNMENT);
    size_t new_alloc = offset + size;

    // Bounds check
    if (UNLIKELY(new_alloc > arena->size)) {
        return NULL;
    }

    // Update allocation pointer and return memory
    arena->allocated = new_alloc;
    return arena->memory + offset;
}

ALWAYS_INLINE void* larena_alloc(LArena* arena, size_t size) {
    return larena_alloc_optimized(arena, size);
}

// Pure C calloc implementation (replaces assembly version)
ALWAYS_INLINE void* larena_calloc_optimized(LArena* arena, size_t count, size_t size) {
    if (UNLIKELY(!arena || !count || !size)) {
        return NULL;
    }

    // Check for overflow using division
    if (UNLIKELY(count > SIZE_MAX / size)) {
        return NULL;
    }

    size_t total_size = count * size;
    void* ptr         = larena_alloc(arena, total_size);

    if (LIKELY(ptr)) {
        // Zero the allocated memory
        memset(ptr, 0, total_size);
    }

    return ptr;
}

// Zero-fill allocation variant
void* larena_calloc(LArena* arena, size_t count, size_t size) {
    return larena_calloc_optimized(arena, count, size);
}

// Optimized string allocation
char* larena_alloc_string(LArena* arena, const char* s) {
    // Fast path for empty string
    if (UNLIKELY(*s == '\0')) {
        char* ptr = larena_alloc(arena, 1);
        if (LIKELY(ptr))
            *ptr = '\0';
        return ptr;
    }

    // Use strlen and allocate
    size_t len = strlen(s);
    char* ptr  = larena_alloc(arena, len + 1);
    if (UNLIKELY(!ptr))
        return NULL;

    // Copy string data
    memcpy(ptr, s, len);
    ptr[len] = '\0';
    return ptr;
}

// Resize with exponential growth strategy
bool larena_resize(LArena* arena, size_t new_size) {
    if (UNLIKELY(!arena || new_size <= arena->size)) {
        return false;
    }

    // Exponential growth factor of 1.5x (common in vector implementations)
    size_t growth_size = arena->size + (arena->size >> 1);
    new_size           = align_up(new_size > growth_size ? new_size : growth_size, CACHE_LINE_SIZE);

    void* new_memory = aligned_alloc_cross_platform(CACHE_LINE_SIZE, new_size);
    if (UNLIKELY(!new_memory)) {
        LOG_ERROR("Aligned allocation failed during resize");
        return false;
    }

    // Prefetch the new memory area
    PREFETCH_WRITE(new_memory);

    // Copy old content
    if (arena->allocated > 0) {
        memcpy(new_memory, arena->memory, arena->allocated);
    }

    aligned_free_cross_platform(arena->memory);

    arena->memory = (char*)new_memory;
    arena->size   = new_size;
    return true;
}

size_t larena_getfree_memory(LArena* arena) {
    return arena->size - arena->allocated;
}

void larena_reset(LArena* arena) {
    arena->allocated = 0;
}

void larena_destroy(LArena* arena) {
    if (LIKELY(arena)) {
        aligned_free_cross_platform(arena->memory);
        free(arena);
    }
}
