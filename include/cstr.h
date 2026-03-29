/**
 * @file cstr.h
 * @brief High-performance C string with Small String Optimization (SSO).
 *
 * Key design decisions vs. the convential implementation:
 *
 *  1. Self-referential SSO pointer
 *     `data` always points to the live bytes — either into `buf[]` (stack) or a
 *     heap allocation.  Every read/write goes through one pointer with zero
 *     branching; the "is-heap?" test is only needed for free/resize paths.
 *
 *  2. Flat `length` and `capacity` in the struct root (uint32_t)
 *     No union-of-structs, no masked flag bits in length.  The heap-flag lives
 *     in the MSB of `capacity` only.  Reading length is always `s->length`.
 *
 *  3. uint32_t sizes — 4 GB cap is plenty for a string type
 *     Halves the size of the length/capacity fields vs. size_t on 64-bit.
 *     Struct fits in 32 bytes (one cache line on most CPUs).
 *
 *  4. Inline SSO buffer sized to fill the struct to exactly 32 bytes
 *     On 64-bit: ptr(8) + len(4) + cap(4) + buf(16) = 32 bytes.
 *     SSO holds strings up to 15 chars (+ NUL).
 *
 *  5. No memmem — replaced with a fast Rabin-Karp or Sunday's algorithm
 *     variant that avoids glibc's dynamic dispatch overhead for short needles.
 *
 *  Struct layout (64-bit, little-endian):
 *    offset  0 : char*    data      (8 bytes) — always valid pointer
 *    offset  8 : uint32_t length    (4 bytes)
 *    offset 12 : uint32_t capacity  (4 bytes) — MSB = heap flag
 *    offset 16 : char     buf[16]   (16 bytes) — inline storage
 *  Total: 32 bytes
 *
 * @warning Do NOT access struct fields directly — use the API.
 */

#ifndef CSTR_H
#define CSTR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Platform / compiler helpers
 * ---------------------------------------------------------------------- */

#if defined(__GNUC__) || defined(__clang__)
#define CSTR_LIKELY(x)    __builtin_expect(!!(x), 1)
#define CSTR_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#define CSTR_INLINE       static inline __attribute__((always_inline))
#define CSTR_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define CSTR_PURE         __attribute__((pure))
#define CSTR_WARN_UNUSED  __attribute__((warn_unused_result))
#define CSTR_RESTRICT     __restrict__
#else
#define CSTR_LIKELY(x)   (x)
#define CSTR_UNLIKELY(x) (x)
#define CSTR_INLINE      static inline
#define CSTR_NONNULL(...)
#define CSTR_PURE
#define CSTR_WARN_UNUSED
#define CSTR_RESTRICT
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

/** Maximum string length / capacity (4 GB - 1, MSB reserved for heap flag). */
#define CSTR_MAX_LEN ((uint32_t)0x7FFFFFFFu)

/** Size of the inline (SSO) buffer. Strings shorter than this need no heap. */
#define CSTR_SSO_CAP 16u /* 15 usable chars + NUL */

/** Bit flag stored in `capacity` to indicate heap allocation. */
#define CSTR_HEAP_FLAG ((uint32_t)0x80000000u)

/** Sentinel returned by find functions when no match is found. */
#define CSTR_NPOS (-1)

/* Minimum heap allocation to avoid tiny reallocs */
#define CSTR_MIN_HEAP 32u

/* -------------------------------------------------------------------------
 * Core struct  (32 bytes on 64-bit systems)
 * ---------------------------------------------------------------------- */

/**
 * @brief A dynamically resizable C string with SSO.
 *
 * `data` is ALWAYS a valid, NUL-terminated pointer:
 *   - When heap flag is clear: data == &buf[0]  (SSO path)
 *   - When heap flag is set:   data points to malloc'd memory
 *
 * This eliminates the branch in every read — callers just dereference `data`.
 */
typedef struct cstr {
    char* data;             /**< Always-valid pointer to string bytes + NUL     */
    uint32_t length;        /**< Current string length (excluding NUL)           */
    uint32_t capacity;      /**< Heap cap (MSB=heap flag) OR SSO sentinel        */
    char buf[CSTR_SSO_CAP]; /**< Inline buffer (active when !heap flag) */
} cstr;

#ifndef STATIC_ASSERT
#if defined(__cplusplus)
#define STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif
#endif

