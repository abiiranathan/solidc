#include "../include/larena.h"

// LArena is a linear memory allocator.
// Uses a bump allocation strategy to allocate memory.
// It is NOT thread-safe.
typedef struct LArena {
    char* memory;
    size_t allocated;
    size_t size;
} LArena;

// Create a linear allocator of given size.
// Returns the pointer to the allocator if successful or NULL if malloc fails.
LArena* larena_create(size_t size) {
    LArena* arena = (LArena*)malloc(sizeof(LArena));
    if (!arena) {
        return NULL;
    }

    arena->memory = malloc(size);
    if (!arena->memory) {
        free(arena);
        return NULL;
    }
    memset(arena->memory, 0, size);
    arena->size      = size;
    arena->allocated = 0;
    return arena;
}

// Allocate size bytes in the arena.
// Not thread-safe.
// Returns the pointer to the memory or NULL if arena is out of memory.
void* larena_alloc(LArena* arena, size_t size) {
    if (arena->allocated + size > arena->size) {
        fprintf(stderr, "LArena is out of memory\n");
        return NULL;
    }

    arena->allocated += size;
    void* ptr = arena->memory + size;
    return ptr;
}

// Reset arena memory. Simply reset the allocated memory to 0 and
// calls memset to zero the memory.
void larena_reset(LArena* arena) {
    arena->allocated = 0;
    memset(arena->memory, 0, arena->size);
}

// Free memory used to LArena and underlying memory.
void larena_destroy(LArena* arena) {
    if (!arena) {
        return;
    }
    free(arena->memory);
    free(arena);
}
