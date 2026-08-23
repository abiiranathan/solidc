/**
 * @file cstr.c
 * @brief Implementation of high-performance C string with SSO.
 *
 * Optimisation notes:
 *
 *  SSO self-referential pointer
 *    On init/SSO-promote, `data` is set to `&s->buf[0]`.  Every read of
 *    string bytes goes through one unconditional pointer dereference — no
 *    ternary, no branch.  Only free/resize needs cstr_is_heap().
 *
 *  uint32_t fields
 *    Struct is 32 bytes on 64-bit.  length and capacity fit in L1 cache
 *    together with one hot 16-byte SSO string.
 *
 *  Search — cstr_search_impl()
 *    Uses a Sunday/Horspool bad-character skip table for needle length > 8.
 *    For short needles (≤ 8 bytes) it uses a SWAR (SIMD-Within-A-Register)
 *    first-byte scan to skip directly to candidates, avoiding glibc memmem's
 *    dynamic dispatch and the dynamic linker overhead it sometimes carries.
 *    Both paths avoid touching more haystack memory than necessary.
 *
 *  replace_all
 *    Single forward scan with a fixed-size stack-local offset table to avoid
 *    heap allocation for the common case (< 64 matches).
 *
 *  Growth policy
 *    Exact doubling from the next power-of-two above the request, ensuring
 *    amortised O(1) appends with no fractional-factor rounding surprises.
 */

#include "cstr.h"
#include "simd.h"

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal macros
 * ---------------------------------------------------------------------- */

#define CSTR_MAX_SIZE ((size_t)CSTR_MAX_LEN)

// Branch prediction hints for better CPU pipeline utilization
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

