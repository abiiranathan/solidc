#ifndef L_ARANA_H_
#define L_ARANA_H_

#include <stdlib.h>
#include <string.h>

// LArena is a linear memory allocator.
// Uses a bump allocation strategy to allocate memory.
// It is NOT thread-safe.
typedef struct LArena LArena;

#ifdef __cplusplus
extern "C" {
#endif

// Create a linear allocator of given size.
// Returns the pointer to the allocator if successful or NULL if malloc fails.
LArena* larena_create(size_t size);

// Allocate size bytes in the arena.
// Not thread-safe.
// Returns the pointer to the memory or NULL if arena is out of memory.
void* larena_alloc(LArena* arena, size_t size);

// Allocate a new NULL-terminated string in the arena and copy s into it.
void* larena_alloc_string(LArena* arena, const char* s);

// Reset arena memory. Simply reset the allocated memory to 0 and
// calls memset to zero the memory.
void larena_reset(LArena* arena);

// Free memory used to LArena and underlying memory.
void larena_destroy(LArena* arena);

#ifdef __cplusplus
}
#endif

#endif  // L_ARANA_H_
