#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/hash.h"

void test_hash_function(const char* name, uint32_t (*hash_func)(const void*), const char* input,
                        uint32_t expected) {
    uint32_t result = hash_func(input);
    printf("Testing %s, result: %u, expected: %u\n", name, result, expected);
    assert(result == expected);
}

void test_hash_function_with_length(const char* name, uint32_t (*hash_func)(const void*, size_t),
                                    const char* input, uint32_t expected) {
    uint32_t result = hash_func(input, strlen(input));
    printf("Testing %s, result: %u, expected: %u\n", name, result, expected);
    assert(result == expected);
}

uint32_t murmur_hash_wrapper(const void* key) {
    return solidc_murmur_hash(key, strlen(key), 0);
}

uint32_t XXH32Wrapper(const void* key) {
    return solidc_XXH32(key, strlen(key), 0);
}

int main() {
    // Test cases
    test_hash_function("djb2", solidc_djb2_hash, "hello", 261238937);
    test_hash_function("sdbm", solidc_sdbm_hash, "hello", 684824882);
    test_hash_function("fnv1a", solidc_fnv1a_hash, "hello", 2158673163);
    test_hash_function("elf", solidc_elf_hash, "hello", 7258927);
    test_hash_function("djb2a", solidc_djb2a_hash, "hello", 178056679);

    test_hash_function_with_length("crc32", solidc_crc32_hash, "hello", 907060870);  // done
    test_hash_function("murmur", murmur_hash_wrapper, "kinkajou", 3067714808);       // done
    // test_hash_function("xxhash", XXH32Wrapper, "hello", 4211111929);  // done

    printf("All tests passed!\n");
    return 0;
}