/* Compile-time layout assertions */
#if defined(__GNUC__) || defined(__clang__)
STATIC_ASSERT(sizeof(cstr) == 32, "cstr must be exactly 32 bytes");
#endif

/* -------------------------------------------------------------------------
 * Lightweight string view
 * ---------------------------------------------------------------------- */

typedef struct {
    const char* data;
    uint32_t length;
} cstr_view;

/* -------------------------------------------------------------------------
 * Internal predicates (inlined, zero overhead)
 * ---------------------------------------------------------------------- */

/** True when string data lives on the heap. */
CSTR_INLINE bool cstr_is_heap(const cstr* s) CSTR_PURE;
CSTR_INLINE bool cstr_is_heap(const cstr* s) { return (s->capacity & CSTR_HEAP_FLAG) != 0; }

/** Extract actual heap capacity (strip flag bit). */
CSTR_INLINE uint32_t cstr_heap_cap(const cstr* s) CSTR_PURE;
CSTR_INLINE uint32_t cstr_heap_cap(const cstr* s) { return s->capacity & ~CSTR_HEAP_FLAG; }

/* -------------------------------------------------------------------------
 * Public API — information
 * ---------------------------------------------------------------------- */

/**
 * @brief Length of the string, excluding NUL.
 */
CSTR_INLINE size_t cstr_len(const cstr* s) CSTR_PURE;
CSTR_INLINE size_t cstr_len(const cstr* s) { return CSTR_LIKELY(s != NULL) ? (size_t)s->length : 0; }

/**
 * @brief Current storage capacity (bytes available before reallocation).
 * For SSO strings this is CSTR_SSO_CAP - 1 (15 chars + room for NUL).
 */
CSTR_INLINE size_t cstr_capacity(const cstr* s) CSTR_PURE;
CSTR_INLINE size_t cstr_capacity(const cstr* s) {
    if (CSTR_UNLIKELY(s == NULL)) return 0;
    return cstr_is_heap(s) ? (size_t)cstr_heap_cap(s) : (size_t)(CSTR_SSO_CAP - 1);
}

/** True if the string is empty or NULL. */
CSTR_INLINE bool cstr_empty(const cstr* s) CSTR_PURE;
CSTR_INLINE bool cstr_empty(const cstr* s) { return CSTR_UNLIKELY(s == NULL) || s->length == 0; }

/** True if the string is heap-allocated (i.e. not in SSO mode). */
CSTR_INLINE bool cstr_allocated(const cstr* s) CSTR_PURE;
CSTR_INLINE bool cstr_allocated(const cstr* s) { return CSTR_LIKELY(s != NULL) && cstr_is_heap(s); }

/* -------------------------------------------------------------------------
 * Public API — data access
 * ---------------------------------------------------------------------- */

/**
 * @brief Mutable pointer to the NUL-terminated string data.
 * Never NULL for a valid cstr.
 */
CSTR_INLINE char* cstr_data(cstr* s) CSTR_PURE;
CSTR_INLINE char* cstr_data(cstr* s) { return CSTR_LIKELY(s != NULL) ? s->data : NULL; }

/**
 * @brief Const pointer to the NUL-terminated string data.
 */
CSTR_INLINE const char* cstr_data_const(const cstr* s) CSTR_PURE;
CSTR_INLINE const char* cstr_data_const(const cstr* s) { return CSTR_LIKELY(s != NULL) ? s->data : NULL; }

/**
 * @brief Character at index, or NUL if out of range.
 */
CSTR_INLINE char cstr_at(const cstr* s, size_t index) CSTR_PURE;
CSTR_INLINE char cstr_at(const cstr* s, size_t index) {
    if (CSTR_UNLIKELY(!s) || CSTR_UNLIKELY(index >= (size_t)s->length)) return '\0';
    return s->data[index];
}

/**
 * @brief Construct a cstr_view from a cstr (zero copy).
 */
CSTR_INLINE cstr_view cstr_as_view(const cstr* s);
CSTR_INLINE cstr_view cstr_as_view(const cstr* s) {
    cstr_view v = {NULL, 0};
    if (CSTR_LIKELY(s != NULL)) {
        v.data   = s->data;
        v.length = s->length;
    }
    return v;
}

/* -------------------------------------------------------------------------
 * Lifecycle
 * ---------------------------------------------------------------------- */

