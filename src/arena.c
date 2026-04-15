#include "arena.h"
#include "aligned_alloc.h"
#include "macros.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#define STATIC_BUFFER_SIZE (1024 * 1024)
static THREAD_LOCAL char static_buffer[STATIC_BUFFER_SIZE] ARENA_ALIGNED(64);
static THREAD_LOCAL bool static_buffer_in_use = false;

static ARENA_INLINE size_t get_page_size(void) {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return (size_t)sysconf(_SC_PAGESIZE);
#endif
}

void arena_init(Arena* restrict a, void* restrict buf, size_t size) {
    memset(a, 0, sizeof(Arena));
    a->page_size = get_page_size();
    a->heap_allocated = false;

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
    if (!a) return NULL;

    if (!static_buffer_in_use && reserve_size <= STATIC_BUFFER_SIZE) {
        arena_init(a, static_buffer, STATIC_BUFFER_SIZE);
        static_buffer_in_use = true;
    } else {
        size_t initial_size = reserve_size > 0 ? reserve_size : ARENA_MIN_BLOCK_SIZE;
        initial_size = (initial_size + get_page_size() - 1) & ~(get_page_size() - 1);

        char* buf = (char*)aligned_alloc_xp(64, initial_size);
        if (!buf) {
            aligned_free_xp(a);
            return NULL;
        }
        arena_init(a, buf, initial_size);
        a->first_block.is_static = false;  // We own this buffer; arena_destroy must free it.
    }

    a->heap_allocated = true;
    return a;
}

void* _arena_alloc_slow(Arena* restrict a, size_t size, size_t alignment) {
    // Try the next cached block before allocating a new one.
    ArenaBlock* next = a->current_block->next;
    if (next) {
        uintptr_t aligned = ((uintptr_t)next->base + alignment - 1) & ~(alignment - 1);
        if (aligned + size <= (uintptr_t)next->end) {
            a->current_block = next;
            a->curr = (char*)(aligned + size);
            a->end = next->end;
            return (void*)aligned;
        }
    }

    // Allocate a new block. The ArenaBlock header lives at the start of the
    // allocation itself, eliminating the separate malloc() call.
    size_t current_size = (size_t)(a->current_block->end - a->current_block->base);
    size_t needed = sizeof(ArenaBlock) + alignment + size;
    size_t next_size = current_size * 2;

    if (next_size < needed) next_size = needed;
    if (next_size < ARENA_MIN_BLOCK_SIZE) next_size = ARENA_MIN_BLOCK_SIZE;
    next_size = (next_size + a->page_size - 1) & ~(a->page_size - 1);

    // Single allocation: header at [ptr], usable memory at [ptr + sizeof(ArenaBlock)].
    char* ptr = (char*)aligned_alloc_xp(64, next_size);
    if (!ptr) return NULL;

    ArenaBlock* block = (ArenaBlock*)ptr;
    block->base = (char*)(((uintptr_t)(ptr + sizeof(ArenaBlock)) + alignment - 1) & ~(alignment - 1));
    block->end = ptr + next_size;
    block->is_static = false;
    block->next = a->current_block->next;

    a->current_block->next = block;
    a->current_block = block;
    a->total_committed += next_size;

    uintptr_t aligned = ((uintptr_t)block->base + alignment - 1) & ~(alignment - 1);
    a->curr = (char*)(aligned + size);
    a->end = block->end;

    return (void*)aligned;
}

void arena_destroy(Arena* restrict a) {
    if (!a) return;

    ArenaBlock* block = a->head;

    // Free the first block's buffer if we heap-allocated it.
    if (block && !block->is_static) aligned_free_xp(block->base);

    // Walk overflow blocks. Each was allocated as a single slab where the
    // ArenaBlock header lives at the start of the pointer returned by
    // aligned_alloc_xp, so we free the block pointer itself (not block->base).
    block = block ? block->next : NULL;
    while (block) {
        ArenaBlock* temp = block;
        block = block->next;
        aligned_free_xp(temp);  // frees the whole slab (header + data)
    }

    if (a->heap_allocated) aligned_free_xp(a);
}
