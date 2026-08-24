#include "../include/hash.h"
#include "../include/macros.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void test_hash_function(const char* name, uint32_t (*hash_func)(const void*), const char* input, uint32_t expected) {
    uint32_t result = hash_func(input);
    printf("Testing %s, result: %u, expected: %u\n", name, result, expected);
    ASSERT_EQ(result, expected);
}

void test_hash_function_with_length(const char* name, uint32_t (*hash_func)(const void*, size_t), const char* input,
                                    uint32_t expected) {
    // strlen returns size_t, which matches the function pointer signature
    uint32_t result = hash_func(input, strlen(input));
    printf("Testing %s, result: %u, expected: %u\n", name, result, expected);
    assert(result == expected);
}

uint32_t murmur_hash_wrapper(const void* key) {
    // 1. Explicit cast for size_t -> uint32_t (narrowing, but intentional)
    // 2. Used '0u' for the seed to match uint32_t param
    return solidc_murmur_hash(key, (uint32_t)strlen(key), 0u);
}

uint32_t XXH32Wrapper(const void* key) {
    return solidc_XXH32(key, strlen(key), 0u);
}

/* NULL keys must return 0 for every hash function, never crash. */
void test_null_safety(void) {
    ASSERT_EQ(solidc_djb2_hash(NULL), 0u);
    ASSERT_EQ(solidc_djb2a_hash(NULL), 0u);
    ASSERT_EQ(solidc_sdbm_hash(NULL), 0u);
    ASSERT_EQ(solidc_fnv1a_hash(NULL), 0u);
    ASSERT_EQ(solidc_fnv1a_hash64(NULL), 0ULL);
    ASSERT_EQ(solidc_elf_hash(NULL), 0u);
    ASSERT_EQ(solidc_crc32_hash(NULL, 10), 0u);   /* NULL with len: defined */
    ASSERT_EQ(solidc_crc32_hash(NULL, 0), 0u);    /* degenerate */
    ASSERT_EQ(solidc_murmur_hash(NULL, 4, 0), 0u);
    /* zero-length hashes of a non-NULL pointer are well-defined too */
    const char* s = "x";
    ASSERT_EQ(solidc_crc32_hash(s, 0), 0u);       /* CRC of empty is 0 */
    ASSERT_EQ(solidc_murmur_hash(s, 0, 0), 0u);
}

/* Standard CRC-32 (IEEE) known-answer vectors. */
void test_crc32_known_answers(void) {
    /* "" -> 0x00000000 */
    ASSERT_EQ(solidc_crc32_hash("", 0), 0x00000000u);
    /* "a" -> 0xE8B7BE43 */
    ASSERT_EQ(solidc_crc32_hash("a", 1), 0xE8B7BE43u);
    /* "abc" -> 0x352441C2 */
    ASSERT_EQ(solidc_crc32_hash("abc", 3), 0x352441C2u);
    /* "123456789" -> 0xCBF43926 (classic check value) */
    ASSERT_EQ(solidc_crc32_hash("123456789", 9), 0xCBF43926u);
    /* "The quick brown fox jumps over the lazy dog" -> 0x414FA339 */
    const char* fox = "The quick brown fox jumps over the lazy dog";
    ASSERT_EQ(solidc_crc32_hash(fox, strlen(fox)), 0x414FA339u);
}

/* Bitwise reference implementation: the slice-by-eight fast path must
 * produce bit-identical output across lengths that hit every code path
 * (bulk loop, all tail remainders). */
