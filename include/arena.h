/**
 * arena.h - High-performance arena allocator
 *
 * Pure bump-pointer allocator. Overflow blocks are co-located with their
 * ArenaBlock header (one allocation per block instead of two). Blocks double
 * in size on each expansion. Reset is O(1) and keeps committed pages alive.
 *
 * Two creation modes:
 *
 *   // Heap-allocated arena struct:
 *   Arena* a = arena_create(0);
 *   arena_destroy(a);
 *
 *   // Stack/static arena struct (zero heap overhead at init):
 *   char buf[64 * 1024];
 *   Arena a;
 *   arena_init(&a, buf, sizeof(buf));
 *   arena_destroy(&a);  // frees overflow blocks; buf is caller-owned
 */

#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef _MSC_VER
#include <intrin.h>
#define ARENA_INLINE         __forceinline
#define ARENA_PREFETCH(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#define ARENA_LIKELY(x)      (x)
#define ARENA_UNLIKELY(x)    (x)
#define ARENA_ALIGNED(n)     __declspec(align(n))
#else
#define ARENA_INLINE         inline __attribute__((always_inline))
#define ARENA_PREFETCH(addr) __builtin_prefetch((addr), 1, 3)
#define ARENA_LIKELY(x)      __builtin_expect(!!(x), 1)
#define ARENA_UNLIKELY(x)    __builtin_expect(!!(x), 0)
#define ARENA_ALIGNED(n)     __attribute__((aligned(n)))
#endif

/** Default alignment for arena_alloc(). Suitable for SSE/AVX and NEON. */
#define ARENA_DEFAULT_ALIGN 16

/** Minimum size for heap-backed overflow blocks. */
#define ARENA_MIN_BLOCK_SIZE (64 * 1024)

/**
 * Block descriptor. For overflow blocks this struct is stored at the start of
 * the heap allocation itself (co-located), so each new block costs one call to
 * aligned_alloc_xp instead of two. The usable region begins at block->base.
 *
 * The first block uses first_block inside Arena and points into a caller- or
 * TLS-provided buffer, so is_static is set and its memory is never freed.
 */
typedef struct ArenaBlock {
    struct ArenaBlock* next;
    char* base; /** Start of usable region (past header + alignment padding). */
    char* end;  /** One past the last byte of this block's buffer.            */
    bool is_static;
} ArenaBlock;

/**
 * Arena control structure. Aligned to 64 bytes to keep curr and end in the
 * same cache line as current_block. With the free list removed the struct is
 * ~72 bytes — one cache line for the hot fields, a second for the rest.
 */
typedef struct ARENA_ALIGNED(64) Arena {
    char* curr;                /** Next allocation address.                          */
    char* end;                 /** End of current block.                             */
    ArenaBlock* current_block; /** Block being allocated from.                       */
    ArenaBlock* head;          /** Head of the block chain.                          */
    size_t page_size;          /** OS page size (typically 4096).                    */
    size_t total_committed;
    bool heap_allocated;    /** True when Arena struct was allocated by arena_create(). */
    ArenaBlock first_block; /** Embedded header for the initial (static/stack) buffer.  */
} Arena;

/**
 * Initialises an Arena using caller-provided storage as the first block.
 *
 * The arena never frees `buf`. Overflow blocks are heap-allocated and freed
 * by arena_destroy(). No heap allocation, no TLS lookup — init is free.
 *
 * @param a    Caller-owned Arena struct (stack, static, or embedded).
 * @param buf  Backing buffer. Must outlive the arena.
 * @param size Size of `buf` in bytes.
 */
void arena_init(Arena* restrict a, void* restrict buf, size_t size);

/**
 * Allocates an Arena struct on the heap and initialises it.
 *
 * Uses a thread-local 1 MB static buffer as the first block when available
 * and large enough; otherwise heap-allocates the initial block.
 *
 * @param reserve_size Initial block size hint. 0 uses the default (64 KB, or the TLS buffer).
 * @return New arena, or NULL on allocation failure.
 */
Arena* arena_create(size_t reserve_size);

/**
 * Resets the arena so all committed memory can be reused. O(1).
 * All previously returned pointers become invalid.
 * Committed pages are retained to avoid subsequent page faults.
 */
static ARENA_INLINE void arena_reset(Arena* restrict a) {
    if (!a || !a->head) return;
    a->current_block = a->head;
    a->curr = a->head->base;
    a->end = a->head->end;
}

/**
 * Destroys the arena and frees all heap-allocated overflow blocks.
 * If the Arena struct was heap-allocated by arena_create(), it is freed too.
 * NULL is safely ignored.
 */
void arena_destroy(Arena* restrict arena);

/** Returns total bytes committed across all blocks (physical RAM in use). */
static ARENA_INLINE size_t arena_committed_size(const Arena* restrict a) { return a->total_committed; }

/**
 * Returns bytes consumed by user allocations in the current arena state.
 * Assumes all blocks before current_block are fully used, which holds for
 * any arena that has not been reset mid-chain.
 */
static ARENA_INLINE size_t arena_used_size(const Arena* restrict a) {
    if (!a || !a->current_block) return 0;
    size_t block_capacity = (size_t)(a->current_block->end - a->current_block->base);
    size_t block_used = (size_t)(a->curr - a->current_block->base);
    return (a->total_committed - block_capacity) + block_used;
}

