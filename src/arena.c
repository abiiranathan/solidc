
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/arena.h"
#include "../include/lock.h"

typedef struct Chunk {
    void* base;          // Pointer to the base of the chunk
    size_t size;         // Size of the chunk
    size_t used;         // Amount of memory used in the chunk
    struct Chunk* next;  // Pointer to the next chunk
} Chunk;

typedef struct Arena {
    Chunk* head;        // Pointer to the head of the chunk
    size_t chunk_size;  // Size of the chunk
    size_t alignment;   // Alignment of the chunk

    // use a mutex for thread safety to avoid the contention associated with spinlocks
    Lock* lock;
} Arena;

#ifdef _WIN32
void* alloc_chunk(size_t size) {
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void free_chunk(void* base, size_t size) {
    VirtualFree(base, 0, MEM_RELEASE);
}
#else
void* alloc_chunk(size_t size) {
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
}

void free_chunk(void* base, size_t size) {
    munmap(base, size);
}
#endif

static void* align_pointer(void* ptr, size_t alignment) {
    uintptr_t p = (uintptr_t)ptr;
    size_t offset = (alignment - (p % alignment)) % alignment;
    return (void*)(p + offset);
}

Arena* arena_create(size_t chunk_size, size_t alignment) {
    // Make sure the alignment is a power of 2
    if ((alignment & (alignment - 1)) != 0) {
        fprintf(stderr, "arena_create(): Alignment must be a power of 2\n");
        exit(1);
    }

    if (alignment == 0) {
        alignment = ARENA_DEFAULT_ALIGNMENT;
    }

    Arena* arena = (Arena*)malloc(sizeof(Arena));
    if (!arena) {
        return NULL;
    }

    arena->chunk_size = chunk_size;
    arena->alignment = alignment;
    arena->head = NULL;

    // Initialize the lock
    arena->lock = (Lock*)malloc(sizeof(Lock));
    if (!arena->lock) {
        free(arena);
        return NULL;
    }

    lock_init(arena->lock);
    return arena;
}

void* arena_alloc(Arena* arena, size_t size) {
    lock_acquire(arena->lock);
    size = (size + arena->alignment - 1) & ~(arena->alignment - 1);  // Align size
    if (!arena->head || arena->head->used + size > arena->head->size) {
        // Allocate a new chunk
        Chunk* chunk = (Chunk*)malloc(sizeof(Chunk));
        if (!chunk) {
            lock_release(arena->lock);
            return NULL;
        }

        chunk->size = arena->chunk_size > size ? arena->chunk_size : size;
        chunk->used = 0;
        chunk->base = alloc_chunk(chunk->size);
        if (!chunk->base) {
            free(chunk);
            lock_release(arena->lock);
            return NULL;
        }
        chunk->next = arena->head;
        arena->head = chunk;
    }

    // Allocate memory from the current chunk
    void* ptr = (char*)arena->head->base + arena->head->used;
    ptr = align_pointer(ptr, arena->alignment);  // Align the pointer
    arena->head->used += size;
    lock_release(arena->lock);
    return ptr;
}

void* arena_realloc(Arena* arena, void* ptr, size_t size) {
    if (!ptr) {
        return arena_alloc(arena, size);
    }

    lock_acquire(arena->lock);
    Chunk* chunk = arena->head;
    while (chunk) {
        // Check if the pointer is within the chunk
        if (ptr >= chunk->base && (char*)ptr < (char*)chunk->base + chunk->size) {
            size = (size + arena->alignment - 1) & ~(arena->alignment - 1);  // Align size
            if (size <= chunk->size) {
                lock_release(arena->lock);
                return ptr;  // No need to reallocate
            }

            // Release the lock before allocating a new chunk
            // because arena_alloc acquires the lock. This avoids
            // deadlock.
            lock_release(arena->lock);

            void* new_ptr = arena_alloc(arena, size);
            if (!new_ptr) {
                return NULL;
            }
            memcpy(new_ptr, ptr, chunk->used);
            return new_ptr;
        }
        chunk = chunk->next;
    }

    lock_release(arena->lock);
    return NULL;
}

// Reset the arena and merge the chunks into one block to avoid fragmentation
// such that arena.used = 0 and arena.head->size = total size of all chunks.
void arena_reset(Arena* arena) {
    lock_acquire(arena->lock);
    Chunk* chunk = arena->head;
    size_t total_size = 0;  // excludes the head

    while (chunk) {
        chunk->used = 0;
        if (chunk != arena->head) {
            free_chunk(chunk->base, chunk->size);

            // Merge the blocks
            Chunk* next = chunk->next;
            if (next && (char*)chunk->base + chunk->size == next->base) {
                chunk->size += next->size;
                chunk->next = next->next;

                // Free the next chunk
                free_chunk(next->base, next->size);
            }

            total_size += chunk->size;
        }
        chunk = chunk->next;
    }

    // Reset the head. Before any allocations, the head is NULL.
    if (arena->head) {
        arena->head->used = 0;
        arena->head->size += total_size;
        arena->head->next = NULL;
    }
    lock_release(arena->lock);
}

void arena_destroy(Arena* arena) {
    Chunk* chunk = arena->head;
    while (chunk) {
        Chunk* next = chunk->next;
        free_chunk(chunk->base, chunk->size);
        free(chunk);
        chunk = next;
    }

    lock_free(arena->lock);
    free(arena->lock);
    free(arena);
}

// Allocate a string from the arena. The string is copied into the arena memory.
// Returns NULL on failure.
char* arena_alloc_string(Arena* arena, const char* str) {
    char* new_ptr = arena_alloc(arena, strlen(str) + 1);
    if (!new_ptr) {
        return NULL;
    }

    strcpy(new_ptr, str);
    return new_ptr;
}

// Allocate and assign an integer.
int* arena_alloc_int(Arena* arena, int n) {
    int* new_ptr = arena_alloc(arena, sizeof(n));
    if (!new_ptr) {
        return NULL;
    }

    *new_ptr = n;
    return new_ptr;
}

void arena_free(Arena* arena, void* ptr) {
    (void)arena;
    (void)ptr;
}
