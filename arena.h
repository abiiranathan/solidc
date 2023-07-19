#include <stdio.h>
#include <stdlib.h>

// Linear(arena) of contigous memory block.
typedef struct arena arena_t;

// Init & allocate a new area of memory size.
arena_t* arena_create(size_t size);

// Allocate a memory of size from the area.
void* arena_alloc(arena_t* arena, size_t size);

// Free the arena block.
void arena_free(arena_t* arena);