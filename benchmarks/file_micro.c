#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*
 * file_micro.c — bandwidth and latency probes for the file/filepath libs.
 *
 * Scenarios:
 *   readall   — file_readall() over a temp-file of SIZE bytes.
 *               Expected regime: bounded by first-touch page faults of the
 *               destination buffer (~2 GB/s for 128 MB on tmpfs), NOT by
 *               the read itself (raw read() reaches ~5-6 GB/s).  Compare
 *               against `raw read()` numbers when tuning.
 *   copy      — file_copy() src->dst.  On Linux this exercises the
 *               copy_file_range(2) kernel path; on tmpfs it is kernel-memcpy
 *               bound, on reflink-capable filesystems it is near-instant.
 *   strpath   — filepath_basename/extension/join ns-level string costs.
 *   dirlist   — dir_list() over generated directories (allocation-heavy).
 *
 * Everything runs against files under $TMPDIR/file_micro (created and
 * cleaned up by the harness).
 */

#include "../include/file.h"
#include "../include/filepath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BENCH_SIZE (128u << 20) /* 128 MB */

static uint64_t ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static double gb_s(size_t bytes, uint64_t d) { return (double)bytes / 1073741824.0 / ((double)d / 1e9); }

int main(void) {
    const char* dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";

    char base[512], src[560], dst[560];
    snprintf(base, sizeof base, "%s/file_micro", dir);
    snprintf(src, sizeof src, "%s/src.bin", base);
    snprintf(dst, sizeof dst, "%s/dst.bin", base);

    char cmd[640];
    snprintf(cmd, sizeof cmd, "mkdir -p %s && rm -rf %s/*", base, base);
    if (system(cmd) != 0) return 1;

    printf("File/Path Micro-Benchmarks (best of runs)\n");

    /* ── prepare source ─────────────────────────────────────────────────── */
    char* data = malloc(BENCH_SIZE);
    if (!data) return 1;
    memset(data, 0xAB, BENCH_SIZE);
    FILE* f = fopen(src, "wb");
    if (!f) return 1;
    fwrite(data, 1, BENCH_SIZE, f);
    fclose(f);

    /* ── readall ────────────────────────────────────────────────────────── */
    {
        file_t file;
        if (file_open(&file, src, "rb") != FILE_SUCCESS) return 1;
        uint64_t best = ~0ull;
        size_t got = 0;
        for (int r = 0; r < 5; r++) {
            uint64_t t0 = ns_now();
            void* buf = file_readall(&file, &got);
            uint64_t d = ns_now() - t0;
            if (d < best) best = d;
            free(buf);
        }
        printf("  readall %zuMB : %8.2f GB/s\n", (size_t)BENCH_SIZE >> 20, gb_s(got, best));
        file_close(&file);
    }

    /* ── copy ───────────────────────────────────────────────────────────── */
    {
        file_t s, d;
        if (file_open(&s, src, "rb") != FILE_SUCCESS) return 1;
        uint64_t best = ~0ull;
        for (int r = 0; r < 5; r++) {
            if (file_open(&d, dst, "wb") != FILE_SUCCESS) return 1;
            uint64_t t0 = ns_now();
            file_copy(&s, &d);
            uint64_t dd = ns_now() - t0;
            if (dd < best) best = dd;
            file_close(&d);
            fseek(s.stream, 0, SEEK_SET);
        }
        printf("  copy    %zuMB : %8.2f GB/s\n", (size_t)BENCH_SIZE >> 20, gb_s(BENCH_SIZE, best));
        file_close(&s);
    }

    /* ── path string ops ────────────────────────────────────────────────── */
    {
        const char* p = "/usr/local/lib/some/deep/library.so";
        char out[512];
        int n = 10000000;
        uint64_t t0 = ns_now();
        for (int i = 0; i < n; i++) {
            filepath_basename(p, out, sizeof out);
            filepath_extension(p, out, sizeof out);
        }
        printf("  basename+extension pair : %6.1f ns\n", (double)(ns_now() - t0) / n);
    }

    /* ── extension correctness spot-checks ──────────────────────────────── */
    {
        struct {
            const char* in;
            const char* want;
        } cases[] = {
            {"/path/to.dir/file", ""},      /* dot in directory part ignored */
            {"archive.tar.gz", ".gz"},
            {".bashrc", ""},                /* hidden marker, not extension */
            {"name.", "."},
            {"noext", ""},
        };
        int fails = 0;
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char ext[64];
            filepath_extension(cases[i].in, ext, sizeof ext);
            if (strcmp(ext, cases[i].want) != 0) {
                fprintf(stderr, "  EXT FAIL '%s' -> '%s' (want '%s')\n", cases[i].in, ext, cases[i].want);
                fails++;
            }
        }
        printf("  extension edge cases    : %s\n", fails ? "FAILED" : "ok");
    }

    free(data);
    snprintf(cmd, sizeof cmd, "rm -rf %s", base);
    system(cmd);
    return 0;
}