/**
 * Internal slow path: allocates a new block when the current one is full.
 * Not for direct use.
 */
void* _arena_alloc_slow(Arena* restrict arena, size_t size, size_t alignment);

/**
 * Allocates `size` bytes aligned to `alignment` (must be a power of two).
 *
 * @return Aligned pointer valid until the next arena_reset() or
 *         arena_destroy(), or NULL on failure.
 */
static ARENA_INLINE void* arena_alloc_align(Arena* restrict arena, size_t size, size_t alignment) {
    if (size == 0) return NULL;

    uintptr_t aligned = ((uintptr_t)arena->curr + alignment - 1) & ~(alignment - 1);
    uintptr_t next = aligned + size;

    if (ARENA_LIKELY(next <= (uintptr_t)arena->end)) {
        arena->curr = (char*)next;
        return (void*)aligned;
    }

    return _arena_alloc_slow(arena, size, alignment);
}

/**
 * Allocates `size` bytes with ARENA_DEFAULT_ALIGN (16-byte) alignment.
 * This is the primary allocation function (~2-3 ns on the fast path).
 */
static ARENA_INLINE void* arena_alloc(Arena* restrict arena, size_t size) {
    return arena_alloc_align(arena, size, ARENA_DEFAULT_ALIGN);
}

/**
 * Allocates `size` bytes with no alignment padding (1-byte aligned).
 * Use for byte buffers, strings, or data with no alignment requirement.
 * Slightly faster than arena_alloc() as it skips the alignment arithmetic.
 */
static ARENA_INLINE void* arena_alloc_unaligned(Arena* restrict arena, size_t size) {
    if (size == 0) return NULL;

    char* ptr = arena->curr;
    char* next = ptr + size;

    if (ARENA_LIKELY(next <= arena->end)) {
        arena->curr = next;
        return ptr;
    }

    return _arena_alloc_slow(arena, size, 1);
}

/** Allocates a single object with its natural alignment. */
#define ARENA_ALLOC(arena, type) ((type*)arena_alloc_align((arena), sizeof(type), _Alignof(type)))

/** Allocates an array of `count` objects with the type's natural alignment. */
#define ARENA_ALLOC_ARRAY(arena, type, count) \
    ((type*)arena_alloc_align((arena), sizeof(type) * (count), _Alignof(type)))

/** Allocates a zero-initialized object. Returns NULL on failure. */
#define ARENA_ALLOC_ZERO(arena, type)                         \
    ({                                                        \
        type* _ptr = ARENA_ALLOC((arena), type);              \
        (_ptr) ? (type*)memset(_ptr, 0, sizeof(type)) : NULL; \
    })

/** Allocates a zero-initialized array. Returns NULL on failure. */
#define ARENA_ALLOC_ARRAY_ZERO(arena, type, count)                      \
    ({                                                                  \
        type* _ptr = ARENA_ALLOC_ARRAY((arena), type, (count));         \
        (_ptr) ? (type*)memset(_ptr, 0, sizeof(type) * (count)) : NULL; \
    })

/**
 * Allocates `count` blocks in a single contiguous region, each aligned to
 * ARENA_DEFAULT_ALIGN. Requires only one bounds check and at most one block
 * transition. All-or-nothing: on failure, no pointers are written.
 *
 * @param sizes    Array of per-block sizes (length `count`).
 * @param out_ptrs Array to receive the resulting pointers (length `count`).
 * @return true on success, false on failure or invalid arguments.
 */
static ARENA_INLINE bool arena_alloc_batch(Arena* restrict arena, const size_t* restrict sizes, size_t count,
                                           void** restrict out_ptrs) {
    if (!arena || !sizes || !out_ptrs || count == 0) return false;

    const size_t mask = ARENA_DEFAULT_ALIGN - 1;
    size_t total = 0;
    for (size_t i = 0; i < count; ++i) {
        total = (total + mask) & ~mask;
        total += sizes[i];
    }

    char* base = (char*)arena_alloc(arena, total);
    if (!base) return false;

    char* cur = base;
    for (size_t i = 0; i < count; ++i) {
        cur = (char*)(((uintptr_t)cur + mask) & ~mask);
        out_ptrs[i] = cur;
        cur += sizes[i];
    }

    return true;
}

/**
 * Duplicates a null-terminated string into the arena.
 * Returns NULL if either argument is NULL or allocation fails.
 */
static ARENA_INLINE char* arena_strdup(Arena* restrict arena, const char* restrict str) {
    if (!arena || !str) return NULL;
    size_t len = strlen(str);
    char* dup = (char*)arena_alloc_unaligned(arena, len + 1);
    if (!dup) return NULL;
    memcpy(dup, str, len + 1);
    return dup;
}

/**
 * Copies exactly `length` bytes from `str` and appends a null terminator.
 * Does not require `str` to be null-terminated.
 * Returns NULL if either pointer argument is NULL or allocation fails.
 */
static ARENA_INLINE char* arena_strdupn(Arena* restrict arena, const char* restrict str, size_t length) {
    if (!arena || !str) return NULL;
    char* dup = (char*)arena_alloc_unaligned(arena, length + 1);
    if (!dup) return NULL;
    memcpy(dup, str, length);
    dup[length] = '\0';
    return dup;
}

#if defined(__cplusplus)
}
#endif

#endif  // ARENA_H
