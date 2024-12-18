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

    // Allocate the first chunk
    arena->head = (Chunk*)system_alloc(chunk_size);
    if (arena->head == NULL) {
        free(arena);
        return NULL;
    }

    arena->head->base = (char*)arena->head + sizeof(Chunk);
    arena->head->size = chunk_size - sizeof(Chunk);
    arena->head->used = 0;
    arena->head->next = NULL;

    // Initialize the spinlock
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

    size = align_up(size, ARENA_ALIGNMENT);

    spinlock_acquire(&arena->spin_lock);

    // Fast path: try to allocate from the current chunk
    Chunk* chunk = arena->head;
    if (__builtin_expect(chunk->used + size <= chunk->size, 1)) {
        void* ptr = (char*)chunk->base + chunk->used;
        chunk->used += size;
        spinlock_release(&arena->spin_lock);
        return ptr;
    }

    // Slow path: allocate a new chunk
    size_t new_chunk_size =
        arena->chunk_size > size + sizeof(Chunk) ? arena->chunk_size : size + sizeof(Chunk);

    Chunk* new_chunk = (Chunk*)system_alloc(new_chunk_size);
    if (__builtin_expect(new_chunk == NULL, 0)) {
        spinlock_release(&arena->spin_lock);
        return NULL;
    }

    new_chunk->base = (char*)new_chunk + sizeof(Chunk);
    new_chunk->size = new_chunk_size - sizeof(Chunk);
    new_chunk->used = size;
    new_chunk->next = arena->head;
    arena->head = new_chunk;

    spinlock_release(&arena->spin_lock);
    return new_chunk->base;
}

void* arena_realloc(Arena* arena, void* ptr, size_t size) {
    if (__builtin_expect(arena == NULL || ptr == NULL || size == 0, 0)) {
        return NULL;
    }

    spinlock_acquire(&arena->spin_lock);

    // Find the chunk containing the pointer
    Chunk* chunk = arena->head;
    while (chunk != NULL) {
        if (ptr >= chunk->base && (char*)ptr < (char*)chunk->base + chunk->used) {
            break;
        }
        chunk = chunk->next;
    }

    if (__builtin_expect(chunk == NULL, 0)) {
        // Pointer not found in the arena
        spinlock_release(&arena->spin_lock);
        return NULL;
    }

    // Lets find the size of the previous allocation(for ptr)
    size_t old_size = 0;
    if (ptr == chunk->base) {
        // ptr is at the beginning of the chunk
        old_size = chunk->used;
    } else {
        // Iterate through free space to find the previous allocation
        char* current = (char*)chunk->base;
        while (current < (char*)ptr) {
            // Check if current points to the start of an allocation
            if (current == chunk->base || *(size_t*)current != 0) {
                old_size = (char*)ptr - current;
                break;  // Found the previous allocation
            }
            // Move to the next potential allocation start
            current += align_up(*(size_t*)current, ARENA_ALIGNMENT);
        }
    }

    // Align the new size
    size = align_up(size, ARENA_ALIGNMENT);

    // There is enough space in the current chunk
    if (old_size + size <= chunk->size) {
        chunk->used = old_size + size;
        spinlock_release(&arena->spin_lock);
        return ptr;
    }

    // unlock the mutex as arena_alloc will lock it again
    // Otherwise, it will cause a deadlock
    spinlock_release(&arena->spin_lock);

    // Allocate a new block and copy the data
    void* new_ptr = arena_alloc(arena, size);
    if (new_ptr == NULL) {
        return NULL;
    }

    // if old_size > size, copy only the allocated memory
    size_t copy_size = old_size < size ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);

    return new_ptr;
}

char* arena_alloc_string(Arena* arena, const char* str) {
    if (str == NULL) {
        return NULL;
    }

    size_t len = strlen(str) + 1;
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
