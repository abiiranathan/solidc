#ifndef E5F7B449_2B66_43F7_9A59_3B15792FB993
#define E5F7B449_2B66_43F7_9A59_3B15792FB993

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ARENA_DEFAULT_CHUNKSIZE
#define ARENA_DEFAULT_CHUNKSIZE (size_t)(1 << 20)
#endif

#ifndef ARENA_ALIGNMENT
#define ARENA_ALIGNMENT (alignof(max_align_t))
#endif

#ifndef ARENA_NOLOCK
#define ARENA_NOLOCK 0
#endif

// Arena memory block.
typedef struct Arena Arena;

// Create a new arena with a chuck size per block of chunk_size;
Arena* arena_create(size_t chunk_size) __attribute__((warn_unused_result()));

// Arena destruction. Frees the memory region allocated for the arena.
void arena_destroy(Arena* arena);

// Reset used memory to zero for all chunks.
// Individual chunks are preserved.
void arena_reset(Arena* arena);

// Clear arena and free allocated chunks.
void arena_clear(Arena* arena);

// Allocate a memory block from the arena.
// Returns NULL on failure.
void* arena_alloc(Arena* arena, size_t size) __attribute__((warn_unused_result()));

// Allocate a new memory block for ptr and copy contents of ptr.
// If ptr is NULL, its the same as arena_alloc.
// If there is no need for re-allocation, the ptr is returned as is.
void* arena_realloc(Arena* arena, void* ptr, size_t size) __attribute__((warn_unused_result()));

// Allocate a string from the arena. The string is copied into the arena
// memory. Returns NULL on failure.
char* arena_alloc_string(Arena* arena, const char* str) __attribute__((warn_unused_result()));

// Allocate and assign an integer.
int* arena_alloc_int(Arena* arena, int n) __attribute__((warn_unused_result()));

#ifdef __cplusplus
}
#endif

#endif /* E5F7B449_2B66_43F7_9A59_3B15792FB993 */
