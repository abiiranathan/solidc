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

    return 0;
}
