#include "arena.h"
#include "aligned_alloc.h"
#include "macros.h"

#include <stdlib.h> /* abort    */
#include <string.h> /* memset   */

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h> /* (reserved for future mmap path) */
#include <unistd.h>   /* sysconf */
#endif

/* -------------------------------------------------------------------------
 * Thread-local static backing buffer
 *
 * arena_create() uses this buffer as the first block when it is large enough
 * and not already claimed.  One buffer per thread means no contention and no
 * heap allocation on the happy path.
 * ---------------------------------------------------------------------- */

#define STATIC_BUFFER_SIZE (1024 * 1024) /* 1 MB per thread */

static alignas(64) THREAD_LOCAL char static_buffer[STATIC_BUFFER_SIZE];
static THREAD_LOCAL bool static_buffer_in_use = false;

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

static ARENA_INLINE size_t get_page_size(void) {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
#else
    return (size_t)sysconf(_SC_PAGESIZE);
#endif
}

/* -------------------------------------------------------------------------
 * Public API — lifecycle
 * ---------------------------------------------------------------------- */

void arena_init(Arena* a, void* buf, size_t size) {
    memset(a, 0, sizeof(Arena));
    a->page_size = get_page_size();
    a->heap_allocated = false;
    a->tls_in_use_origin = NULL;

    a->first_block.base = (char*)buf;
    a->first_block.end = (char*)buf + size;
    a->first_block.is_static = true;
    a->first_block.next = NULL;

    a->head = &a->first_block;
    a->current_block = a->head;
    a->curr = a->head->base;
    a->end = a->head->end;
    a->total_committed = size;
}

Arena* arena_create(size_t reserve_size) {
    Arena* a = (Arena*)aligned_alloc_xp(64, sizeof(Arena));
#ifdef ARENA_ABORT_ON_OOM
    if (ARENA_UNLIKELY(!a)) ARENA_OOM_HANDLER(sizeof(Arena));
#endif
    if (!a) return NULL;

    if (!static_buffer_in_use && reserve_size <= STATIC_BUFFER_SIZE) {
        arena_init(a, static_buffer, STATIC_BUFFER_SIZE);
        static_buffer_in_use = true;

        // Avoid race condition if the arena is handed off to another thread
        // after creation since we are storing this threads address.
        a->tls_in_use_origin = &static_buffer_in_use;
    } else {
        size_t page_size = get_page_size();
        size_t initial_size = reserve_size > 0 ? reserve_size : ARENA_MIN_BLOCK_SIZE;
        /* Round up to page boundary. */
        initial_size = (initial_size + page_size - 1) & ~(page_size - 1);

        char* buf = (char*)aligned_alloc_xp(64, initial_size);
        if (!buf) {
            aligned_free_xp(a);
#ifdef ARENA_ABORT_ON_OOM
            ARENA_OOM_HANDLER(initial_size);
#endif
            return NULL;
        }
        arena_init(a, buf, initial_size);
        /* Mark the first block as heap-owned so arena_destroy frees it. */
        a->first_block.is_static = false;
    }

    a->heap_allocated = true;
    return a;
}

void arena_destroy(Arena* a) {
    if (!a) return;

    ArenaBlock* block = a->head;

    /* Free the first block's buffer only if we heap-allocated it.
     * Static/stack/TLS-backed buffers are caller-owned and must not be freed. */
    if (block && !block->is_static) { aligned_free_xp(block->base); }

    /* If the first block was the TLS static buffer, mark it available again
     * so the next arena_create() can reuse it without leaking capacity. 
     Comparing block->base == static_buffer is a bug if the arena switched threads.
     */
    if (block && block->is_static && a->tls_in_use_origin) { *a->tls_in_use_origin = false; }

    /* Walk overflow blocks.  Each was allocated as a single slab where the
     * ArenaBlock header lives at the start of the pointer returned by
     * aligned_alloc_xp, so we free the block pointer itself (not block->base). */
    block = block ? block->next : NULL;
    while (block) {
        ArenaBlock* next = block->next;
        aligned_free_xp(block); /* frees the whole slab (header + data) */
        block = next;
    }

    if (a->heap_allocated) aligned_free_xp(a);
}

/* -------------------------------------------------------------------------
 * Slow path — new block allocation
 * ---------------------------------------------------------------------- */

void* _arena_alloc_slow(Arena* a, size_t size, size_t alignment) {
    /* --- Try existing cached blocks after current_block first ------------ */

    ArenaBlock* next = a->current_block->next;
    while (next) {
        uintptr_t aligned = ((uintptr_t)next->base + alignment - 1) & ~(uintptr_t)(alignment - 1);
        if (aligned + size <= (uintptr_t)next->end) {
            /* Found a cached block with enough room. */
            a->current_block = next;
            a->curr = (char*)(aligned + size);
            a->end = next->end;
            return (void*)aligned;
        }
        next = next->next;
    }

    /* --- Allocate a fresh block ----------------------------------------- */

    /* The new block slab must hold: ArenaBlock header + worst-case alignment
     * padding + the requested allocation. */
    size_t current_capacity = (size_t)(a->current_block->end - a->current_block->base);
    size_t needed = sizeof(ArenaBlock) + (alignment - 1) + size;

    // Check for overflow.
    size_t next_size = (current_capacity <= ARENA_MAX_BLOCK_SIZE / 2) ? current_capacity * 2 : ARENA_MAX_BLOCK_SIZE;
    if (next_size < needed) next_size = needed;
    if (next_size < ARENA_MIN_BLOCK_SIZE) next_size = ARENA_MIN_BLOCK_SIZE;

    /* Round up to page boundary so mmap-based backends work efficiently. */
    next_size = (next_size + a->page_size - 1) & ~(a->page_size - 1);

    /* Single allocation: header at [ptr], usable memory starts immediately
     * after the header (with any required padding for alignment). */
    char* ptr = (char*)aligned_alloc_xp(64, next_size);
    if (!ptr) return NULL;

    ArenaBlock* block = (ArenaBlock*)ptr;

    /* base starts right after the header, already accounting for the 64-byte
     * slab alignment from aligned_alloc_xp.  If the requested alignment is
     * <= 64 (the common case) block->base is already correctly aligned — the
     * extra alignment arithmetic below reduces to a no-op. */
    uintptr_t base_addr = (uintptr_t)(ptr + sizeof(ArenaBlock));
    uintptr_t aligned = (base_addr + alignment - 1) & ~(uintptr_t)(alignment - 1);

    block->base = (char*)base_addr; /* record the true start for capacity math */
    block->end = ptr + next_size;
    block->is_static = false;
    /* Insert at the tail of the chain so skipped cached blocks are revisited
     * on future slow-path calls instead of being orphaned mid-chain. */
    block->next = NULL;
    a->current_block->next = block;

    a->current_block = block;
    a->total_committed += next_size;
    a->curr = (char*)(aligned + size);
    a->end = block->end;

    return (void*)aligned;
}