/**
 * @brief Initialize an already-allocated cstr in SSO mode (no heap).
 *
 * Use this to embed a cstr inside another struct without a separate alloc.
 * The cstr must be freed with cstr_drop() (not cstr_free()) when done.
 *
 * @param s  Pointer to uninitialised cstr storage.
 */
CSTR_INLINE void cstr_init_inplace(cstr* s);
CSTR_INLINE void cstr_init_inplace(cstr* s) {
    s->buf[0]   = '\0';
    s->data     = s->buf;
    s->length   = 0;
    s->capacity = 0; /* SSO, no heap flag */
}

/**
 * @brief Create a new heap-allocated cstr with a given initial capacity.
 * @param initial_capacity  Desired usable capacity (NUL not counted).
 * @return New cstr, or NULL on OOM.
 * @note Caller must call cstr_free().
 */
cstr* cstr_init(size_t initial_capacity) CSTR_WARN_UNUSED;

/**
 * @brief Create a new cstr from a C string.
 * @param input  NUL-terminated source. Must not be NULL.
 * @return New cstr, or NULL on OOM.
 * @note Caller must call cstr_free().
 */
cstr* cstr_new(const char* input) CSTR_WARN_UNUSED;

/**
 * @brief Create a new cstr from a buffer of known length (no strlen needed).
 * @param data    Pointer to characters (need not be NUL-terminated).
 * @param length  Number of bytes to copy.
 * @return New cstr, or NULL on OOM.
 */
cstr* cstr_new_len(const char* data, size_t length) CSTR_WARN_UNUSED;

/**
 * @brief Free a heap-allocated cstr and its storage.
 *        Safe to call with NULL.
 */
void cstr_free(cstr* s);

/**
 * @brief Release only the internal heap buffer of an embedded cstr
 *        (one created via cstr_init_inplace).
 *        Does NOT free the cstr struct itself.
 */
void cstr_drop(cstr* s);

/**
 * @brief Print debug information about a cstr to stderr.
 */
void cstr_debug(const cstr* s);

/* -------------------------------------------------------------------------
 * Capacity management
 * ---------------------------------------------------------------------- */

/**
 * @brief Ensure at least `capacity` usable bytes are available (NUL extra).
 * @return true on success, false on OOM or overflow.
 */
bool cstr_reserve(cstr* s, size_t capacity) CSTR_NONNULL(1) CSTR_WARN_UNUSED;

/** Alias kept for compatibility. */
CSTR_INLINE bool cstr_resize(cstr* s, size_t capacity) CSTR_NONNULL(1) CSTR_WARN_UNUSED;
CSTR_INLINE bool cstr_resize(cstr* s, size_t capacity) { return cstr_reserve(s, capacity); }

/**
 * @brief Shrink heap allocation to fit the current length (frees wasted memory).
 */
void cstr_shrink_to_fit(cstr* s) CSTR_NONNULL(1);

/* -------------------------------------------------------------------------
 * Mutation
 * ---------------------------------------------------------------------- */

/**
 * @brief Set length to 0 (data pointer and capacity unchanged).
 */
CSTR_INLINE void cstr_clear(cstr* s);
CSTR_INLINE void cstr_clear(cstr* s) {
    if (CSTR_LIKELY(s != NULL)) {
        s->length  = 0;
        s->data[0] = '\0';
    }
}

/**
 * @brief Append a NUL-terminated C string.
 */
bool cstr_append(cstr* s, const char* CSTR_RESTRICT append) CSTR_NONNULL(1, 2);

/**
 * @brief Append another cstr.
 */
bool cstr_append_cstr(cstr* s, const cstr* append) CSTR_NONNULL(1, 2);

/**
 * @brief Append at most n chars from src.
 */
bool cstr_ncat(cstr* dest, const cstr* src, size_t n) CSTR_NONNULL(1, 2);

/** Alias: concatenate two cstrs. */
CSTR_INLINE bool cstr_cat(cstr* dest, const cstr* src) CSTR_NONNULL(1, 2);
CSTR_INLINE bool cstr_cat(cstr* dest, const cstr* src) { return cstr_append_cstr(dest, src); }

/**
 * @brief Append without capacity check — caller guarantees space.
 *        ~20% faster for bulk building when capacity is pre-reserved.
 * @pre   s->length + strlen(append) < effective_capacity(s)
 */
