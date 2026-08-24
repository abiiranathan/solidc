#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ─── Types ────────────────────────────────────────────────────────────────────

// A non-owning view into a byte sequence.
// The slice does NOT null-terminate and does NOT free its data.
// The caller must ensure the underlying buffer outlives all slices into it.
typedef struct {
    const char* data;  // pointer into some external buffer
    size_t len;        // number of bytes in the view
} StrSlice;

// Typed result — never use errno for slice ops.
typedef enum {
    SS_OK = 0,
    SS_NULL = 1,       // null data pointer
    SS_BOUNDS = 2,     // out-of-range indices
    SS_NOT_FOUND = 3,  // substring not found
    SS_OVERFLOW = 4,   // value exceeds the target type's range
    SS_INVALID = 5,    // malformed input (e.g. "1.2.3", bare "e", "maybe")
} StrSliceErr;

// ─── Construction ─────────────────────────────────────────────────────────────

// Wrap a pointer + explicit length. Does NOT check for null terminator.
static inline StrSlice ss_from(const char* data, size_t len) { return (StrSlice){.data = data, .len = len}; }

// Wrap a null-terminated C string (measures with strlen at call time).
static inline StrSlice ss_from_cstr(const char* cstr) {
    if (!cstr) return (StrSlice){0};
    return (StrSlice){.data = cstr, .len = strlen(cstr)};
}

// Convenience macro for string literals — no strlen call at all.
// Usage:  StrSlice s = SS_LIT("hello");
#define SS_LIT(literal) ((StrSlice){.data = (literal), .len = sizeof(literal) - 1})

// Empty slice (len == 0, data may be NULL).
static inline StrSlice ss_empty(void) { return (StrSlice){0}; }

// Print a slice to stdout (for debugging).
static inline void ss_print(StrSlice s) {
    if (s.data) printf("%.*s", (int)s.len, s.data);
}

static inline void ss_println(StrSlice s) {
    ss_print(s);
    printf("\n");
}

// ─── Validity ─────────────────────────────────────────────────────────────────

// A slice is valid if it has a non-null data pointer or zero length (empty view).
static inline bool ss_is_valid(StrSlice s) {
    // A zero-length slice with a non-null pointer is valid (empty view).
    // A non-zero length with a null pointer is always invalid.
    return s.len == 0 || s.data != NULL;
}

// A slice is empty if its length is zero, regardless of the data pointer.
static inline bool ss_is_empty(StrSlice s) { return s.len == 0; }

// Convert to a NUL-terminated owned string. Caller must free() the result.
// Returns NULL if the slice is invalid (e.g. non-null pointer with positive length).
static inline char* ss_to_owned_cstr(StrSlice s) {
    if (!ss_is_valid(s)) return NULL;
    return strndup(s.data, s.len);
}

// ─── Sub-slicing ──────────────────────────────────────────────────────────────

// Returns a sub-slice [start, start+len).
// Sets *err on bounds violation; returns ss_empty() on error.
static inline StrSlice ss_slice(StrSlice s, size_t start, size_t len, StrSliceErr* err) {
    if (!ss_is_valid(s)) {
        if (err) *err = SS_NULL;
        return ss_empty();
    }
    if (start + len > s.len) {
        if (err) *err = SS_BOUNDS;
        return ss_empty();
    }
    if (err) *err = SS_OK;
    return (StrSlice){.data = s.data + start, .len = len};
}

// Chop off the first `n` bytes.
static inline StrSlice ss_skip(StrSlice s, size_t n) {
    if (n >= s.len) return ss_empty();
    return (StrSlice){.data = s.data + n, .len = s.len - n};
}

// Keep only the first `n` bytes.
static inline StrSlice ss_take(StrSlice s, size_t n) {
    if (n > s.len) n = s.len;
    return (StrSlice){.data = s.data, .len = n};
}

// ─── Comparison ───────────────────────────────────────────────────────────────

static inline bool ss_equal(StrSlice a, StrSlice b) {
    return a.len == b.len && (a.data == b.data || memcmp(a.data, b.data, a.len) == 0);
}

// Case-insensitive ASCII equality.
static inline bool ss_equal_nocase(StrSlice a, StrSlice b) {
    if (a.len != b.len) return false;
    for (size_t i = 0; i < a.len; ++i) {
        unsigned char ca = (unsigned char)a.data[i];
        unsigned char cb = (unsigned char)b.data[i];
        if ((ca | 32u) != (cb | 32u)) return false;
    }
    return true;
}

static inline bool ss_starts_with(StrSlice s, StrSlice prefix) {
    return s.len >= prefix.len && memcmp(s.data, prefix.data, prefix.len) == 0;
}