static uint32_t crc32_bitwise_ref(const void* key, size_t len) {
    const unsigned char* data = (const unsigned char*)key;
    uint32_t crc = 0xFFFFFFFFu;
    while (len--) {
        crc ^= *data++;
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void test_crc32_matches_bitwise_reference(void) {
    enum { MAXLEN = 300 };
    static unsigned char buf[MAXLEN];
    for (size_t i = 0; i < sizeof(buf); i++) {
        buf[i] = (unsigned char)(i * 131 + 7); /* deterministic pseudo-data */
    }

    for (size_t len = 0; len <= MAXLEN; len++) {
        uint32_t fast = solidc_crc32_hash(buf, len);
        uint32_t ref = crc32_bitwise_ref(buf, len);
        if (fast != ref) {
            fprintf(stderr, "CRC mismatch at len=%zu: fast=%08x ref=%08x\n", len, fast, ref);
            ASSERT(fast == ref);
        }
    }
}

/* MurmurHash3 must be correct for buffers at every alignment offset,
 * since block loads previously cast to uint32_t* (UB on unaligned data). */
void test_murmur_unaligned(void) {
    unsigned char pattern[16];
    for (size_t i = 0; i < sizeof(pattern); i++) pattern[i] = (unsigned char)(i + 1);
    unsigned char big[64];
    for (size_t i = 0; i < sizeof(big); i++) big[i] = pattern[i % sizeof(pattern)];

    /* Real unaligned check: hash sub-windows whose starts differ mod 4.
     * The unaligned in-place read must agree with hashing the identical
     * bytes copied into a fresh (aligned) object. */
    for (int off = 0; off < 8; off++) {
        unsigned char window[32];
        memcpy(window, big + off, 32);
        uint32_t h_aligned_obj = solidc_murmur_hash((const char*)window, 32, 5u);
        uint32_t h_unaligned = solidc_murmur_hash((const char*)(big + off), 32, 5u);
        ASSERT_EQ(h_unaligned, h_aligned_obj);
    }

    /* seed variation changes the hash but stays stable */
    uint32_t s0 = solidc_murmur_hash((const char*)big, 60, 0u);
    uint32_t s1 = solidc_murmur_hash((const char*)big, 60, 0u);
    uint32_t s2 = solidc_murmur_hash((const char*)big, 60, 1u);
    ASSERT_EQ(s0, s1);
    ASSERT(s0 != s2);

    /* tail lengths 1..3 exercised */
    for (uint32_t len = 1; len <= 11; len++) {
        uint32_t h = solidc_murmur_hash((const char*)big, len, 7u);
        (void)h; /* must not crash or read out of bounds (checked by ASan in CI) */
    }
}

/* CRC-32C (Castagnoli) known-answer vectors.  Distinct polynomial from
 * the IEEE CRC32 above — deliberately different digests. */
void test_crc32c_known_answers(void) {
    ASSERT_EQ(solidc_crc32c_hash("", 0), 0x00000000u);
    /* "123456789" -> 0xE3069283 (CRC-32C check value) */
    ASSERT_EQ(solidc_crc32c_hash("123456789", 9), 0xE3069283u);
    /* "a" -> 0xC1D04330 */
    ASSERT_EQ(solidc_crc32c_hash("a", 1), 0xC1D04330u);
    /* "hello" -> 0x9A71BB4C */
    ASSERT_EQ(solidc_crc32c_hash("hello", 5), 0x9A71BB4Cu);
    /* must differ from the IEEE digest of the same bytes */
    ASSERT(solidc_crc32c_hash("123456789", 9) != solidc_crc32_hash("123456789", 9));

    /* long buffer: hw and table paths (when both exist) must agree with
     * themselves across repeated calls, and NULL policy holds */
    static unsigned char buf[1000];
    for (size_t i = 0; i < sizeof(buf); i++) { buf[i] = (unsigned char)(i * 17 + 3); }
    ASSERT_EQ(solidc_crc32c_hash(buf, sizeof(buf)), solidc_crc32c_hash(buf, sizeof(buf)));
    ASSERT_EQ(solidc_crc32c_hash(NULL, 5), 0u);
}

/* The slice-by-sixteen rewrite must stay bit-identical to the bitwise
 * reference (guards the 16-byte bulk path added over slice-by-eight). */
void test_crc32_slice16_still_matches(void) {
    enum { MAXLEN = 300 };
    static unsigned char buf[MAXLEN];
    for (size_t i = 0; i < sizeof(buf); i++) {
        buf[i] = (unsigned char)(i * 131 + 7);
    }
    for (size_t len = 0; len <= MAXLEN; len++) {
        ASSERT_EQ(solidc_crc32_hash(buf, len), crc32_bitwise_ref(buf, len));
    }
}

int main() {
    // Test cases
    // Added 'u' suffix to all expected values to prevent signed-int interpretation
    test_hash_function("djb2", solidc_djb2_hash, "hello", 261238937u);
    test_hash_function("sdbm", solidc_sdbm_hash, "hello", 684824882u);
    test_hash_function("fnv1a", solidc_fnv1a_hash, "hello", 1335831723u);
    test_hash_function("elf", solidc_elf_hash, "hello", 7258927u);
    test_hash_function("djb2a", solidc_djb2a_hash, "hello", 178056679u);

    test_hash_function_with_length("crc32", solidc_crc32_hash, "hello", 907060870u);

    // Especially important here: 3067714808 does not fit in a standard signed 32-bit int
    test_hash_function("murmur", murmur_hash_wrapper, "kinkajou", 3067714808u);

    test_null_safety();
    printf("test_null_safety passed\n");

    test_crc32_known_answers();
    printf("test_crc32_known_answers passed\n");

    test_crc32_matches_bitwise_reference();
    printf("test_crc32_matches_bitwise_reference passed\n");

    test_crc32_slice16_still_matches();
    printf("test_crc32_slice16_still_matches passed\n");

    test_crc32c_known_answers();
    printf("test_crc32c_known_answers passed\n");

    test_murmur_unaligned();
    printf("test_murmur_unaligned passed\n");

    return 0;
}
