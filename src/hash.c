#include "../include/hash.h"

#include <stddef.h>
#include <string.h>

/**
 * DJB2 hash function implementation.
 * Simple and effective hash created by Daniel J. Bernstein.
 */
uint32_t solidc_djb2_hash(const void* key) {
    const unsigned char* str = (const unsigned char*)key;
    uint32_t hash = 5381;
    int c;

    while ((c = *str++) != '\0') {
        hash = ((hash << 5) + hash) + (uint32_t)c;  // hash * 33 + c
    }

    return hash;
}

/**
 * DJB2A hash function implementation (XOR variant).
 * Uses XOR instead of addition for mixing.
 */
uint32_t solidc_djb2a_hash(const void* key) {
    const unsigned char* str = (const unsigned char*)key;
    uint32_t hash = 5381;
    int c;

    while ((c = *str++) != '\0') {
        hash = ((hash << 5) + hash) ^ (unsigned)c;  // hash * 33 ^ c
    }

    return hash;
}

/**
 * FNV-1a 32-bit hash function implementation.
 * Fowler-Noll-Vo hash with good distribution.
 */
uint32_t solidc_fnv1a_hash(const void* key) {
    const unsigned char* str = (const unsigned char*)key;
    uint32_t hash = 2166136261u;  // FNV offset basis

    while (*str) {
        hash ^= (uint32_t)(*str++);
        hash *= 16777619u;  // FNV prime
    }

    return hash;
}

/**
 * FNV-1a 64-bit hash function implementation.
 * 64-bit version providing larger hash space.
 */
uint64_t solidc_fnv1a_hash64(const void* key) {
    const unsigned char* str = (const unsigned char*)key;
    uint64_t hash = 14695981039346656037ULL;  // FNV offset basis

    while (*str) {
        hash ^= (uint64_t)(*str++);
        hash *= 1099511628211ULL;  // FNV prime
    }

    return hash;
}

/**
 * ELF hash function implementation.
 * Used in ELF object file format.
 */
uint32_t solidc_elf_hash(const void* key) {
    const unsigned char* str = (const unsigned char*)key;
    uint32_t hash = 0;
    uint32_t x = 0;

    while (*str) {
        hash = (hash << 4) + (*str++);
        x = hash & 0xF0000000UL;
        if (x != 0) {
            hash ^= (x >> 24);
        }
        hash &= ~x;
    }

    return hash;
}

/**
 * SDBM hash function implementation.
 * Used in SDBM database library.
 */
uint32_t solidc_sdbm_hash(const void* key) {
    const unsigned char* str = (const unsigned char*)key;
    uint32_t hash = 0;
    int c;

    while ((c = *str++) != '\0') {
        hash = (unsigned char)c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

/**
 * CRC32 hash function implementation.
 * Standard CRC32 with polynomial 0xEDB88320.
 */
uint32_t solidc_crc32_hash(const void* key, size_t len) {
    const unsigned char* data = (const unsigned char*)key;
    uint32_t crc = 0xFFFFFFFF;

    while (len--) {
        crc ^= *data++;
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }

    return ~crc;
}

/**
 * MurmurHash3 32-bit implementation.
 * Excellent general-purpose hash with good distribution.
 */
uint32_t solidc_murmur_hash(const char* key, uint32_t len, uint32_t seed) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 4;

    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // Body: process 4-byte blocks
    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);

    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> (32 - 15));
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> (32 - 13));
        h1 = h1 * 5 + 0xe6546b64;
    }

    // Tail: process remaining bytes
    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;

    switch (len & 3) {
        case 3:
            k1 ^= (uint32_t)(tail[2] << 16u);
            // fallthrough
        case 2:
            k1 ^= (uint32_t)(tail[1] << 8u);
            // fallthrough
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = (k1 << 15) | (k1 >> (32 - 15));
            k1 *= c2;
            h1 ^= k1;
    }

    // Finalization
    h1 ^= len;

    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}