static inline bool ss_ends_with(StrSlice s, StrSlice suffix) {
    return s.len >= suffix.len && memcmp(s.data + s.len - suffix.len, suffix.data, suffix.len) == 0;
}

// ─── Search ───────────────────────────────────────────────────────────────────

// Returns the byte offset of the first occurrence of `needle`, or (size_t)-1.
static inline size_t ss_find(StrSlice haystack, StrSlice needle) {
    if (needle.len == 0) return 0;
    if (needle.len > haystack.len) return (size_t)-1;
    size_t limit = haystack.len - needle.len;
    for (size_t i = 0; i <= limit; ++i) {
        if (memcmp(haystack.data + i, needle.data, needle.len) == 0) return i;
    }
    return (size_t)-1;
}

static inline bool ss_contains(StrSlice s, StrSlice needle) { return ss_find(s, needle) != (size_t)-1; }

// Split at the first occurrence of `sep`.
// On success: *head = everything before sep, *tail = everything after sep.
// Returns SS_NOT_FOUND (and leaves *head/*tail unchanged) if sep absent.
static inline StrSliceErr ss_split_on(StrSlice s, StrSlice sep, StrSlice* head, StrSlice* tail) {
    size_t pos = ss_find(s, sep);
    if (pos == (size_t)-1) return SS_NOT_FOUND;
    *head = ss_take(s, pos);
    *tail = ss_skip(s, pos + sep.len);
    return SS_OK;
}

// ─── Trimming ─────────────────────────────────────────────────────────────────

static inline bool _ss_is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static inline StrSlice ss_trim(StrSlice s) {
    size_t lo = 0, hi = s.len;
    while (lo < hi && _ss_is_space(s.data[lo])) ++lo;
    while (hi > lo && _ss_is_space(s.data[hi - 1])) --hi;
    return (StrSlice){.data = s.data + lo, .len = hi - lo};
}

// ─── Access ───────────────────────────────────────────────────────────────────

// Safe single-byte fetch. Returns false on out-of-bounds.
static inline bool ss_get(StrSlice s, size_t i, char* out) {
    if (i >= s.len) return false;
    *out = s.data[i];
    return true;
}

/*
 * Parses an optional sign followed by decimal digits.
 * Stops at the first non-digit after the sign.
 *
 * Returns:
 *   SS_OK        — *out is set to the parsed value
 *   SS_NOT_FOUND — no digits found (empty slice, sign with no digits)
 *   SS_OVERFLOW  — value exceeds [INT_MIN, INT_MAX]
 *   SS_NULL      — out is NULL
 *
 * Call ss_trim() beforehand if leading whitespace is possible.
 */
static inline StrSliceErr ss_to_int(StrSlice s, int* out) {
    if (!out) return SS_NULL;

    size_t i = 0;
    bool neg = false;

    if (i < s.len && s.data[i] == '-') {
        neg = true;
        ++i;
    } else if (i < s.len && s.data[i] == '+') {
        ++i;
    }

    if (i >= s.len || s.data[i] < '0' || s.data[i] > '9') return SS_NOT_FOUND;

    // Accumulate into unsigned to avoid signed-overflow UB (C11 §6.5),
    // then range-check before the final cast.
    unsigned int acc = 0;
    for (; i < s.len; ++i) {
        char c = s.data[i];
        if (c < '0' || c > '9') break;
        unsigned int d = (unsigned int)(c - '0');
        // Would acc*10+d wrap past UINT_MAX?
        if (acc > (UINT_MAX - d) / 10u) return SS_OVERFLOW;
        acc = acc * 10u + d;
    }

    if (neg) {
        // INT_MIN = -(INT_MAX + 1); the +1u is safe in unsigned arithmetic.
        if (acc > (unsigned int)INT_MAX + 1u) return SS_OVERFLOW;
        *out = (acc == (unsigned int)INT_MAX + 1u) ? INT_MIN : -(int)acc;
    } else {
        if (acc > (unsigned int)INT_MAX) return SS_OVERFLOW;
        *out = (int)acc;
    }
    return SS_OK;
}

/*
 * Parses:  [sign] digit* ['.' digit*] [('e'|'E') [sign] digit+]
 *
 * Accumulates the mantissa as a 64-bit integer (exact for up to 19 significant
 * digits) then applies the combined decimal exponent in a single step, which
 * avoids the rounding drift that builds up when multiplying by 0.1 per digit.
 *
 * Returns:
 *   SS_OK        — *out is set
 *   SS_NOT_FOUND — no digits found
 *   SS_INVALID   — exponent marker with no digits following ("1e" "1e+")
 *   SS_NULL      — out is NULL
 *
 * Overflow/underflow of the final double maps to ±HUGE_VAL / 0.0 respectively
 * (IEEE 754 behaviour); no SS_OVERFLOW is raised since those are valid doubles.
 */
