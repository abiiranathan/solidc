#include "../include/arena.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <unistd.h>
#endif

typedef struct Chunk {
    void* base;          // Pointer to the base of the chunk
    size_t size;         // Size of the chunk
    size_t used;         // Amount of memory used in the chunk
    struct Chunk* next;  // Pointer to the next chunk
} Chunk;

typedef struct Arena {
    Chunk* head;            // Pointer to the head of the chunk
    size_t chunk_size;      // Size of the chunk
    atomic_flag spin_lock;  // Spinlock for thread safety
} Arena;

typedef struct AllocHeader {
    size_t size;  // Size of the allocation (excluding header)
} AllocHeader;

#ifdef _WIN32
static void* system_alloc(size_t size) {
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

static void system_free(void* ptr, size_t size) {
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
}
#else
#include <sys/mman.h>

static inline void* system_alloc(size_t size) {
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    return ptr;
}

static inline void system_free(void* ptr, size_t size) {
    if (munmap(ptr, size) == -1) {
        perror("munmap");
    }
}
#endif

static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Fast spinlock implementation.
static inline void spinlock_acquire(atomic_flag* lock) {
    while (atomic_flag_test_and_set(lock)) {
// yield the CPU to the OS to avoid busy waiting
#ifdef _WIN32
        Sleep(0);
#else
        sched_yield();
#endif
    }
}

static inline void spinlock_release(atomic_flag* lock) {
    atomic_flag_clear(lock);
}

Arena* arena_create(size_t chunk_size) {
    if (chunk_size == 0) {
        chunk_size = ARENA_DEFAULT_CHUNKSIZE;
    }

    Arena* arena = (Arena*)malloc(sizeof(Arena));
    if (arena == NULL) {
        return NULL;
    }

    arena->chunk_size = chunk_size;
    arena->head       = (Chunk*)system_alloc(chunk_size);
    if (arena->head == NULL) {
        free(arena);
        return NULL;
    }

    arena->head->base = (char*)arena->head + sizeof(Chunk);
    arena->head->size = chunk_size - sizeof(Chunk);
    arena->head->used = 0;
    arena->head->next = NULL;

    atomic_flag_clear(&arena->spin_lock);
    return arena;
}

void arena_destroy(Arena* arena) {
    if (arena == NULL) {
        return;
    }

    Chunk* chunk = arena->head;
    while (chunk != NULL) {
        Chunk* next = chunk->next;
        system_free(chunk, chunk->size + sizeof(Chunk));
        chunk = next;
    }

    free(arena);
    arena = NULL;
}

void arena_reset(Arena* arena) {
    if (arena == NULL) {
        return;
    }

    spinlock_acquire(&arena->spin_lock);

    Chunk* chunk = arena->head->next;
    while (chunk != NULL) {
        Chunk* next = chunk->next;
        system_free(chunk, chunk->size + sizeof(Chunk));
        chunk = next;
    }

    arena->head->next = NULL;
    arena->head->used = 0;

    spinlock_release(&arena->spin_lock);
}

void* arena_alloc(Arena* arena, size_t size) {
    if (__builtin_expect(arena == NULL || size == 0, 0)) {
        return NULL;
    }

    // Include space for the header and align the total size
    size_t header_size = align_up(sizeof(AllocHeader), ARENA_ALIGNMENT);
    size_t total_size  = header_size + align_up(size, ARENA_ALIGNMENT);

    spinlock_acquire(&arena->spin_lock);

    Chunk* chunk = arena->head;
    if (__builtin_expect(chunk->used + total_size <= chunk->size, 1)) {
        // Fast path: allocate from current chunk
        void* ptr           = (char*)chunk->base + chunk->used;
        AllocHeader* header = (AllocHeader*)ptr;
        header->size        = size;  // Store the requested size
        void* user_ptr      = (char*)ptr + header_size;
        chunk->used += total_size;
        spinlock_release(&arena->spin_lock);
        return user_ptr;
    }

    // Slow path: allocate a new chunk
    size_t new_chunk_size = arena->chunk_size > total_size ? arena->chunk_size : total_size;
    Chunk* new_chunk      = (Chunk*)system_alloc(new_chunk_size);
    if (__builtin_expect(new_chunk == NULL, 0)) {
        spinlock_release(&arena->spin_lock);
        return NULL;
    }

    new_chunk->base     = (char*)new_chunk + sizeof(Chunk);
    new_chunk->size     = new_chunk_size - sizeof(Chunk);
    new_chunk->used     = total_size;
    new_chunk->next     = arena->head;
    AllocHeader* header = (AllocHeader*)new_chunk->base;
    header->size        = size;
    void* user_ptr      = (char*)new_chunk->base + header_size;
    arena->head         = new_chunk;

    spinlock_release(&arena->spin_lock);
    return user_ptr;
}

void* arena_realloc(Arena* arena, void* ptr, size_t size) {
    if (arena == NULL || size == 0) {
        return NULL;
    }

    if (ptr == NULL) {
        return arena_alloc(arena, size);
    }

    size = align_up(size, ARENA_ALIGNMENT);

    // Calculate the header position
    size_t header_size  = align_up(sizeof(AllocHeader), ARENA_ALIGNMENT);
    AllocHeader* header = (AllocHeader*)((char*)ptr - header_size);
    size_t old_size     = header->size;

    spinlock_acquire(&arena->spin_lock);

    // Find the chunk containing ptr
    Chunk* chunk = arena->head;
    while (chunk != NULL) {
        if (ptr >= chunk->base && (char*)ptr <= (char*)chunk->base + chunk->used) {
            break;
        }
        chunk = chunk->next;
    }

    if (chunk == NULL) {
        spinlock_release(&arena->spin_lock);
        return NULL;  // Invalid pointer
    }

    // Check if ptr is the last allocation and can be resized in place
    char* alloc_end = (char*)ptr + align_up(old_size, ARENA_ALIGNMENT);
    if (alloc_end == (char*)chunk->base + chunk->used) {
        size_t new_total_size = header_size + size;
        size_t old_total_size = header_size + align_up(old_size, ARENA_ALIGNMENT);
        if (new_total_size <= chunk->size) {
            if (new_total_size <= old_total_size) {
                // Shrinking in place
                header->size = size;
                chunk->used  = ((char*)ptr - (char*)chunk->base) + size;

                // Ensure null termination if shrinking past original data
                if (size < old_size) {
                    char* new_end = (char*)ptr + size - 1;
                    if (size > 1 && *new_end != '\0')
                        *new_end = '\0';
                }
                spinlock_release(&arena->spin_lock);
                return ptr;
            } else if (chunk->used + (new_total_size - old_total_size) <= chunk->size) {
                // Enlarging in place
                header->size = size;
                chunk->used += (new_total_size - old_total_size);
                spinlock_release(&arena->spin_lock);
                return ptr;
            }
        }
    }

    // Allocate a new block and copy data
    spinlock_release(&arena->spin_lock);
    void* new_ptr = arena_alloc(arena, size);
    if (new_ptr == NULL) {
        return NULL;
    }

    // Copy the minimum of old_size and size (standard realloc behavior)
    size_t copy_size = old_size < size ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);

    return new_ptr;
}

char* arena_alloc_string(Arena* arena, const char* str) {
    if (str == NULL) {
        return NULL;
    }

    size_t len    = strlen(str) + 1;
    char* new_str = (char*)arena_alloc(arena, len);
    if (new_str == NULL) {
        return NULL;
    }
    memcpy(new_str, str, len);
    return new_str;
}

int* arena_alloc_int(Arena* arena, int n) {
    int* ptr = (int*)arena_alloc(arena, sizeof(int));
    if (ptr == NULL) {
        return NULL;
    }
    *ptr = n;
    return ptr;
}