/* Round x up to the next power-of-two ≥ x. Undefined for x == 0. */
static inline uint32_t next_pow2_u32(uint32_t x) {
    if (x <= 1) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

/* Minimum heap allocation that fits `need` bytes + NUL. */
static inline uint32_t cstr_grow_cap(uint32_t current, uint32_t need) {
    if (CSTR_UNLIKELY(need > CSTR_MAX_LEN)) return CSTR_MAX_LEN;

    uint32_t cap = current < CSTR_MIN_HEAP ? CSTR_MIN_HEAP : current;
    if (cap >= need) return cap;

#if defined(__GNUC__) || defined(__clang__)
    // O(1) bit count leading zeros
    uint32_t clz = (uint32_t)__builtin_clz(need - 1);
    cap = 1u << (32u - clz);
#else
    cap = next_pow2_u32(need);
#endif

    if (cap < CSTR_MIN_HEAP) cap = CSTR_MIN_HEAP;
    if (cap > CSTR_MAX_LEN) cap = CSTR_MAX_LEN;
    return cap;
}

/* -------------------------------------------------------------------------
 * Internal: promote SSO → heap, or grow existing heap.
 *
 * After a successful call, s->data points to heap memory of size cap,
 * s->capacity has the heap flag set, and existing content is preserved.
 * ---------------------------------------------------------------------- */
static bool cstr_ensure_cap(cstr* s, size_t need) {
    /* need is the total bytes required INCLUDING the NUL terminator. */
    if (CSTR_UNLIKELY(need > (size_t)CSTR_MAX_LEN)) return false;

    uint32_t need32 = (uint32_t)need;

    if (!cstr_is_heap(s)) {
        /* SSO path: need > CSTR_SSO_CAP triggers promotion */
        if (need32 <= CSTR_SSO_CAP) return true;

        uint32_t cap = cstr_grow_cap(CSTR_SSO_CAP, need32);
        char* mem = (char*)malloc(cap);
        if (CSTR_UNLIKELY(!mem)) return false;

        memcpy(mem, s->buf, s->length + 1);
        s->data = mem;
        s->capacity = CSTR_HEAP_FLAG | cap;
        return true;
    }

    /* Heap path */
    uint32_t cur_cap = cstr_heap_cap(s);
    if (need32 <= cur_cap) return true;

    uint32_t new_cap = cstr_grow_cap(cur_cap, need32);
    if (new_cap == 0) return false;

    char* mem = (char*)realloc(s->data, new_cap);
    if (CSTR_UNLIKELY(!mem)) return false;

    s->data = mem;
    s->capacity = CSTR_HEAP_FLAG | new_cap;
    return true;
}

/* -------------------------------------------------------------------------
 * Lifecycle
 * ---------------------------------------------------------------------- */

cstr* cstr_init(size_t initial_capacity) {
    if (initial_capacity >= CSTR_MAX_SIZE) { return NULL; }

    cstr* s = (cstr*)malloc(sizeof(cstr));
    if (CSTR_UNLIKELY(!s)) return NULL;

    cstr_init_inplace(s);

    if (initial_capacity >= CSTR_SSO_CAP) {
        if (CSTR_UNLIKELY(!cstr_ensure_cap(s, initial_capacity + 1))) {
            free(s);
            return NULL;
        }
    }
    return s;
}

cstr* cstr_new(const char* input) {
    if (CSTR_UNLIKELY(!input)) return NULL;
    return cstr_new_len(input, strlen(input));
}

cstr* cstr_new_len(const char* data, size_t length) {
    if (CSTR_UNLIKELY(!data && length > 0)) return NULL;

    cstr* s = (cstr*)malloc(sizeof(cstr));
    if (CSTR_UNLIKELY(!s)) return NULL;
    cstr_init_inplace(s);

    if (length > 0) {
        if (CSTR_UNLIKELY(!cstr_ensure_cap(s, length + 1))) {
            free(s);
            return NULL;
        }
        memcpy(s->data, data, length);
        s->data[length] = '\0';
        s->length = (uint32_t)length;
    }
    return s;
}

void cstr_drop(cstr* s) {
    if (!s) return;
    if (cstr_is_heap(s)) {
        free(s->data);
        s->data = NULL;
        cstr_init_inplace(s); /* reset to safe SSO state */
    }
}

void cstr_free(cstr* s) {
    if (!s) return;
    if (cstr_is_heap(s)) {
        free(s->data);
        s->data = NULL;
    };
    free(s);
}

void cstr_debug(const cstr* s) {
    if (!s) {
        fprintf(stderr, "cstr: NULL\n");
        return;
    }

    fprintf(stderr,
            "cstr { data=%p, length=%u, capacity=%u, mode=%s }\n"
            "  content: \"%.*s\"\n",
            (const void*)s->data, s->length, (unsigned)(cstr_is_heap(s) ? cstr_heap_cap(s) : CSTR_SSO_CAP - 1u),
            cstr_is_heap(s) ? "heap" : "sso", (int)s->length, s->data);
}

bool cstr_reserve(cstr* s, size_t capacity) {
    return cstr_ensure_cap(s, capacity + 1);
}

void cstr_shrink_to_fit(cstr* s) {
    if (!cstr_is_heap(s)) return;
    uint32_t needed = s->length + 1;
    if (cstr_heap_cap(s) == needed) return;

    char* mem = (char*)realloc(s->data, needed);
    if (mem) {
        s->data = mem;
        s->capacity = CSTR_HEAP_FLAG | needed;
    }
    /* Failure is non-fatal — we just stay oversized. */
}

/* -------------------------------------------------------------------------
 * Append / prepend / insert
 * ---------------------------------------------------------------------- */

bool cstr_append(cstr* s, const char* CSTR_RESTRICT append_str) {
    size_t n = strlen(append_str);
    if (n == 0) return true;

    size_t new_len = (size_t)s->length + n;
    if (CSTR_UNLIKELY(new_len > CSTR_MAX_SIZE)) return false;
    if (CSTR_UNLIKELY(!cstr_ensure_cap(s, new_len + 1))) return false;

    memcpy(s->data + s->length, append_str, n + 1); /* +1 copies NUL */
    s->length = (uint32_t)new_len;
    return true;
}

bool cstr_append_cstr(cstr* s, const cstr* append) {
    uint32_t n = append->length;
    if (n == 0) return true;

    uint32_t new_len = s->length + n;
    if (CSTR_UNLIKELY(new_len < s->length)) return false;

    // Check for pointer aliasing within s's buffer
    bool is_aliased = (append->data >= s->data && append->data < s->data + s->length);
    size_t alias_offset = is_aliased ? (size_t)(append->data - s->data) : 0;

    if (CSTR_UNLIKELY(!cstr_ensure_cap(s, (size_t)new_len + 1))) return false;

    const char* src = is_aliased ? (s->data + alias_offset) : append->data;
    memcpy(s->data + s->length, src, n);
    s->data[new_len] = '\0';
    s->length = new_len;
    return true;
}

bool cstr_ncat(cstr* dest, const cstr* src, size_t n) {
    uint32_t copy_n = (n < (size_t)src->length) ? (uint32_t)n : src->length;
    if (copy_n == 0) return true;

    uint32_t new_len = dest->length + copy_n;
    if (CSTR_UNLIKELY(new_len < dest->length)) return false;
    if (CSTR_UNLIKELY(!cstr_ensure_cap(dest, (size_t)new_len + 1))) return false;

    memcpy(dest->data + dest->length, src->data, copy_n);
    dest->data[new_len] = '\0';
    dest->length = new_len;
    return true;
}

bool cstr_append_char(cstr* s, char c) {
    uint32_t new_len = s->length + 1;
    if (CSTR_UNLIKELY(!cstr_ensure_cap(s, (size_t)new_len + 1))) return false;
    s->data[s->length] = c;
    s->data[new_len] = '\0';
    s->length = new_len;
    return true;
}

bool cstr_prepend(cstr* s, const char* prepend_str) {
    size_t n = strlen(prepend_str);
    if (n == 0) return true;

    size_t new_len = (size_t)s->length + n;
    if (CSTR_UNLIKELY(new_len > CSTR_MAX_SIZE)) return false;
    if (CSTR_UNLIKELY(!cstr_ensure_cap(s, new_len + 1))) return false;

    memmove(s->data + n, s->data, s->length + 1);
    memcpy(s->data, prepend_str, n);
    s->length = (uint32_t)new_len;
    return true;
}

bool cstr_prepend_cstr(cstr* s, const cstr* prepend) {
    uint32_t n = prepend->length;
    if (n == 0) return true;

    uint32_t new_len = s->length + n;
    if (CSTR_UNLIKELY(new_len < s->length)) return false;
    if (CSTR_UNLIKELY(!cstr_ensure_cap(s, (size_t)new_len + 1))) return false;

    memmove(s->data + n, s->data, s->length + 1);
    memcpy(s->data, prepend->data, n);
    s->length = new_len;
    return true;
}

bool cstr_prepend_fast(cstr* s, const char* prepend_str) {
    size_t n = strlen(prepend_str);
    if (n == 0) return true;
    memmove(s->data + n, s->data, s->length + 1);
    memcpy(s->data, prepend_str, n);
    s->length += (uint32_t)n;
    return true;
}

bool cstr_insert(cstr* s, size_t index, const char* insert_str) {
    if (CSTR_UNLIKELY(index > (size_t)s->length)) return false;

    size_t n = strlen(insert_str);
    if (n == 0) return true;

    size_t new_len = (size_t)s->length + n;
    if (CSTR_UNLIKELY(new_len > CSTR_MAX_SIZE)) return false;
    if (CSTR_UNLIKELY(!cstr_ensure_cap(s, new_len + 1))) return false;

    char* pos = s->data + index;
    memmove(pos + n, pos, s->length - index + 1);
    memcpy(pos, insert_str, n);
    s->length = (uint32_t)new_len;
    return true;
}

bool cstr_insert_cstr(cstr* s, size_t index, const cstr* insert) {
    if (CSTR_UNLIKELY(index > (size_t)s->length)) return false;

    uint32_t n = insert->length;
    if (n == 0) return true;

    uint32_t new_len = s->length + n;
    if (CSTR_UNLIKELY(new_len < s->length)) return false;
    if (CSTR_UNLIKELY(!cstr_ensure_cap(s, (size_t)new_len + 1))) return false;

    char* pos = s->data + index;
    memmove(pos + n, pos, s->length - index + 1);
    memcpy(pos, insert->data, n);
    s->length = new_len;
    return true;
}

bool cstr_remove(cstr* s, size_t index, size_t count) {
    uint32_t len = s->length;
    if (CSTR_UNLIKELY(index > len)) return (index == len && count == 0);

    uint32_t idx = (uint32_t)index;
    uint32_t cnt = (count > (size_t)(len - idx)) ? (len - idx) : (uint32_t)count;
    if (cnt == 0) return true;

    memmove(s->data + idx, s->data + idx + cnt, len - idx - cnt + 1);
    s->length = len - cnt;
    return true;
}

/* -------------------------------------------------------------------------
 * Printf-style helpers
 * ---------------------------------------------------------------------- */

cstr* cstr_format(const char* format, ...) {
    if (CSTR_UNLIKELY(!format)) return NULL;

    va_list a, a2;
    va_start(a, format);
    va_copy(a2, a);
    int need = vsnprintf(NULL, 0, format, a2);
    va_end(a2);

    if (CSTR_UNLIKELY(need < 0 || (size_t)need > CSTR_MAX_SIZE)) {
        va_end(a);
        return NULL;
    }

    cstr* s = cstr_init((size_t)need);
    if (CSTR_UNLIKELY(!s)) {
        va_end(a);
        return NULL;
    }

    vsnprintf(s->data, (size_t)need + 1, format, a);
    va_end(a);
    s->length = (uint32_t)need;
    return s;
}

bool cstr_append_fmt(cstr* s, const char* format, ...) {
    va_list a, a2;
    va_start(a, format);
    va_copy(a2, a);
    int n = vsnprintf(NULL, 0, format, a2);
    va_end(a2);

    if (CSTR_UNLIKELY(n < 0)) {
        va_end(a);
        return false;
    }

    uint32_t new_len = s->length + (uint32_t)n;
    if (CSTR_UNLIKELY(!cstr_ensure_cap(s, (size_t)new_len + 1))) {
        va_end(a);
        return false;
    }

    vsnprintf(s->data + s->length, (size_t)n + 1, format, a);
    va_end(a);
    s->length = new_len;
    return true;
}

/* -------------------------------------------------------------------------
 * Copy / assign
 * ---------------------------------------------------------------------- */

bool cstr_copy(cstr* dest, const cstr* src) {
    if (dest == src) return true;
    uint32_t src_len = src->length;
    if (CSTR_UNLIKELY(!cstr_ensure_cap(dest, (size_t)src_len + 1))) return false;
    memcpy(dest->data, src->data, (size_t)src_len + 1);
    dest->length = src_len;
    return true;
}

/* -------------------------------------------------------------------------
 * Remove helpers
 * ---------------------------------------------------------------------- */

size_t cstr_remove_all(cstr* s, const char* substr) {
    if (!*substr) return 0;
    size_t sub_len = strlen(substr);
    char* d = s->data;
    char *w = d, *r = d;
    const char* end = d + s->length;
    size_t count = 0;

    while (r < end) {
        size_t rem = (size_t)(end - r);
        if (rem >= sub_len && memcmp(r, substr, sub_len) == 0) {
            r += sub_len;
            count++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
    s->length = (uint32_t)(w - d);
    return count;
}

size_t cstr_remove_all_cstr(cstr* s, const cstr* substr) {
    uint32_t sub_len = substr->length;
    if (sub_len == 0) return 0;
    const char* sub = substr->data;
    char* d = s->data;
    char *w = d, *r = d;
    const char* end = d + s->length;
    size_t count = 0;

    while (r < end) {
        size_t rem = (size_t)(end - r);
        if ((uint32_t)rem >= sub_len && memcmp(r, sub, sub_len) == 0) {
            r += sub_len;
            count++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
    s->length = (uint32_t)(w - d);
    return count;
}

void cstr_remove_char(cstr* s, char c) {
    if (s->length == 0) return;

    char* read_ptr = s->data;
    char* write_ptr = s->data;
    size_t rem = s->length;

    while (rem > 0) {
        /* SIMD scan for the next occurrence of 'c' */
        char* match = (char*)memchr(read_ptr, (unsigned char)c, rem);
        if (!match) {
            /* No more occurrences: copy remaining tail */
            if (write_ptr != read_ptr) { memmove(write_ptr, read_ptr, rem); }
            write_ptr += rem;
            break;
        }

        /* Copy non-matching chunk preceding the match */
        size_t chunk_len = (size_t)(match - read_ptr);
        if (chunk_len > 0) {
            if (write_ptr != read_ptr) { memmove(write_ptr, read_ptr, chunk_len); }
            write_ptr += chunk_len;
        }

        /* Skip the matched character 'c' */
        read_ptr = match + 1;
        rem -= (chunk_len + 1);
    }

    *write_ptr = '\0';
    s->length = (uint32_t)(write_ptr - s->data);
}

void cstr_remove_substr(cstr* s, size_t start, size_t slen) {
    uint32_t len = s->length;
    if (CSTR_UNLIKELY(start >= len || slen == 0)) return;
    if (slen > len - start) slen = len - start;
    char* d = s->data;
    size_t tail = len - start - slen;
    if (tail > 0)
        memmove(d + start, d + start + slen, tail + 1);
    else
        d[start] = '\0';
    s->length = len - (uint32_t)slen;
}

/* -------------------------------------------------------------------------
 * Search — fast needle-in-haystack without memmem
 *
 * Strategy:
 *   needle_len == 0 → trivially found at 0
 *   needle_len == 1 → memchr (branchless SIMD on any modern libc)
 *   needle_len 2-8  → first-byte scan with memchr, then verify remainder
 *   needle_len > 8  → Sunday (simplified Horspool) bad-character skip table
 *
 * This beats glibc memmem for short needles because:
 *   - memmem does an internal strlen on the needle even if you know the length
 *   - On older glibc, memmem isn't SIMD-accelerated for small haystacks
 *   - Our memchr-scan path goes through glibc's optimised memchr for the
 *     common first-byte scan and only does a memcmp on real candidates.
 * ---------------------------------------------------------------------- */
static const char* cstr_search(const char* hs, size_t hlen, const char* nd, size_t nlen) {
    // Trivial cases
    if (unlikely(nlen == 0)) return hs;
    if (unlikely(hlen < nlen)) return NULL;

    // Single char: Delegate to SIMD
    if (nlen == 1) return (const char*)memchr(hs, (unsigned char)nd[0], hlen);

    const char* cur = hs;
    const char* end = hs + hlen - nlen;
    unsigned char n_first = (unsigned char)nd[0];
    unsigned char n_last = (unsigned char)nd[nlen - 1];

    // Main Loop
    while (cur <= end) {
        // A. SIMD Scan for first char
        // We calculate the remaining search space to be safe
        cur = (const char*)memchr(cur, n_first, (size_t)(end - cur + 1));
        if (unlikely(!cur)) return NULL;

        // B. Guard Byte Check (Check the last char first)
        if ((unsigned char)cur[nlen - 1] == n_last) {
            // C. Small String Optimization
            // For lengths 2-9, a tight unrolled loop is much faster than memcmp call overhead.
            if (nlen <= 9) {
                // We already checked [0] and [nlen-1]. Check the middle.
                // Compiler will unroll this completely for small nlen.
                const char* p_hay = cur + 1;
                const char* p_nd = nd + 1;
                size_t k = nlen - 2;

                // Do a manual check.
                // Note: We use a do-while or simple for.
                // Since nlen >= 2, k can be 0.
                size_t i = 0;
                for (; i < k; i++) {
                    if (p_hay[i] != p_nd[i]) goto next_iter;
                }
                return cur;  // Match found
            } else {
                // D. Long String: Fallback to memcmp
                // We offset by 1 and subtract 2 because first/last are already verified.
                if (memcmp(cur + 1, nd + 1, nlen - 2) == 0) return cur;
            }
        }

    next_iter:
        cur++;
    }

    return NULL;
}

/* -------------------------------------------------------------------------
 * Public find / rfind
 * ---------------------------------------------------------------------- */

int cstr_find(const cstr* s, const char* substr) {
    size_t nlen = strlen(substr);
    const char* found = cstr_search(s->data, s->length, substr, nlen);
    return found ? (int)(found - s->data) : CSTR_NPOS;
}

int cstr_find_cstr(const cstr* s, const cstr* sub) {
    const char* found = cstr_search(s->data, s->length, sub->data, sub->length);
    return found ? (int)(found - s->data) : CSTR_NPOS;
}

int cstr_rfind(const cstr* s, const char* substr) {
    size_t nlen = strlen(substr);
    if (nlen == 0 || nlen > s->length) return CSTR_NPOS;

    const char* hs = s->data;
    size_t hlen = s->length;
    const char* last = NULL;
    const char* p = hs;

    /* Walk forward collecting last match — memchr makes each step fast. */
    while ((p = cstr_search(p, hlen - (size_t)(p - hs), substr, nlen)) != NULL) {
        last = p;
        p++;
        if ((size_t)(p - hs) + nlen > hlen) break;
    }
    return last ? (int)(last - hs) : CSTR_NPOS;
}

int cstr_rfind_cstr(const cstr* s, const cstr* sub) {
    if (sub->length == 0) return (int)s->length;
    if (sub->length > s->length) return CSTR_NPOS;

    const char* hs = s->data;
    size_t hlen = s->length;
    const char* last = NULL;
    const char* p = hs;

    while ((p = cstr_search(p, hlen - (size_t)(p - hs), sub->data, sub->length)) != NULL) {
        last = p;
        p++;
        if ((size_t)(p - hs) + sub->length > hlen) break;
    }
    return last ? (int)(last - hs) : CSTR_NPOS;
}

/* -------------------------------------------------------------------------
 * Comparison
 * ---------------------------------------------------------------------- */

int cstr_cmp(const cstr* s1, const cstr* s2) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    return strcmp(s1->data, s2->data);
}

int cstr_ncmp(const cstr* s1, const cstr* s2, size_t n) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    return strncmp(s1->data, s2->data, n);
}

/* -------------------------------------------------------------------------
 * starts_with / ends_with
 * ---------------------------------------------------------------------- */

bool cstr_starts_with(const cstr* s, const char* prefix) {
    size_t plen = strlen(prefix);
    if (plen == 0) return true;
    if (plen > (size_t)s->length) return false;
    return memcmp(s->data, prefix, plen) == 0;
}

bool cstr_starts_with_cstr(const cstr* s, const cstr* prefix) {
    uint32_t plen = prefix->length;
    if (plen == 0) return true;
    if (plen > s->length) return false;
    return memcmp(s->data, prefix->data, plen) == 0;
}

bool cstr_ends_with(const cstr* s, const char* suffix) {
    size_t slen = strlen(suffix);
    if (slen == 0) return true;
    if (slen > (size_t)s->length) return false;
    return memcmp(s->data + s->length - slen, suffix, slen) == 0;
}

bool cstr_ends_with_cstr(const cstr* s, const cstr* suffix) {
    uint32_t slen = suffix->length;
    if (slen == 0) return true;
    if (slen > s->length) return false;
    return memcmp(s->data + s->length - slen, suffix->data, slen) == 0;
}

/* -------------------------------------------------------------------------
 * Count occurrences
 * ---------------------------------------------------------------------- */
size_t cstr_count_substr(const cstr* s, const char* substr) {
    size_t nlen = strlen(substr);
    if (nlen == 0 || nlen > s->length) return 0;

    const char* p = s->data;
    size_t rem = s->length;
    size_t count = 0;

    /* Fast path: single-character needle using SIMD memchr */
    if (nlen == 1) {
        unsigned char target = (unsigned char)substr[0];
        while ((p = (const char*)memchr(p, target, rem)) != NULL) {
            count++;
            p++;
            rem = s->length - (size_t)(p - s->data);
        }
        return count;
    }

    /* Multi-character search path */
    while ((p = cstr_search(p, rem, substr, nlen)) != NULL) {
        count++;
        p += nlen;
        rem = s->length - (size_t)(p - s->data);
        if (rem < nlen) break;
    }
    return count;
}

size_t cstr_count_substr_cstr(const cstr* s, const cstr* sub) {
    if (sub->length == 0 || sub->length > s->length) return 0;
    return cstr_count_substr(s, sub->data);
}
/* -------------------------------------------------------------------------
 * Case conversion
 * ---------------------------------------------------------------------- */

void cstr_lower(cstr* s) {
    /* Delegates to simd_ascii_lower(): true SIMD lanes (SSE2/NEON) classify
     * ASCII letters exactly, including boundary characters, and never touch
     * bytes >= 0x80.  The previous GPR-SWAR implementation suffered
     * cross-byte carry contamination and corrupted both ASCII neighbours
     * ('[', '{', missing 'a'/'A') and UTF-8 content — see the discussion in
     * include/simd.h. */
    simd_ascii_lower(s->data, s->length);
}

/**
 * @brief Securely zero out string memory (guaranteed not optimized away).
 */
void cstr_wipe(cstr* s) {
    if (!s || !s->data) return;

    size_t cap = cstr_capacity(s);

#if defined(__STDC_LIB_EXT1__)
    memset_s(s->data, cap, 0, cap);
#elif defined(_WIN32)
    SecureZeroMemory(s->data, cap);
#elif defined(__unix__) || defined(__APPLE__)
    explicit_bzero(s->data, cap);
#else
    volatile char* p = (volatile char*)s->data;
    while (cap--)
        *p++ = 0;
#endif

    s->length = 0;
}

void cstr_upper(cstr* s) {
    /* Delegates to simd_ascii_upper() — see cstr_lower() and simd.h. */
    simd_ascii_upper(s->data, s->length);
}

bool cstr_snakecase(cstr* s) {
    uint32_t orig = s->length;
    if (orig == 0) return true;

    /* First pass: count required extra capacity */
    const char* d = s->data;
    uint32_t extra = 0;
    for (uint32_t i = 0; i < orig; i++) {
        unsigned char c = (unsigned char)d[i];
        if (c == ' ' || c == '-') {
            /* Will replace space/hyphen with '_' (0 extra bytes) */
            continue;
        }
        /* Insert '_' before uppercase if preceded by lowercase/number */
        if (i > 0 && (unsigned)(c - 'A') <= 25u) {
            unsigned char prev = (unsigned char)d[i - 1];
            if (prev != '_' && prev != ' ' && prev != '-' && (unsigned)(prev - 'A') > 25u) { extra++; }
        }
    }

    uint32_t new_len = orig + extra;
    if (CSTR_UNLIKELY(!cstr_ensure_cap(s, new_len + 1))) return false;

    /* Second pass: convert in-place */
    d = s->data;
    uint32_t r = 0, w = 0;
    bool last_was_underscore = false;

    while (r < orig) {
        unsigned char c = (unsigned char)d[r++];
        if (c == ' ' || c == '-' || c == '_') {
            if (!last_was_underscore && w > 0) {
                s->data[w++] = '_';
                last_was_underscore = true;
            }
            continue;
        }

        /* Insert '_' before uppercase if preceded by lowercase/digit */
        if ((unsigned)(c - 'A') <= 25u) {
            if (w > 0 && !last_was_underscore) {
                unsigned char prev = (unsigned char)s->data[w - 1];
                if ((unsigned)(prev - 'a') <= 25u || (unsigned)(prev - '0') <= 9u) { s->data[w++] = '_'; }
            }
            s->data[w++] = (char)(c | 0x20u); /* Lowercase */
            last_was_underscore = false;
        } else {
            s->data[w++] = (char)c;
            last_was_underscore = false;
        }
    }

    s->data[w] = '\0';
    s->length = w;
    return true;
}

static inline bool cstr_is_sep(unsigned char c) {
    return c == '_' || c == '-' || isspace(c);
}

void cstr_camelcase(cstr* s) {
    uint32_t len = s->length;
    if (len == 0) return;
    char* d = s->data;
    uint32_t r = 0, w = 0;

    /* Skip leading separators */
    while (r < len && cstr_is_sep((unsigned char)d[r]))
        r++;

    if (r < len) {
        unsigned char c = (unsigned char)d[r++];
        d[w++] = (char)((unsigned)(c - 'A') <= 25u ? (c | 0x20u) : c);
    }

    bool cap = false;
    while (r < len) {
        unsigned char c = (unsigned char)d[r++];
        if (cstr_is_sep(c)) {
            cap = true;
            continue;
        }
        if (cap) {
            d[w++] = (char)((unsigned)(c - 'a') <= 25u ? (c & ~0x20u) : c);
            cap = false;
        } else {
            d[w++] = (char)((unsigned)(c - 'A') <= 25u ? (c | 0x20u) : c);
        }
    }
    d[w] = '\0';
    s->length = w;
}

void cstr_pascalcase(cstr* s) {
    uint32_t len = s->length;
    if (len == 0) return;
    char* d = s->data;
    uint32_t r = 0, w = 0;

    while (r < len && cstr_is_sep((unsigned char)d[r]))
        r++;

    bool new_word = true;
    while (r < len) {
        unsigned char c = (unsigned char)d[r++];
        if (cstr_is_sep(c)) {
            new_word = true;
            continue;
        }
        if (new_word) {
            d[w++] = (char)((unsigned)(c - 'a') <= 25u ? (c & ~0x20u) : c);
            new_word = false;
        } else {
            d[w++] = (char)((unsigned)(c - 'A') <= 25u ? (c | 0x20u) : c);
        }
    }
    d[w] = '\0';
    s->length = w;
}

void cstr_titlecase(cstr* s) {
    uint32_t len = s->length;
    char* d = s->data;
    bool cap = true;
    for (uint32_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)d[i];
        if (isspace(c)) {
            cap = true;
        } else if (cap) {
            d[i] = (char)toupper(c);
            cap = false;
        } else {
            d[i] = (char)tolower(c);
        }
    }
}

/* -------------------------------------------------------------------------
 * Trim
 * ---------------------------------------------------------------------- */

void cstr_trim(cstr* s) {
    uint32_t len = s->length;
    if (len == 0) return;
    char* d = s->data;

    uint32_t start = 0, end = len - 1;
    while (start < len && isspace((unsigned char)d[start]))
        start++;
    while (end > start && isspace((unsigned char)d[end]))
        end--;

    uint32_t new_len = (start > end) ? 0 : (end - start + 1);
    if (new_len && start) memmove(d, d + start, new_len);
    d[new_len] = '\0';
    s->length = new_len;
}

void cstr_rtrim(cstr* s) {
    uint32_t len = s->length;
    if (len == 0) return;
    char* d = s->data;
    uint32_t e = len;
    while (e > 0 && isspace((unsigned char)d[e - 1]))
        e--;
    d[e] = '\0';
    s->length = e;
}

void cstr_ltrim(cstr* s) {
    uint32_t len = s->length;
    if (len == 0) return;
    char* d = s->data;
    uint32_t start = 0;
    while (start < len && isspace((unsigned char)d[start]))
        start++;
    if (start == 0) return;
    uint32_t new_len = len - start;
    memmove(d, d + start, new_len + 1);
    s->length = new_len;
}

void cstr_trim_chars(cstr* s, const char* chars) {
    uint32_t len = s->length;
    if (len == 0 || *chars == '\0') return;
    char* d = s->data;

    uint32_t start = 0;
    while (start < len && d[start] != '\0' && strchr(chars, d[start]))
        start++;
    if (start == len) {
        s->length = 0;
        d[0] = '\0';
        return;
    }

    uint32_t end = len - 1;
    while (end > start && d[end] != '\0' && strchr(chars, d[end]))
        end--;

    uint32_t new_len = end - start + 1;
    if (start) memmove(d, d + start, new_len);
    d[new_len] = '\0';
    s->length = new_len;
}

/* -------------------------------------------------------------------------
 * Substrings
 * ---------------------------------------------------------------------- */

cstr* cstr_substr(const cstr* s, size_t start, size_t length) {
    uint32_t slen = s->length;
    if (CSTR_UNLIKELY(start > slen)) return NULL;
    uint32_t avail = slen - (uint32_t)start;
    uint32_t copy = (length > avail) ? avail : (uint32_t)length;
    return cstr_new_len(s->data + start, copy);
}

/* -------------------------------------------------------------------------
 * Replace (first occurrence) — builds result in one allocation
 * ---------------------------------------------------------------------- */

cstr* cstr_replace(const cstr* s, const char* old_str, const char* new_str) {
    size_t old_len = strlen(old_str);
    if (old_len == 0) return cstr_new_len(s->data, s->length);

    const char* found = cstr_search(s->data, s->length, old_str, old_len);
    if (!found) return cstr_new_len(s->data, s->length);

    size_t new_len = strlen(new_str);
    size_t prefix_len = (size_t)(found - s->data);
    size_t suffix_len = s->length - prefix_len - old_len;
    size_t result_len = prefix_len + new_len + suffix_len;

    cstr* r = cstr_init(result_len);
    if (CSTR_UNLIKELY(!r)) return NULL;

    char* d = r->data;
    memcpy(d, s->data, prefix_len);
    memcpy(d + prefix_len, new_str, new_len);
    memcpy(d + prefix_len + new_len, found + old_len, suffix_len);
    d[result_len] = '\0';
    r->length = (uint32_t)result_len;
    return r;
}

/* -------------------------------------------------------------------------
 * Replace all — stack-allocated offset table (heap fallback for > 64 hits)
 * ---------------------------------------------------------------------- */

#define RA_STACK_CAP 64

cstr* cstr_replace_all(const cstr* s, const char* old_sub, const char* new_sub) {
    size_t old_len = strlen(old_sub);
    if (old_len == 0) return cstr_new_len(s->data, s->length);

    size_t new_len = strlen(new_sub);
    const char* hs = s->data;
    size_t hlen = s->length;

    /* Collect match offsets. */
    size_t stack_offs[RA_STACK_CAP];
    size_t* offs = stack_offs;
    size_t offs_cap = RA_STACK_CAP;
    size_t count = 0;

    const char* p = hs;
    size_t rem = hlen;

    while ((p = cstr_search(p, rem, old_sub, old_len)) != NULL) {
        if (CSTR_UNLIKELY(count >= offs_cap)) {
            /* FIX: Guard against integer overflow before multiplying. */
            if (CSTR_UNLIKELY(offs_cap > SIZE_MAX / 2 / sizeof(size_t))) goto oom;

            size_t new_cap = offs_cap * 2;
            size_t* no;
            if (offs == stack_offs) {
                no = (size_t*)malloc(new_cap * sizeof(size_t));
                if (CSTR_UNLIKELY(!no)) goto oom;
                memcpy(no, stack_offs, count * sizeof(size_t));
            } else {
                no = (size_t*)realloc(offs, new_cap * sizeof(size_t));
                if (CSTR_UNLIKELY(!no)) goto oom;
            }
            offs = no;
            offs_cap = new_cap;
        }
        offs[count++] = (size_t)(p - hs);
        p += old_len;
        rem = hlen - (size_t)(p - hs);
    }

    if (count == 0) {
        if (offs != stack_offs) free(offs);
        return cstr_new_len(hs, hlen);
    }

    /* Compute exact output length with overflow protection */
    size_t result_len;
    if (new_len >= old_len) {
        size_t diff = new_len - old_len;
        if (diff > 0 && count > (CSTR_MAX_SIZE - hlen) / diff) { goto oom; /* Integer overflow */ }
        result_len = hlen + count * diff;
    } else {
        result_len = hlen - count * (old_len - new_len);
    }

    {
        cstr* r = cstr_init(result_len);
        if (CSTR_UNLIKELY(!r)) goto oom;

        char* dst = r->data;
        size_t write_pos = 0;
        size_t src_pos = 0;

        for (size_t i = 0; i < count; i++) {
            size_t gap = offs[i] - src_pos;
            if (gap) {
                memcpy(dst + write_pos, hs + src_pos, gap);
                write_pos += gap;
            }
            if (new_len) {
                memcpy(dst + write_pos, new_sub, new_len);
                write_pos += new_len;
            }
            src_pos = offs[i] + old_len;
        }
        size_t tail = hlen - src_pos;
        if (tail) {
            memcpy(dst + write_pos, hs + src_pos, tail);
            write_pos += tail;
        }

        dst[write_pos] = '\0';
        r->length = (uint32_t)write_pos;

        if (offs != stack_offs) free(offs);
        return r;
    }

oom:
    if (offs != stack_offs) free(offs);
    return NULL;
}

#undef RA_STACK_CAP

/* -------------------------------------------------------------------------
 * Split & join
 * ---------------------------------------------------------------------- */

cstr** cstr_split(const cstr* s, const char* delim, size_t* count_out) {
    *count_out = 0;

    /* Handle empty or NULL delimiter */
    if (!delim || !*delim) {
        cstr** r = (cstr**)malloc(sizeof(cstr*));
        if (CSTR_UNLIKELY(!r)) return NULL;
        r[0] = cstr_new_len(s->data, s->length);
        if (CSTR_UNLIKELY(!r[0])) {
            free(r);
            return NULL;
        }
        *count_out = 1;
        return r;
    }

    size_t dlen = strlen(delim);
    size_t cap = 8;
    cstr** result = (cstr**)malloc(cap * sizeof(cstr*));
    if (CSTR_UNLIKELY(!result)) return NULL;

    const char* start = s->data;
    size_t rem = s->length;
    size_t count = 0;

    if (dlen == 1) {
        /* Fast path: single-character delimiter using SIMD memchr */
        unsigned char target = (unsigned char)delim[0];
        while (1) {
            const char* match = (const char*)memchr(start, target, rem);
            size_t tok_len = match ? (size_t)(match - start) : rem;

            if (CSTR_UNLIKELY(count >= cap)) {
                if (CSTR_UNLIKELY(cap > SIZE_MAX / 2 / sizeof(cstr*))) goto split_err;
                size_t new_cap = cap * 2;
                cstr** tmp = (cstr**)realloc(result, new_cap * sizeof(cstr*));
                if (CSTR_UNLIKELY(!tmp)) goto split_err;
                result = tmp;
                cap = new_cap;
            }

            result[count] = cstr_new_len(start, tok_len);
            if (CSTR_UNLIKELY(!result[count])) goto split_err;
            count++;

            if (!match) break;
            start = match + 1;
            rem -= (tok_len + 1);
        }
    } else {
        /* Multi-character delimiter path using cstr_search */
        const char* end = s->data + s->length;
        while (1) {
            const char* found = cstr_search(start, (size_t)(end - start), delim, dlen);
            const char* tok_end = found ? found : end;

            if (CSTR_UNLIKELY(count >= cap)) {
                if (CSTR_UNLIKELY(cap > SIZE_MAX / 2 / sizeof(cstr*))) goto split_err;
                size_t new_cap = cap * 2;
                cstr** tmp = (cstr**)realloc(result, new_cap * sizeof(cstr*));
                if (CSTR_UNLIKELY(!tmp)) goto split_err;
                result = tmp;
                cap = new_cap;
            }

            result[count] = cstr_new_len(start, (size_t)(tok_end - start));
            if (CSTR_UNLIKELY(!result[count])) goto split_err;
            count++;

            if (!found) break;
            start = found + dlen;
        }
    }

    *count_out = count;
    return result;

split_err:
    for (size_t i = 0; i < count; i++) {
        cstr_free(result[i]);
    }
    free(result);
    return NULL;
}

cstr* cstr_join(const cstr** strings, size_t count, const char* delim) {
    if (CSTR_UNLIKELY(!strings || count == 0)) return cstr_new_len("", 0);

    /* Single element fast path: no delimiter needed */
    if (count == 1) {
        if (CSTR_UNLIKELY(!strings[0])) return NULL;
        return cstr_new_len(strings[0]->data, strings[0]->length);
    }

    size_t dlen = delim ? strlen(delim) : 0;
    size_t total = 0;

    /* Pass 1: Compute exact size with overflow validation */
    for (size_t i = 0; i < count; i++) {
        if (CSTR_UNLIKELY(!strings[i])) return NULL;

        size_t slen = strings[i]->length;
        if (CSTR_UNLIKELY(slen > CSTR_MAX_SIZE - total)) return NULL; /* Overflow */
        total += slen;

        if (i + 1 < count && dlen) {
            if (CSTR_UNLIKELY(dlen > CSTR_MAX_SIZE - total)) return NULL; /* Overflow */
            total += dlen;
        }
    }

    cstr* r = cstr_init(total);
    if (CSTR_UNLIKELY(!r)) return NULL;

    char* w = r->data;

    /* Write 1st string outside loop to eliminate 'i + 1 < count' check inside loop */
    uint32_t first_len = strings[0]->length;
    if (first_len) {
        memcpy(w, strings[0]->data, first_len);
        w += first_len;
    }

    /* Pass 2: Copy remaining strings */
    if (dlen == 1) {
        /* Specialized fast-path: 1-byte delimiter (direct byte store) */
        char d_char = delim[0];
        for (size_t i = 1; i < count; i++) {
            *w++ = d_char;
            uint32_t slen = strings[i]->length;
            if (slen) {
                memcpy(w, strings[i]->data, slen);
                w += slen;
            }
        }
    } else if (dlen > 1) {
        /* Multi-byte delimiter path */
        for (size_t i = 1; i < count; i++) {
            memcpy(w, delim, dlen);
            w += dlen;
            uint32_t slen = strings[i]->length;
            if (slen) {
                memcpy(w, strings[i]->data, slen);
                w += slen;
            }
        }
    } else {
        /* No delimiter path */
        for (size_t i = 1; i < count; i++) {
            uint32_t slen = strings[i]->length;
            if (slen) {
                memcpy(w, strings[i]->data, slen);
                w += slen;
            }
        }
    }

    *w = '\0';
    r->length = (uint32_t)(w - r->data);
    return r;
}

/* -------------------------------------------------------------------------
 * Reverse
 * ---------------------------------------------------------------------- */

cstr* cstr_reverse(const cstr* s) {
    uint32_t len = s->length;
    cstr* r = cstr_init(len);
    if (!r) return NULL;
    char* dst = r->data;
    const char* src = s->data;
    for (uint32_t i = 0; i < len; i++)
        dst[i] = src[len - 1 - i];
    dst[len] = '\0';
    r->length = len;
    return r;
}

void cstr_reverse_inplace(cstr* s) {
    uint32_t len = s->length;
    if (len < 2) return;
    char* d = s->data;
    for (uint32_t i = 0, j = len - 1; i < j; i++, j--) {
        char t = d[i];
        d[i] = d[j];
        d[j] = t;
    }
}
