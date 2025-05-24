#include "../include/larena.h"
#include <assert.h>
#include <emmintrin.h>  // SSE2
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Create arena with optimized alignment and prefetching
NOINLINE LArena* larena_create(size_t size) {
    // Align size to cache lines and add one extra cache line for prefetching
    size = align_up(size, CACHE_LINE_SIZE) + CACHE_LINE_SIZE;

    LArena* arena = malloc(sizeof(LArena));
    if (UNLIKELY(!arena)) {
        LOG_ERROR("Failed to allocate arena struct");
        return NULL;
    }

    // Allocate with cache line alignment
    if (posix_memalign((void**)&arena->memory, CACHE_LINE_SIZE, size)) {
        LOG_ERROR("posix_memalign failed: %s", strerror(errno));
        free(arena);
        return NULL;
    }

    // Prefetch the first cache line
    PREFETCH_WRITE(arena->memory);

    arena->size      = size;
    arena->allocated = 0;
    return arena;
}

// Assembly-optimized hot path targeting the specific bottlenecks
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

ALWAYS_INLINE void* larena_alloc_asm_optimized(LArena* arena, size_t size) {
    void* result;
    size_t new_allocated;

    // Use inline assembly to control register allocation and eliminate redundant loads
    __asm__ volatile(
        "movq   %c4(%2), %%rax\n\t"  // Load arena->allocated
        "movq   %c5(%2), %%rdx\n\t"  // Load arena->size
        "movq   (%2), %%rcx\n\t"     // Load arena->memory

        // Alignment calculation (assuming 16-byte alignment)
        "addq   $15, %%rax\n\t"   // Add alignment - 1
        "andq   $-16, %%rax\n\t"  // Mask for alignment

        // Calculate new allocation
        "leaq   (%%rax,%3), %%r8\n\t"  // new_alloc = offset + size

        // Bounds check
        "cmpq   %%r8, %%rdx\n\t"  // Compare arena_size with new_alloc
        "jb     1f\n\t"           // Jump if out of bounds

        // Success path
        "movq   %%r8, %c4(%2)\n\t"  // Update arena->allocated
        "addq   %%rcx, %%rax\n\t"   // Calculate return pointer
        "jmp    2f\n\t"

        // Failure path
        "1:\n\t"
        "xorq   %%rax, %%rax\n\t"  // Return NULL

        "2:\n\t"
        "movq   %%rax, %0\n\t"  // Store result
        "movq   %%r8, %1\n\t"   // Store new_allocated for update

        : "=m"(result), "=m"(new_allocated)
        : "r"(arena), "r"(size), "i"(offsetof(LArena, allocated)), "i"(offsetof(LArena, size))
        : "rax", "rdx", "rcx", "r8", "memory");

    return result;
}

ALWAYS_INLINE void* larena_alloc(LArena* arena, size_t size) {
    return larena_alloc_asm_optimized(arena, size);
}

ALWAYS_INLINE void* larena_calloc_asm(LArena* arena, size_t count, size_t size) {
    size_t total_size;

    // Overflow check and size calculation in assembly
    __asm__ volatile(
        "mov %1, %%rax\n\t"
        "mul %2\n\t"             // count * size
        "mov %%rax, %0\n\t"      // store result
        "jc 1f\n\t"              // jump if overflow (CF=1)
        "test %%rdx, %%rdx\n\t"  // check high bits
        "jz 2f\n\t"
        "1:\n\t"
        "xor %%rax, %%rax\n\t"  // return NULL on overflow
        "2:\n\t"
        : "=r"(total_size)
        : "r"(count), "r"(size)
        : "rax", "rdx", "cc");

    if (!total_size)
        return NULL;

    void* ptr;
    size_t new_alloc;

    __asm__ volatile(
        // Load arena fields
        "movq %c4(%2), %%r8\n\t"  // allocated
        "movq %c5(%2), %%r9\n\t"  // size
        "movq (%2), %%r10\n\t"    // memory

        // Alignment calculation
        "lea 15(%%r8), %%rax\n\t"
        "and $-16, %%rax\n\t"

        // New allocation
        "lea (%%rax,%3), %%r11\n\t"

        // Bounds check
        "cmp %%r11, %%r9\n\t"
        "jb 1f\n\t"

        // Zero memory
        "mov %%rax, %%rdi\n\t"
        "add %%r10, %%rdi\n\t"
        "mov %3, %%rcx\n\t"
        "xor %%eax, %%eax\n\t"
        "shr $3, %%rcx\n\t"
        "rep stosq\n\t"

        // Update arena
        "mov %%r11, %c4(%2)\n\t"
        "mov %%rdi, %0\n\t"
        "jmp 2f\n\t"

        "1:\n\t"
        "xor %%rax, %%rax\n\t"

        "2:\n\t"
        : "=r"(ptr), "=m"(new_alloc)
        : "r"(arena), "r"(total_size), "i"(offsetof(LArena, allocated)), "i"(offsetof(LArena, size))
        : "rax", "rcx", "rdi", "r8", "r9", "r10", "r11", "memory");

    return ptr;
}

// Zero-fill allocation variant
ALWAYS_INLINE void* larena_calloc(LArena* arena, size_t count, size_t size) {
    return larena_calloc_asm(arena, count, size);
}

// Optimized string allocation with SIMD copying
char* larena_alloc_string(LArena* arena, const char* s) {
    if (UNLIKELY(!arena || !s))
        return NULL;

    // Fast path for empty string
    if (UNLIKELY(*s == '\0')) {
        char* ptr = larena_alloc(arena, 1);
        if (LIKELY(ptr))
            *ptr = '\0';
        return ptr;
    }

    // Use strlen that may use SIMD instructions
    size_t len = strlen(s);
    char* ptr  = larena_alloc(arena, len + 1);
    if (UNLIKELY(!ptr))
        return NULL;

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

    void* new_memory;
    if (posix_memalign(&new_memory, CACHE_LINE_SIZE, new_size)) {
        LOG_ERROR("posix_memalign failed: %s", strerror(errno));
        return false;
    }

    // Prefetch the new memory area
    PREFETCH_WRITE(new_memory);

    // Copy old content
    if (arena->allocated > 0) {
        memcpy(new_memory, arena->memory, arena->allocated);
    }

    free(arena->memory);

    arena->memory = new_memory;
    arena->size   = new_size;
    return true;
}

ALWAYS_INLINE size_t larena_getfree_memory(LArena* arena) {
    return arena->size - arena->allocated;
}

ALWAYS_INLINE void larena_reset(LArena* arena) {
    if (LIKELY(arena)) {
        arena->allocated = 0;
    }
}

ALWAYS_INLINE void larena_destroy(LArena* arena) {
    if (LIKELY(arena)) {
        free(arena->memory);
        free(arena);
    }
}