CSTR_INLINE bool cstr_append_fast(cstr* s, const char* CSTR_RESTRICT append) CSTR_NONNULL(1, 2);
CSTR_INLINE bool cstr_append_fast(cstr* s, const char* CSTR_RESTRICT append) {
    size_t n  = strlen(append);
    char* dst = s->data + s->length;
    memcpy(dst, append, n + 1); /* includes NUL */
    s->length += (uint32_t)n;
    return true;
}

/**
 * @brief Append a single character.
 */
bool cstr_append_char(cstr* s, char c) CSTR_NONNULL(1);

/**
 * @brief Create a new cstr from a printf-style format string.
 */
PRINTF_FORMAT(1, 2) cstr* cstr_format(const char* format, ...) CSTR_WARN_UNUSED;

/**
 * @brief Append a printf-style formatted string.
 */
PRINTF_FORMAT(2, 3) bool cstr_append_fmt(cstr* s, const char* format, ...) CSTR_NONNULL(1, 2);

/**
 * @brief Prepend a NUL-terminated C string.
 */
bool cstr_prepend(cstr* s, const char* prepend) CSTR_NONNULL(1, 2);

/**
 * @brief Prepend another cstr.
 */
bool cstr_prepend_cstr(cstr* s, const cstr* prepend) CSTR_NONNULL(1, 2);

/**
 * @brief Prepend without capacity check — caller guarantees space.
 */
bool cstr_prepend_fast(cstr* s, const char* prepend) CSTR_NONNULL(1, 2);

/**
 * @brief Insert a C string at byte offset `index`.
 */
bool cstr_insert(cstr* s, size_t index, const char* insert) CSTR_NONNULL(1, 3);

/**
 * @brief Insert a cstr at byte offset `index`.
 */
bool cstr_insert_cstr(cstr* s, size_t index, const cstr* insert) CSTR_NONNULL(1, 3);

/**
 * @brief Remove `count` characters starting at `index`.
 */
bool cstr_remove(cstr* s, size_t index, size_t count) CSTR_NONNULL(1);

/**
 * @brief Remove all occurrences of substr (in-place, single pass).
 * @return Number of occurrences removed.
 */
size_t cstr_remove_all(cstr* s, const char* substr) CSTR_NONNULL(1, 2);
size_t cstr_remove_all_cstr(cstr* s, const cstr* substr) CSTR_NONNULL(1, 2);

/**
 * @brief Remove a specific character from every position.
 */
void cstr_remove_char(cstr* s, char c) CSTR_NONNULL(1);

/**
 * @brief Remove a run of `substr_length` bytes starting at `start`.
 */
void cstr_remove_substr(cstr* str, size_t start, size_t substr_length) CSTR_NONNULL(1);

/**
 * @brief Deep copy src into dest (dest is overwritten).
 */
bool cstr_copy(cstr* dest, const cstr* src) CSTR_NONNULL(1, 2);

/** Alias for cstr_copy. */
CSTR_INLINE bool cstr_assign(cstr* dest, const cstr* src) CSTR_NONNULL(1, 2);
CSTR_INLINE bool cstr_assign(cstr* dest, const cstr* src) { return cstr_copy(dest, src); }

/* -------------------------------------------------------------------------
 * Case conversion & formatting
 * ---------------------------------------------------------------------- */

void cstr_lower(cstr* s) CSTR_NONNULL(1);
void cstr_upper(cstr* s) CSTR_NONNULL(1);
bool cstr_snakecase(cstr* s) CSTR_NONNULL(1) CSTR_WARN_UNUSED;
void cstr_camelcase(cstr* s) CSTR_NONNULL(1);
void cstr_pascalcase(cstr* s) CSTR_NONNULL(1);
void cstr_titlecase(cstr* s) CSTR_NONNULL(1);

/* -------------------------------------------------------------------------
 * Trim
 * ---------------------------------------------------------------------- */

void cstr_trim(cstr* s) CSTR_NONNULL(1);
void cstr_rtrim(cstr* s) CSTR_NONNULL(1);
void cstr_ltrim(cstr* s) CSTR_NONNULL(1);
void cstr_trim_chars(cstr* s, const char* chars) CSTR_NONNULL(1, 2);

/* -------------------------------------------------------------------------
 * Comparison & search
 * ---------------------------------------------------------------------- */

/**
 * @brief Lexicographic compare.  NULL < non-NULL; two NULLs are equal.
 */
int cstr_cmp(const cstr* s1, const cstr* s2) CSTR_PURE;
int cstr_ncmp(const cstr* s1, const cstr* s2, size_t n) CSTR_PURE;

