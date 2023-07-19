#include <stdio.h>
#include <stdlib.h>

typedef struct arena {
    char* base;   // base pointer
    size_t size;  // Arena size
    size_t used;  // Total allocated memory
} arena_t;

arena_t* arena_create(size_t size) {
    arena_t* arena = malloc(sizeof(arena_t));
    if (arena) {
        arena->base = malloc(size);
        if (!arena->base) {
            printf("could not allocate arena");
            free(arena);
            return NULL;
        }
        arena->size = size;
        arena->used = 0;
    }
    return arena;
}

void* arena_alloc(arena_t* arena, size_t size) {
    if (arena->used + size > arena->size) {
        return NULL;
    }

    void* ptr = arena->base + arena->used;
    arena->used += size;
    return ptr;
}

void arena_free(arena_t* arena) {
    if (!arena)
        return;

    if (arena->base) {
        free(arena->base);
    }
    free(arena);
}