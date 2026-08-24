#include "../include/hash.h"

#include <stddef.h>
#include <string.h>

#ifndef _WIN32
#include <pthread.h>
#endif

/*
 * All string-hash functions treat a NULL key uniformly: they return 0
 * instead of dereferencing it.  Hashing NULL is almost always a caller
 * bug, but crashing a library over an empty bucket key is worse than
 * returning a well-defined (and documented) value.
 */

/**
 * DJB2 hash function implementation.
 * Simple and effective hash created by Daniel J. Bernstein.
 */
uint32_t solidc_djb2_hash(const void* key) {
    if (!key) return 0;
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
    if (!key) return 0;
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
    if (!key) return 0;
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
    if (!key) return 0;
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
    if (!key) return 0;
    const unsigned char* str = (const unsigned char*)key;
    uint32_t hash = 0;
    uint32_t x = 0;

    while (*str) {
        hash = (hash << 4) + (*str++);
        x = hash & 0xF0000000UL;
        if (x != 0) { hash ^= (x >> 24); }
        hash &= ~x;
    }

    return hash;
}

/**
 * SDBM hash function implementation.
 * Used in SDBM database library.
 */
uint32_t solidc_sdbm_hash(const void* key) {
    if (!key) return 0;
    const unsigned char* str = (const unsigned char*)key;
    uint32_t hash = 0;
    int c;

    while ((c = *str++) != '\0') {
        hash = (unsigned char)c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

/* ---------------------------------------------------------------------------
 * CRC32 (IEEE 802.3, polynomial 0xEDB88320 reflected), slice-by-eight.
 *
 * The previous implementation ran the bitwise loop for every input byte,
 * which costs ~8 shift/xor steps per byte.  Slice-by-eight precomputes
 * 8 tables so the inner loop consumes 8 bytes with 8 table lookups,
 * producing bit-identical results roughly an order of magnitude faster.
 *
 * Tables are initialized exactly once, thread-safely:
 *   - POSIX:   pthread_once
 *   - Windows: InitOnceExecuteOnce
 * ------------------------------------------------------------------------- */
static uint32_t crc_slice8_table[8][256];

static void crc32_build_tables(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int k = 0; k < 8; k++) {
            /* (-crc & 1) avoids implementation-defined int->uint conversions. */
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
        crc_slice8_table[0][i] = crc;
    }
    for (int t = 1; t < 8; t++) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t prev = crc_slice8_table[t - 1][i];
            crc_slice8_table[t][i] = (prev >> 8) ^ crc_slice8_table[0][prev & 0xFF];
        }
    }
}

#ifdef _WIN32
static INIT_ONCE crc_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK crc_once_cb(PINIT_ONCE once, PVOID param, PVOID* ctx) {
    (void)once;
    (void)param;
    (void)ctx;
    crc32_build_tables();
    return TRUE;
}
#else
static pthread_once_t crc_once = PTHREAD_ONCE_INIT;

static void crc_once_cb(void) { crc32_build_tables(); }
#endif

static inline void crc32_init_once(void) {
#ifdef _WIN32
    InitOnceExecuteOnce(&crc_once, crc_once_cb, NULL, NULL);
#else
    pthread_once(&crc_once, crc_once_cb);
#endif
}

/**
 * CRC32 hash function implementation.
 * Standard CRC32 with polynomial 0xEDB88320, computed with the
 * slice-by-eight technique (bit-identical to the naive bitwise loop).
 */
uint32_t solidc_crc32_hash(const void* key, size_t len) {
    if (!key && len > 0) return 0;

    crc32_init_once();

    const unsigned char* data = (const unsigned char*)key;
    uint32_t crc = 0xFFFFFFFFu;

    /* Bulk: consume 8 bytes per iteration. */
    while (len >= 8) {
        uint32_t lo, hi;
        memcpy(&lo, data, 4);
        memcpy(&hi, data + 4, 4);
        lo ^= crc;

        crc = crc_slice8_table[7][lo & 0xFF] ^ crc_slice8_table[6][(lo >> 8) & 0xFF] ^
              crc_slice8_table[5][(lo >> 16) & 0xFF] ^ crc_slice8_table[4][(lo >> 24) & 0xFF] ^
              crc_slice8_table[3][hi & 0xFF] ^ crc_slice8_table[2][(hi >> 8) & 0xFF] ^
              crc_slice8_table[1][(hi >> 16) & 0xFF] ^ crc_slice8_table[0][(hi >> 24) & 0xFF];

        data += 8;
        len -= 8;
    }

    /* Tail: remaining bytes through the first table. */
    while (len--) {
        crc = (crc >> 8) ^ crc_slice8_table[0][(crc ^ *data++) & 0xFF];
    }

    return ~crc;
}

/**
 * MurmurHash3 32-bit implementation.
 * Excellent general-purpose hash with good distribution.
 *
 * Block loads go through memcpy() so they are alignment- and
 * strict-aliasing-safe; every mainstream compiler folds them into a
 * single unaligned load instruction.
 */
uint32_t solidc_murmur_hash(const char* key, uint32_t len, uint32_t seed) {
    if (!key && len > 0) return 0;

    const uint8_t* data = (const uint8_t*)key;
    const uint32_t nblocks = len / 4;

    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // Body: process 4-byte blocks
    for (uint32_t i = 0; i < nblocks; i++) {
        uint32_t k1;
        memcpy(&k1, data + i * 4, sizeof(k1));

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> (32 - 15));
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> (32 - 13));
        h1 = h1 * 5 + 0xe6546b64;
    }

    // Tail: process remaining bytes
    const uint8_t* tail = data + nblocks * 4;
    uint32_t k1 = 0;

    switch (len & 3) {
        case 3:
            k1 ^= (uint32_t)tail[2] << 16u;
            // fallthrough
        case 2:
            k1 ^= (uint32_t)tail[1] << 8u;
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
