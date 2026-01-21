#include "arena.h"

#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

Arena* arena_create(size_t reserve_size) {
    // If reserve_size is zero, use default size of 1GB Virtual address space.
    if (reserve_size == 0) {
        reserve_size = ARENA_DEFAULT_SIZE;
    }

    // Allocate the arena metadata header
    Arena* a = (Arena*)malloc(sizeof(Arena));
    if (!a) return NULL;

    // Reserve Virtual Address Space
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    a->page_size = si.dwPageSize;
    // Reserve address space (no RAM used)
    a->base = (char*)VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_NOACCESS);
#else
    a->page_size = (size_t)sysconf(_SC_PAGESIZE);
    // Reserve address space (no RAM used)
    a->base = (char*)mmap(NULL, reserve_size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (a->base == MAP_FAILED) a->base = NULL;
#endif

    if (!a->base) {
        free(a);
        return NULL;
    }

    a->curr = a->base;
    a->end = a->base;  // Will be updated after commit
    a->reserve_end = a->base + reserve_size;

    // Pre-commit the first page to eliminate initial allocation latency
    // This ensures the first few allocations are instant with no page faults
#if defined(_WIN32)
    void* committed = VirtualAlloc(a->base, a->page_size, MEM_COMMIT, PAGE_READWRITE);
    if (!committed) {
        VirtualFree(a->base, 0, MEM_RELEASE);
        free(a);
        return NULL;
    }
#else
    if (mprotect(a->base, a->page_size, PROT_READ | PROT_WRITE) != 0) {
        munmap(a->base, reserve_size);
        free(a);
        return NULL;
    }
#endif

    // Update end pointer to reflect committed memory
    a->end = a->base + a->page_size;
    return a;
}

void* _arena_alloc_slow(Arena* a, size_t size, size_t align) {
    uintptr_t curr = (uintptr_t)a->curr;
    uintptr_t mask = (uintptr_t)align - 1;
    uintptr_t aligned_curr = (curr + mask) & ~mask;
    uintptr_t needed = aligned_curr + size;

    if (needed > (uintptr_t)a->reserve_end) {
        return NULL;  // Absolute OOM
    }

    // Calculate how many more pages we need to commit
    // We commit in chunks of at least 64KB to reduce syscall frequency
    size_t chunk_size = 64 * 1024;
    size_t to_commit = needed - (uintptr_t)a->end;
    if (to_commit < chunk_size) to_commit = chunk_size;

    // Round to page boundary
    to_commit = (to_commit + a->page_size - 1) & ~(a->page_size - 1);

    // Safety check for reserve boundary
    if ((char*)a->end + to_commit > a->reserve_end) {
        to_commit = (size_t)(a->reserve_end - a->end);
    }

    // Commit the memory.
#if defined(_WIN32)
    if (!VirtualAlloc(a->end, to_commit, MEM_COMMIT, PAGE_READWRITE)) return NULL;
#else
    if (mprotect(a->end, to_commit, PROT_READ | PROT_WRITE) != 0) return NULL;
#endif

    a->end += to_commit;

    a->curr = (char*)needed;
    return (void*)aligned_curr;
}

void arena_destroy(Arena* a) {
    if (!a) return;
#if defined(_WIN32)
    VirtualFree(a->base, 0, MEM_RELEASE);
#else
    munmap(a->base, (size_t)(a->reserve_end - a->base));
#endif
    free(a);
}
