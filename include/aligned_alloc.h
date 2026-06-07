/**
 * @file aligned_alloc.h
 * @brief Aligned memory allocation functions for cross-platform support.
 */

#ifndef ALIGNED_ALLOC_H
#define ALIGNED_ALLOC_H

#include <stdalign.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#include <malloc.h>  // _aligned_malloc and _aligned_free
#endif

/**
 * @brief Cross-platform aligned memory allocation.
 *
 * Allocates @p size bytes whose address is a multiple of @p alignment.
 *
 * Alignment requirements:
 * - @p alignment must be a power of two.
 * - @p alignment must be at least sizeof(void*).
 *
 * Platform implementation details:
 * - Windows uses _aligned_malloc().
 * - POSIX systems use either C11 aligned_alloc() or posix_memalign(),
 *   depending on compiler and standard library support.
 *
 * @warning C11 aligned_alloc() requires that @p size be an exact multiple
 * of @p alignment. Many Linux implementations silently accept non-multiple
 * sizes, but other libc implementations (including those commonly used on
 * macOS) may fail and return NULL.
 *
 * For example:
 *
 * @code
 * void* p = aligned_alloc(64, 120);  // Undefined / may fail
 * @endcode
 *
 * because 120 is not a multiple of 64.
 *
 * This portability difference can cause code that works on Linux to fail on
 * macOS or other platforms. When using aligned_alloc(), callers should
 * either:
 *
 * @code
 * size = (size + alignment - 1) & ~(alignment - 1);
 * @endcode
 *
 * to round the size up to a valid multiple, or prefer an implementation
 * based on posix_memalign(), which does not impose this restriction.
 *
 * @param alignment Required alignment in bytes.
 * @param size Number of bytes to allocate.
 *
 * @return Pointer to aligned storage on success, or NULL if:
 * - alignment is invalid,
 * - the allocation cannot be satisfied,
 * - or the underlying platform rejects the request.
 *
 * @note The returned pointer must be released with
 *       aligned_free_xp().
 */
static inline void* aligned_alloc_xp(size_t alignment, size_t size) {
    if ((alignment & (alignment - 1)) != 0 || alignment < sizeof(void*)) { return NULL; }

    /* Round size up to a multiple of alignment.
     * Required by C11 aligned_alloc() and harmless for
     * posix_memalign()/_aligned_malloc(). */
    size = (size + alignment - 1) & ~(alignment - 1);

#if defined(_WIN32) || defined(__CYGWIN__)
    return _aligned_malloc(size, alignment);
#else
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) { return NULL; }
    return ptr;
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

#define ALIGNED_ALLOC(alignment, size) aligned_alloc_xp(alignment, size)
#define ALIGNED_FREE(ptr)              aligned_free_xp(ptr)

#ifdef __cplusplus
}
#endif

#endif  // ALIGNED_ALLOC_H