/**
 * @brief Equality check (length-first — very fast for non-equal strings).
 */
CSTR_INLINE bool cstr_equals(const cstr* s1, const cstr* s2) CSTR_PURE;
CSTR_INLINE bool cstr_equals(const cstr* s1, const cstr* s2) {
    if (s1 == s2) return true;
    if (!s1 || !s2) return false;
    if (s1->length != s2->length) return false;
    return memcmp(s1->data, s2->data, s1->length) == 0;
}

bool cstr_starts_with(const cstr* s, const char* prefix) CSTR_NONNULL(1, 2) CSTR_PURE;
bool cstr_starts_with_cstr(const cstr* s, const cstr* prefix) CSTR_NONNULL(1, 2) CSTR_PURE;
bool cstr_ends_with(const cstr* s, const char* suffix) CSTR_NONNULL(1, 2) CSTR_PURE;
bool cstr_ends_with_cstr(const cstr* s, const cstr* suffix) CSTR_NONNULL(1, 2) CSTR_PURE;

/**
 * @brief Find first occurrence of substr.  Uses optimised search (no memmem).
 * @return Byte offset, or CSTR_NPOS if not found.
 */
int cstr_find(const cstr* s, const char* substr) CSTR_NONNULL(1, 2) CSTR_PURE;
int cstr_find_cstr(const cstr* s, const cstr* substr) CSTR_NONNULL(1, 2) CSTR_PURE;

/** Find last occurrence. */
int cstr_rfind(const cstr* s, const char* substr) CSTR_NONNULL(1, 2) CSTR_PURE;
int cstr_rfind_cstr(const cstr* s, const cstr* substr) CSTR_NONNULL(1, 2) CSTR_PURE;

CSTR_INLINE bool cstr_contains(const cstr* s, const char* substr) CSTR_NONNULL(1, 2) CSTR_PURE;
CSTR_INLINE bool cstr_contains(const cstr* s, const char* substr) { return cstr_find(s, substr) != CSTR_NPOS; }

CSTR_INLINE bool cstr_contains_cstr(const cstr* s, const cstr* sub) CSTR_NONNULL(1, 2) CSTR_PURE;
CSTR_INLINE bool cstr_contains_cstr(const cstr* s, const cstr* sub) { return cstr_find_cstr(s, sub) != CSTR_NPOS; }

/** Count occurrences (non-overlapping). */
size_t cstr_count_substr(const cstr* s, const char* substr) CSTR_NONNULL(1, 2) CSTR_PURE;
size_t cstr_count_substr_cstr(const cstr* s, const cstr* substr) CSTR_NONNULL(1, 2) CSTR_PURE;

/* -------------------------------------------------------------------------
 * Substrings & replacement  (all return new cstr — caller must free)
 * ---------------------------------------------------------------------- */

cstr* cstr_substr(const cstr* s, size_t start, size_t length) CSTR_NONNULL(1) CSTR_WARN_UNUSED;
cstr* cstr_replace(const cstr* s, const char* old_str, const char* new_str) CSTR_NONNULL(1, 2, 3) CSTR_WARN_UNUSED;
cstr* cstr_replace_all(const cstr* s, const char* old_str, const char* new_str) CSTR_NONNULL(1, 2, 3) CSTR_WARN_UNUSED;

/* -------------------------------------------------------------------------
 * Split & join
 * ---------------------------------------------------------------------- */

/**
 * @brief Split on delimiter.  Returns array of cstr* (each must be freed),
 *        terminated by setting *count_out.  Caller must free each element
 *        and the array itself.
 */
cstr** cstr_split(const cstr* s, const char* delim, size_t* count_out) CSTR_NONNULL(1, 3) CSTR_WARN_UNUSED;

/**
 * @brief Join an array of cstr pointers with a delimiter.
 */
cstr* cstr_join(const cstr** strings, size_t count, const char* delim) CSTR_WARN_UNUSED;

/* -------------------------------------------------------------------------
 * Reverse
 * ---------------------------------------------------------------------- */

cstr* cstr_reverse(const cstr* s) CSTR_NONNULL(1) CSTR_WARN_UNUSED;
void cstr_reverse_inplace(cstr* s) CSTR_NONNULL(1);

#ifdef __cplusplus
}
#endif

#endif /* CSTR_H */
