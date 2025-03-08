#include "../include/hash.h"
#include <stddef.h>
#include <string.h>

#undef get16bits
#define get16bits(d) (*((const unsigned short*)(d)))

// DJB2 Has function.
// http://www.cse.yorku.ca/~oz/hash.html
uint32_t solidc_djb2_hash(const void* key) {
    unsigned char* str = (unsigned char*)key;
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) != '\0') {
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    }
    return hash;
}

// FNV-1a hash function
uint32_t solidc_fnv1a_hash(const void* key) {
    unsigned char* str = (unsigned char*)key;
    unsigned long hash = 14695981039346656037UL;
    while (*str) {
        hash ^= (unsigned long)(unsigned char)*str++;
        hash *= 1099511628211UL;
    }
    return hash;
}

uint32_t solidc_elf_hash(const void* key) {
    const unsigned char* str = (const unsigned char*)key;
    uint32_t hash            = 0;
    uint32_t x;

    while (*str) {
        hash = (hash << 4) + *str++;
        x    = hash & 0xF0000000UL;
        if (x != 0) {
            hash ^= (x >> 24);
        }
        hash &= ~x;
    }
    return hash;
}

// DJB2A hash function
uint32_t solidc_djb2a_hash(const void* key) {
    unsigned char* str = (unsigned char*)key;
    unsigned long hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) ^ *str++;
    }
    return hash;
}

// SDBM hash function
uint32_t solidc_sdbm_hash(const void* key) {
    unsigned char* str = (unsigned char*)key;
    unsigned long hash = 0;
    while (*str) {
        hash = *str++ + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

// CRC32 hash function
uint32_t solidc_crc32_hash(const void* key, size_t len) {
    unsigned char* data = (unsigned char*)key;
    unsigned long crc   = 0xFFFFFFFF;
    while (len--) {
        crc ^= *data++;
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

// https://en.wikipedia.org/wiki/MurmurHash
uint32_t solidc_murmur_hash(const char* key, uint32_t len, uint32_t seed) {
    uint32_t c1            = 0xcc9e2d51;
    uint32_t c2            = 0x1b873593;
    uint32_t r1            = 15;
    uint32_t r2            = 13;
    uint32_t m             = 5;
    uint32_t n             = 0xe6546b64;
    uint32_t h             = 0;
    uint32_t k             = 0;
    uint8_t* d             = (uint8_t*)key;  // 32 bit extract from `key'
    const uint32_t* chunks = NULL;
    const uint8_t* tail    = NULL;  // tail - last 8 bytes
    int i                  = 0;
    uint32_t l             = len / 4;  // chunk length

    h = seed;

    chunks = (const uint32_t*)(d + l * 4);  // body
    tail   = (const uint8_t*)(d + l * 4);   // last 8 byte chunk of `key'

    // for each 4 byte chunk of `key'
    for (i = -l; i != 0; ++i) {
        // next 4 byte chunk of `key'
        k = chunks[i];

        // encode next 4 byte chunk of `key'
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;

        // append to hash
        h ^= k;
        h = (h << r2) | (h >> (32 - r2));
        h = h * m + n;
    }

    k = 0;

    // tail - last 8 byte
    switch (len & 3) {  // `len % 4'
        case 3:
            k ^= (tail[2] << 16);
            // fall through
        case 2:
            k ^= (tail[1] << 8);
            // fall through

        case 1:
            k ^= tail[0];
            k *= c1;
            k = (k << r1) | (k >> (32 - r1));
            k *= c2;
            h ^= k;
    }

    h ^= len;

    h ^= (h >> 16);
    h *= 0x85ebca6b;
    h ^= (h >> 13);
    h *= 0xc2b2ae35;
    h ^= (h >> 16);

    return h;
}

// ================== XXH32 Hash function ==================
#define XXH_PRIME32_1 2654435761U
#define XXH_PRIME32_2 2246822519U
#define XXH_PRIME32_3 3266489917U
#define XXH_PRIME32_4 668265263U
#define XXH_PRIME32_5 374761393U

static uint32_t XXH_rotl32(uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}

static uint32_t XXH_readLE32(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return ((uint32_t)p[0] << 0) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t XXH32_round(uint32_t seed, uint32_t input) {
    seed += input * XXH_PRIME32_2;
    seed = XXH_rotl32(seed, 13);
    seed *= XXH_PRIME32_1;
    return seed;
}

// https://github.com/Cyan4973/xxHash
uint32_t solidc_XXH32(const void* input, size_t len, uint32_t seed) {
    const uint8_t* p          = (const uint8_t*)input;
    const uint8_t* const bEnd = p + len;
    uint32_t h32;

    if (len >= 16) {
        const uint8_t* const limit = bEnd - 16;
        uint32_t v1                = seed + XXH_PRIME32_1 + XXH_PRIME32_2;
        uint32_t v2                = seed + XXH_PRIME32_2;
        uint32_t v3                = seed + 0;
        uint32_t v4                = seed - XXH_PRIME32_1;

        do {
            v1 = XXH32_round(v1, XXH_readLE32(p));
            p += 4;
            v2 = XXH32_round(v2, XXH_readLE32(p));
            p += 4;
            v3 = XXH32_round(v3, XXH_readLE32(p));
            p += 4;
            v4 = XXH32_round(v4, XXH_readLE32(p));
            p += 4;
        } while (p <= limit);

        h32 = XXH_rotl32(v1, 1) + XXH_rotl32(v2, 7) + XXH_rotl32(v3, 12) + XXH_rotl32(v4, 18);
    } else {
        h32 = seed + XXH_PRIME32_5;
    }

    h32 += (uint32_t)len;

    while (p + 4 <= bEnd) {
        h32 += XXH_readLE32(p) * XXH_PRIME32_3;
        h32 = XXH_rotl32(h32, 17) * XXH_PRIME32_4;
        p += 4;
    }

    while (p < bEnd) {
        h32 += (*p++) * XXH_PRIME32_5;
        h32 = XXH_rotl32(h32, 11) * XXH_PRIME32_1;
    }

    h32 ^= h32 >> 15;
    h32 *= XXH_PRIME32_2;
    h32 ^= h32 >> 13;
    h32 *= XXH_PRIME32_3;
    h32 ^= h32 >> 16;

    return h32;
}
