#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
 * hash_micro.c — micro-benchmarks for the solidc hash module.
 *
 * Scenarios (report MB/s and ns/op):
 *   crc32-small  — CRC32 of 64-byte inputs (dominant size for hashes of
 *                  short keys / small messages).
 *   crc32-large  — CRC32 of 64 KiB blocks (bulk checksum throughput).
 *   murmur       — MurmurHash3 x86_32 on 64-byte inputs.
 *   xxh32        — xxHash32 on 64-byte inputs (reference fast path).
 *   fnv1a/djb2   — string hashes over a key set (hash-table workloads).
 *
 * The CRC32 numbers specifically track the slice-by-eight rewrite: the
 * bitwise reference below is compiled in as well so the speedup is
 * visible on the same binary.
 */

#include "../include/hash.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Bitwise CRC32 reference (the pre-optimization implementation). */
static uint32_t crc32_bitwise(const void* key, size_t len) {
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

typedef uint32_t (*crc_fn)(const void*, size_t);

static void bench_crc(const char* name, crc_fn fn, const unsigned char* buf, size_t len, int iters) {
    uint32_t sink = 0;
    uint64_t t0 = ns_now();
    for (int i = 0; i < iters; i++) {
        sink ^= fn(buf, len);
    }
    uint64_t dt = ns_now() - t0;
    double mbps = (double)iters * len / (double)dt * 1000.0;
    printf("%-12s %8zu B  %10u it  %8.1f ns/op  %9.1f MB/s  (sink %08x)\n", name, len, iters, (double)dt / iters,
           mbps, sink);
}

int main(void) {
    size_t LARGE = 65536;
    unsigned char* buf = malloc(LARGE);
    if (!buf) return 1;
    for (size_t i = 0; i < LARGE; i++) { buf[i] = (unsigned char)(i * 31 + 7); }

    /* Correctness cross-check first: the fast path must be bit-identical. */
    if (solidc_crc32_hash(buf, 1234) != crc32_bitwise(buf, 1234)) {
        fprintf(stderr, "FATAL: crc32 mismatch\n");
        return 1;
    }

    puts("== CRC32: slice-by-eight vs bitwise reference ==");
    bench_crc("crc32-slice8", solidc_crc32_hash, buf, 64, 2000000);
    bench_crc("crc32-bitwise", crc32_bitwise, buf, 64, 200000);
    bench_crc("crc32-slice8", solidc_crc32_hash, buf, LARGE, 5000);
    bench_crc("crc32-bitwise", crc32_bitwise, buf, LARGE, 300);

    puts("== Other 32-bit hashes on 64-byte inputs ==");
    uint32_t sink = 0;
    uint64_t t0 = ns_now();
    for (int i = 0; i < 2000000; i++) {
        buf[0] = (unsigned char)i;
        sink ^= solidc_murmur_hash((const char*)buf, 64, 0);
    }
    printf("%-12s %8zu B  %10u it  %8.1f ns/op\n", "murmur3", (size_t)64, 2000000,
           (double)(ns_now() - t0) / 2000000);

    t0 = ns_now();
    for (int i = 0; i < 2000000; i++) {
        buf[0] = (unsigned char)i;
        sink ^= solidc_XXH32(buf, 64, 0);
    }
    printf("%-12s %8zu B  %10u it  %8.1f ns/op  (sink %08x)\n", "xxh32", (size_t)64, 2000000,
           (double)(ns_now() - t0) / 2000000, sink);

    /* String-key workload: short keys, like a hash table would feed us. */
    const char* words[] = {"alpha",  "bravo",   "charlie", "delta",  "echo",
                           "foxtrot", "golf",    "hotel",   "india",  "juliett"};
    enum { WKEYS = 100000 };
    puts("== String hashes over short keys ==");
    t0 = ns_now();
    uint64_t acc = 0;
    for (int i = 0; i < WKEYS; i++) {
        acc += solidc_djb2_hash(words[i & 7]);
        acc += solidc_fnv1a_hash(words[(i >> 1) & 7]);
    }
    double per_key = (double)(ns_now() - t0) / (double)WKEYS / 2.0;
    printf("djb2+fnv1a     avg %8.1f ns/key-pair  (acc %llu)\n", per_key, (unsigned long long)acc);
    (void)sink;
    free(buf);
    return 0;
}
