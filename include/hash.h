/**
 * @file hash.h
 * @brief Collection of fast, non-cryptographic hash functions including MurmurHash, xxHash, and others.
 */

#ifndef A02E572A_DD85_4D77_AC81_41037EDE290A
#define A02E572A_DD85_4D77_AC81_41037EDE290A

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t solidc_djb2_hash(const void* key);
uint32_t solidc_sdbm_hash(const void* key);
uint32_t solidc_fnv1a_hash(const void* key);
uint32_t solidc_elf_hash(const void* key);
uint32_t solidc_djb2a_hash(const void* key);

// CRC32 hash function
uint32_t solidc_crc32_hash(const void* key, size_t len);

// Murmur hash function.
// See https://en.wikipedia.org/wiki/MurmurHash for more details.
uint32_t solidc_murmur_hash(const char* key, uint32_t len, uint32_t seed);

// xxHash is an Extremely fast Hash algorithm, processing at RAM speed
// limits. For a faster implementation see, https://github.com/Cyan4973/xxHash
// on Github. Code is highly portable, and produces hashes identical across
// all platforms (little / big endian)
uint32_t solidc_XXH32(const void* input, size_t len, uint32_t seed);

#if defined(__cplusplus)
}
#endif

#endif /* A02E572A_DD85_4D77_AC81_41037EDE290A */
