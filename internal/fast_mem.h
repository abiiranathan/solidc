#ifndef FAST_MEM_H
#define FAST_MEM_H
#ifndef FAST_MEM_H
#define FAST_MEM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MEMORY_SIZE
#define MEMORY_SIZE 1 * 1024 * 1024
#endif

// Fast malloc.
void* FMALLOC(size_t size);

// Fast calloc.
void* FCALLOC(size_t nmemb, size_t size);

// Fast realloc.
void* FREALLOC(void* ptr, size_t size);

// Fast free.
void FFREE(void* ptr);

// Print state of memory blocks.
void FDEBUG_MEMORY(void);

#ifdef __cplusplus
}
#endif

#endif  // FAST_MEM_H

#endif /* FAST_MEM_H */