static inline StrSliceErr ss_to_double(StrSlice s, double* out) {
    if (!out) return SS_NULL;

    size_t i = 0;
    bool neg = false;

    if (i < s.len && s.data[i] == '-') {
        neg = true;
        ++i;
    } else if (i < s.len && s.data[i] == '+') {
        ++i;
    }

    uint64_t mantissa = 0;
    int dec_shift = 0;  // net decimal places (positive = divide)
    bool seen_dot = false;
    bool has_digits = false;
    bool saturated = false;  // mantissa too wide; extra digits are dropped

    for (; i < s.len; ++i) {
        char c = s.data[i];
        if (c >= '0' && c <= '9') {
            has_digits = true;
            if (!saturated) {
                uint64_t d = (uint64_t)(c - '0');
                if (mantissa > (UINT64_MAX - d) / 10ull) {
                    // Mantissa full.  Integer digits still shift the scale;
                    // fractional digits beyond this point are simply dropped.
                    saturated = true;
                    if (!seen_dot) ++dec_shift;
                } else {
                    mantissa = mantissa * 10ull + d;
                    if (seen_dot) --dec_shift;
                }
            } else if (!seen_dot) {
                ++dec_shift;  // track magnitude of overflowing integer part
            }
        } else if (c == '.' && !seen_dot) {
            seen_dot = true;
        } else {
            break;
        }
    }

    if (!has_digits) return SS_NOT_FOUND;

    // Optional exponent.
    int exp_shift = 0;
    if (i < s.len && (s.data[i] == 'e' || s.data[i] == 'E')) {
        ++i;
        bool exp_neg = false;
        if (i < s.len && s.data[i] == '-') {
            exp_neg = true;
            ++i;
        } else if (i < s.len && s.data[i] == '+') {
            ++i;
        }

        // Exponent marker with no digits is malformed.
        if (i >= s.len || s.data[i] < '0' || s.data[i] > '9') return SS_INVALID;

        for (; i < s.len && s.data[i] >= '0' && s.data[i] <= '9'; ++i) {
            if (exp_shift < 100000)  // cap before int overflow; range check below
                exp_shift = exp_shift * 10 + (s.data[i] - '0');
        }
        if (exp_neg) exp_shift = -exp_shift;
    }

    int total_exp = dec_shift + exp_shift;

    // Build result = mantissa × 10^total_exp.
    // Powers up to ±22 are exact in IEEE 754 double; beyond that we iterate.
    // The range of finite doubles is roughly 10^±308, so cap the loop.
    static const double _p10[23] = {
        1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
        1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22,
    };
    double result = (double)mantissa;
    if (total_exp != 0) {
        int abs_exp = total_exp < 0 ? -total_exp : total_exp;
        if (abs_exp > 308) abs_exp = 308;  // clamp; IEEE will give ±inf / 0
        double scale = (abs_exp <= 22) ? _p10[abs_exp] : ({
            double str = 1.0;
            for (int j = 0; j < abs_exp; ++j) str *= 10.0;
            str;
        });
        result = (total_exp < 0) ? result / scale : result * scale;
    }

    *out = neg ? -result : result;
    return SS_OK;
}

/*
 * Recognises the common human-readable boolean vocabulary:
 *
 *   true  : "true", "yes", "on",  "1"
 *   false : "false", "no",  "off", "0"
 *
 * All string forms are matched case-insensitively.
 * Anything else returns SS_INVALID — the caller knows the input was garbage.
 */
static inline StrSliceErr ss_to_bool(StrSlice s, bool* out) {
    if (!out) return SS_NULL;

    if (ss_equal_nocase(s, SS_LIT("true")) || ss_equal_nocase(s, SS_LIT("yes")) || ss_equal_nocase(s, SS_LIT("on")) ||
        ss_equal(s, SS_LIT("1"))) {
        *out = true;
        return SS_OK;
    }

    if (ss_equal_nocase(s, SS_LIT("false")) || ss_equal_nocase(s, SS_LIT("no")) || ss_equal_nocase(s, SS_LIT("off")) ||
        ss_equal(s, SS_LIT("0"))) {
        *out = false;
        return SS_OK;
    }
    return SS_INVALID;
}

#if defined(__cplusplus)
}
#endif