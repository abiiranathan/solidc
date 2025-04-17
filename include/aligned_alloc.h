#ifndef ALIGNED_ALLOC_H
#define ALIGNED_ALLOC_H

#include <stdlib.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <malloc.h>  // For _aligned_malloc and _aligned_free
#endif

// Cross-platform aligned memory allocation
static inline void* aligned_alloc_xp(size_t alignment, size_t size) {
    // Alignment must be a power of two and at least sizeof(void*)
    if ((alignment & (alignment - 1)) != 0 || alignment < sizeof(void*)) {
        return NULL;
    }

#if defined(_WIN32) || defined(__CYGWIN__)
    return _aligned_malloc(size, alignment);
#else
// Use standard C11 aligned_alloc where available
#if __STDC_VERSION__ >= 201112L
    return aligned_alloc(alignment, size);
#else
    // Fallback for older C standards
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
#endif
#endif
}

// Cross-platform aligned memory deallocation
static inline void aligned_free_xp(void* ptr) {
#if defined(_WIN32) || defined(__CYGWIN__)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// Macro version if preferred
#define ALIGNED_ALLOC(alignment, size) aligned_alloc_xp(alignment, size)
#define ALIGNED_FREE(ptr) aligned_free_xp(ptr)

#endif  // ALIGNED_ALLOC_H
