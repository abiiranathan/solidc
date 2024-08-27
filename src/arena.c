#include "../include/arena.h"
#include "../include/lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    Lock lock;          // Lock for thread safety
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
static void* system_alloc(size_t size) {
    // On Linux we use mmap with MAP_ANONYMOUS to get memory directly from the kernel.
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    return ptr;
}

static void system_free(void* ptr, size_t size) {
    int ret = munmap(ptr, size);
    if (ret == -1) {
        perror("munmap");
    }
}
#endif

static size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

Arena* arena_create(size_t chunk_size, size_t alignment) {
    if (alignment == 0) {
        alignment = ARENA_DEFAULT_ALIGNMENT;
    } else if ((alignment & (alignment - 1)) != 0) {
        // Check if alignment is a power of 2
        fprintf(stderr, "arena_create: Alignment must be a power of 2\n");
        exit(1);
    }

    if (chunk_size == 0) {
        chunk_size = ARENA_DEFAULT_CHUNKSIZE;
    }

    Arena* arena = (Arena*)malloc(sizeof(Arena));
    if (arena == NULL) {
        return NULL;
    }

    arena->chunk_size = chunk_size;
    arena->alignment = alignment;

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

    // Initialize the mutex
    lock_init(&arena->lock);

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

    // Destroy the mutex
    lock_free(&arena->lock);
    free(arena);
}

void arena_reset(Arena* arena) {
    if (arena == NULL) {
        return;
    }

    lock_acquire(&arena->lock);

    Chunk* chunk = arena->head->next;
    while (chunk != NULL) {
        Chunk* next = chunk->next;
        system_free(chunk, chunk->size + sizeof(Chunk));
        chunk = next;
    }

    arena->head->next = NULL;
    arena->head->used = 0;

    lock_release(&arena->lock);
}

void* arena_alloc(Arena* arena, size_t size) {
    if (arena == NULL || size == 0) {
        return NULL;
    }

    lock_acquire(&arena->lock);
    size = align_up(size, arena->alignment);

    // Try to allocate from the current chunk
    Chunk* chunk = arena->head;
    while (chunk != NULL) {
        if (chunk->used + size <= chunk->size) {
            void* ptr = (char*)chunk->base + chunk->used;
            chunk->used += size;
            lock_release(&arena->lock);
            return ptr;
        }
        chunk = chunk->next;
    }

    // Allocate a new chunk
    size_t new_chunk_size =
        arena->chunk_size > size + sizeof(Chunk) ? arena->chunk_size : size + sizeof(Chunk);

    chunk = (Chunk*)system_alloc(new_chunk_size);
    if (chunk == NULL) {
        lock_release(&arena->lock);
        return NULL;
    }

    chunk->base = (char*)chunk + sizeof(Chunk);
    chunk->size = new_chunk_size - sizeof(Chunk);
    chunk->used = size;
    chunk->next = arena->head;
    arena->head = chunk;

    lock_release(&arena->lock);
    return chunk->base;
}

void* arena_realloc(Arena* arena, void* ptr, size_t size) {
    if (ptr == NULL) {
        return arena_alloc(arena, size);
    }

    lock_acquire(&arena->lock);

    // Find the chunk containing the pointer
    Chunk* chunk = arena->head;
    while (chunk != NULL) {
        if (ptr >= chunk->base && (char*)ptr < (char*)chunk->base + chunk->used) {
            break;
        }
        chunk = chunk->next;
    }

    if (chunk == NULL) {
        // Pointer not found in the arena
        lock_release(&arena->lock);
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
            current += align_up(*(size_t*)current, arena->alignment);
        }
    }

    // Align the new size
    size = align_up(size, arena->alignment);

    // There is enough space in the current chunk
    if (old_size + size <= chunk->size) {
        chunk->used = old_size + size;
        lock_release(&arena->lock);
        return ptr;
    }

    // unlock the mutex as arena_alloc will lock it again
    lock_release(&arena->lock);

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
