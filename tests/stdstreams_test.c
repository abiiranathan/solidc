/**
 * @file test_stdstreams.c
 * @brief Comprehensive test suite for stdstreams.
 *
 * Build (example):
 *   cc -g -DDEBUG -o test_stdstreams test_stdstreams.c stdstreams.c \
 *      -I../include -lsolidc
 *
 * Each test_*() function is independent and calls stream_destroy on every
 * stream it creates, so the suite can be run under valgrind / ASan cleanly.
 */

#include "../include/stdstreams.h"
#include "../include/filepath.h"
#include "../include/macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Shared fixtures
 * ---------------------------------------------------------------------- */

static const char* SHORT_DATA = "Hello, World!\n";
static const char* LOREM =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"
    "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.\n";

/* =========================================================================
 * string_stream_write / string_stream_data
 * ====================================================================== */

/* Basic round-trip: write then read back via string_stream_data. */
void test_string_stream_basic_write_read(void) {
    stream_t s = create_string_stream(64);
    ASSERT(s);

    int n = string_stream_write(s, SHORT_DATA);
    ASSERT_EQ(n, (int)strlen(SHORT_DATA));

    const char* data = string_stream_data(s);
    ASSERT(data);
    ASSERT_STR_EQ(data, SHORT_DATA);

    stream_destroy(s);
}

/* Multiple writes accumulate in the backing string. */
void test_string_stream_multiple_writes(void) {
    stream_t s = create_string_stream(16);
    ASSERT(s);

    string_stream_write(s, "foo");
    string_stream_write(s, "bar");
    string_stream_write(s, "baz");

    ASSERT_STR_EQ(string_stream_data(s), "foobarbaz");

    stream_destroy(s);
}

void test_string_string_write_n(void) {
    stream_t s = create_string_stream(16);
    ASSERT(s);

    int n = string_stream_write_len(s, "hello world", 5);
    ASSERT_EQ(n, 5);

    ASSERT_STR_EQ(string_stream_data(s), "hello");

    stream_destroy(s);
}

/* Writing an empty string is a no-op and doesn't corrupt state. */
void test_string_stream_write_empty(void) {
    stream_t s = create_string_stream(32);
    ASSERT(s);

    string_stream_write(s, "before");
    int n = string_stream_write(s, "");
    ASSERT_EQ(n, 0);
    string_stream_write(s, "after");

    ASSERT_STR_EQ(string_stream_data(s), "beforeafter");

    stream_destroy(s);
}

/* string_stream_data returns NULL for a file stream. */
void test_string_stream_data_on_file_stream_returns_null(void) {
    stream_t fs = create_file_stream(stderr);
    ASSERT(fs);

    const char* p = string_stream_data(fs);
    ASSERT(p == NULL);

    /* stderr must not be closed */
    stream_destroy(fs);
}

/* =========================================================================
 * stream_seek
 * ====================================================================== */

/* SEEK_SET to 0 then read confirms position resets correctly. */
void test_string_stream_seek_set(void) {
    stream_t s = create_string_stream(64);
    ASSERT(s);

    string_stream_write(s, "ABCDE");

    /* Read past first byte via read_until. */
    char buf[16];
    read_until(s, 'C', buf, sizeof(buf)); /* consumes "AB" */

    /* Rewind and read everything again. */
    int rc = stream_seek(s, 0, SEEK_SET);
    ASSERT_EQ(rc, 0);

    ssize_t n = read_until(s, '\0', buf, sizeof(buf));
    /* \0 never appears in the data so we get everything until EOF. */
    ASSERT_EQ(n, 5);
    ASSERT_STR_EQ(buf, "ABCDE");

    stream_destroy(s);
}

/* SEEK_SET beyond end returns -1. */
void test_string_stream_seek_beyond_end(void) {
    stream_t s = create_string_stream(32);
    ASSERT(s);

    string_stream_write(s, "abc");

    int rc = stream_seek(s, 100, SEEK_SET);
    ASSERT_EQ(rc, -1);

    stream_destroy(s);
}

/* SEEK_END with offset 0 moves to the last byte boundary. */
void test_string_stream_seek_end(void) {
    stream_t s = create_string_stream(32);
    ASSERT(s);

    string_stream_write(s, "hello");

    int rc = stream_seek(s, 0, SEEK_END);
    ASSERT_EQ(rc, 0);

    /* At EOF — nothing left to read. */
    char buf[16];
    ssize_t n = read_until(s, '\n', buf, sizeof(buf));
    ASSERT_EQ(n, -1); /* EOF with no bytes read */

    stream_destroy(s);
}

/* SEEK_CUR forward and backward. */
void test_string_stream_seek_cur(void) {
    stream_t s = create_string_stream(32);
    ASSERT(s);

    string_stream_write(s, "0123456789");

    /* Skip 3 bytes from the start. */
    stream_seek(s, 0, SEEK_SET);
    int rc = stream_seek(s, 3, SEEK_CUR);
    ASSERT_EQ(rc, 0);

    char buf[16];
    ssize_t n = read_until(s, '\0', buf, sizeof(buf));
    ASSERT_EQ(n, 7);
    ASSERT_STR_EQ(buf, "3456789");

    stream_destroy(s);
}

/* =========================================================================
 * read_until
 * ====================================================================== */

/* Delimiter at the very first byte → empty result (0 bytes, not -1). */
void test_readuntil_delim_at_start(void) {
    stream_t s = create_string_stream(32);
    ASSERT(s);

    string_stream_write(s, ",rest");

    char buf[16];
    ssize_t n = read_until(s, ',', buf, sizeof(buf));
    ASSERT_EQ(n, 0);
    ASSERT_STR_EQ(buf, "");

    stream_destroy(s);
}

/* Delimiter at the very last byte. */
void test_readuntil_delim_at_end(void) {
    stream_t s = create_string_stream(32);
    ASSERT(s);

    string_stream_write(s, "hello,");

    char buf[16];
    ssize_t n = read_until(s, ',', buf, sizeof(buf));
    ASSERT_EQ(n, 5);
    ASSERT_STR_EQ(buf, "hello");

    stream_destroy(s);
}

/* Delimiter never appears — read to EOF. */
void test_readuntil_no_delim(void) {
    stream_t s = create_string_stream(64);
    ASSERT(s);

    string_stream_write(s, "nodeliminhere");

    char buf[64];
    ssize_t n = read_until(s, '|', buf, sizeof(buf));
    ASSERT_EQ(n, 13);
    ASSERT_STR_EQ(buf, "nodeliminhere");

    stream_destroy(s);
}

/* Buffer exactly the right size (no overflow, still NUL-terminated). */
void test_readuntil_exact_buffer_fit(void) {
    stream_t s = create_string_stream(32);
    ASSERT(s);

    string_stream_write(s, "hello|");

    /* buf is 6 bytes: 5 for "hello" + NUL. */
    char buf[6];
    ssize_t n = read_until(s, '|', buf, sizeof(buf));
    ASSERT_EQ(n, 5);
    ASSERT_STR_EQ(buf, "hello");

    stream_destroy(s);
}

/* Buffer too small — data is truncated, buffer is still NUL-terminated. */
void test_readuntil_buffer_truncation(void) {
    stream_t s = create_string_stream(64);
    ASSERT(s);

    string_stream_write(s, "verylongstring|");

    char buf[5]; /* can hold 4 chars + NUL */
    ssize_t n = read_until(s, '|', buf, sizeof(buf));
    /* We read min(14, 4) = 4 before truncation. */
    ASSERT_EQ(n, 4);
    ASSERT_EQ(buf[4], '\0');

    stream_destroy(s);
}

/* Multiple sequential read_until calls on the same stream. */
void test_readuntil_sequential(void) {
    stream_t s = create_string_stream(64);
    ASSERT(s);

    string_stream_write(s, "alpha,beta,gamma");

    char buf[32];
    ssize_t n;

    n = read_until(s, ',', buf, sizeof(buf));
    ASSERT_EQ(n, 5);
    ASSERT_STR_EQ(buf, "alpha");

    n = read_until(s, ',', buf, sizeof(buf));
    ASSERT_EQ(n, 4);
    ASSERT_STR_EQ(buf, "beta");

    n = read_until(s, ',', buf, sizeof(buf));
    ASSERT_EQ(n, 5);
    ASSERT_STR_EQ(buf, "gamma");

    /* Stream is now at EOF — next call returns -1. */
    n = read_until(s, ',', buf, sizeof(buf));
    ASSERT_EQ(n, -1);

    stream_destroy(s);
}

/* read_until on a file stream. */
void test_readuntil_file_stream(void) {
    const char* path = make_tempfile();
    ASSERT(path);

    FILE* fp = fopen(path, "w+");
    ASSERT(fp);
    fwrite("one:two:three", 1, 13, fp);
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    stream_t s = create_file_stream(fp);
    ASSERT(s);

    char buf[32];
    ssize_t n;

    n = read_until(s, ':', buf, sizeof(buf));
    ASSERT_EQ(n, 3);
    ASSERT_STR_EQ(buf, "one");

    n = read_until(s, ':', buf, sizeof(buf));
    ASSERT_EQ(n, 3);
    ASSERT_STR_EQ(buf, "two");

    n = read_until(s, ':', buf, sizeof(buf));
    ASSERT_EQ(n, 5);
    ASSERT_STR_EQ(buf, "three");

    stream_destroy(s);
}

/* =========================================================================
 * io_copy
 * ====================================================================== */

/* Basic string→string copy (exercises fast path). */
void test_iocopy_string_to_string(void) {
    stream_t src = create_string_stream(256);
    stream_t dst = create_string_stream(256);
    ASSERT(src && dst);

    string_stream_write(src, LOREM);

    unsigned long n = io_copy(dst, src);
    ASSERT_EQ((size_t)n, strlen(LOREM));
    ASSERT_STR_EQ(string_stream_data(dst), LOREM);

    stream_destroy(src);
    stream_destroy(dst);
}

/* Copy after partial read — only unread bytes are transferred. */
void test_iocopy_partial_source(void) {
    stream_t src = create_string_stream(64);
    stream_t dst = create_string_stream(64);
    ASSERT(src && dst);

    string_stream_write(src, "ABCDEFGHIJ");

    /* Advance src cursor by 4 bytes. */
    char tmp[8];
    read_until(src, '\0', tmp, 5); /* reads "ABCD" (4 chars + stops) */

    unsigned long n = io_copy(dst, src);
    /* "EFGHIJ" = 6 bytes remain */
    ASSERT_EQ((size_t)n, 6);
    ASSERT_STR_EQ(string_stream_data(dst), "EFGHIJ");

    stream_destroy(src);
    stream_destroy(dst);
}

/* Copying an empty source produces 0 bytes, dst stays unchanged. */
void test_iocopy_empty_source(void) {
    stream_t src = create_string_stream(32);
    stream_t dst = create_string_stream(32);
    ASSERT(src && dst);

    /* src is empty — nothing written to it */
    unsigned long n = io_copy(dst, src);
    ASSERT_EQ((size_t)n, 0);

    stream_destroy(src);
    stream_destroy(dst);
}

/* Multiple io_copy calls accumulate in dst. */
void test_iocopy_accumulates_in_dst(void) {
    stream_t src1 = create_string_stream(32);
    stream_t src2 = create_string_stream(32);
    stream_t dst = create_string_stream(64);
    ASSERT(src1 && src2 && dst);

    string_stream_write(src1, "hello ");
    string_stream_write(src2, "world");

    io_copy(dst, src1);
    io_copy(dst, src2);

    ASSERT_STR_EQ(string_stream_data(dst), "hello world");

    stream_destroy(src1);
    stream_destroy(src2);
    stream_destroy(dst);
}

/* io_copy from file stream into string stream. */
void test_iocopy_file_to_string(void) {
    const char* path = make_tempfile();
    ASSERT(path);

    FILE* fp = fopen(path, "w+");
    ASSERT(fp);
    fwrite(SHORT_DATA, 1, strlen(SHORT_DATA), fp);
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    stream_t fsrc = create_file_stream(fp);
    stream_t dst = create_string_stream(64);
    ASSERT(fsrc && dst);

    unsigned long n = io_copy(dst, fsrc);
    ASSERT_EQ((size_t)n, strlen(SHORT_DATA));
    ASSERT_STR_EQ(string_stream_data(dst), SHORT_DATA);

    stream_destroy(fsrc);
    stream_destroy(dst);
}

/* Caller rewinds then re-copies — verifies seek + io_copy compose. */
void test_iocopy_after_rewind(void) {
    stream_t src = create_string_stream(64);
    stream_t dst = create_string_stream(64);
    ASSERT(src && dst);

    string_stream_write(src, "rewind");

    io_copy(dst, src);

    /* Rewind src and copy again into a fresh dst. */
    stream_seek(src, 0, SEEK_SET);
    stream_t dst2 = create_string_stream(64);
    ASSERT(dst2);

    unsigned long n = io_copy(dst2, src);
    ASSERT_EQ((size_t)n, strlen("rewind"));
    ASSERT_STR_EQ(string_stream_data(dst2), "rewind");

    stream_destroy(src);
    stream_destroy(dst);
    stream_destroy(dst2);
}

/* =========================================================================
 * io_copy_n
 * ====================================================================== */

/* n == 0 copies nothing. */
void test_iocopy_n_zero(void) {
    stream_t src = create_string_stream(64);
    stream_t dst = create_string_stream(64);
    ASSERT(src && dst);

    string_stream_write(src, LOREM);

    unsigned long n = io_copy_n(dst, src, 0);
    ASSERT_EQ((size_t)n, 0);

    stream_destroy(src);
    stream_destroy(dst);
}

/* n < source length — only first n bytes land in dst. */
void test_iocopy_n_partial(void) {
    stream_t src = create_string_stream(256);
    stream_t dst = create_string_stream(256);
    ASSERT(src && dst);

    string_stream_write(src, LOREM);

    size_t want = 32;
    unsigned long n = io_copy_n(dst, src, want);
    ASSERT_EQ((size_t)n, want);

    /* dst content is the first `want` bytes of LOREM. */
    char expected[33];
    memcpy(expected, LOREM, want);
    expected[want] = '\0';

    const char* actual = string_stream_data(dst);
    ASSERT(actual);
    ASSERT_EQ(memcmp(actual, expected, want), 0);

    stream_destroy(src);
    stream_destroy(dst);
}

/* n > source length — copies everything available. */
void test_iocopy_n_exceeds_source(void) {
    stream_t src = create_string_stream(64);
    stream_t dst = create_string_stream(64);
    ASSERT(src && dst);

    string_stream_write(src, "short");

    unsigned long n = io_copy_n(dst, src, 10000);
    ASSERT_EQ((size_t)n, strlen("short"));
    ASSERT_STR_EQ(string_stream_data(dst), "short");

    stream_destroy(src);
    stream_destroy(dst);
}

/* n == exact source length — full copy, nothing more. */
void test_iocopy_n_exact_length(void) {
    stream_t src = create_string_stream(64);
    stream_t dst = create_string_stream(64);
    ASSERT(src && dst);

    const char* payload = "exactfit";
    string_stream_write(src, payload);

    unsigned long n = io_copy_n(dst, src, strlen(payload));
    ASSERT_EQ((size_t)n, strlen(payload));
    ASSERT_STR_EQ(string_stream_data(dst), payload);

    stream_destroy(src);
    stream_destroy(dst);
}

/* =========================================================================
 * string_stream_copy_fast  (Layer 1 fast path)
 * ====================================================================== */

/* Full copy from start. */
void test_copy_fast_full(void) {
    stream_t src = create_string_stream(128);
    stream_t dst = create_string_stream(128);
    ASSERT(src && dst);

    string_stream_write(src, LOREM);

    unsigned long n = string_stream_copy_fast(dst, src);
    ASSERT_EQ((size_t)n, strlen(LOREM));
    ASSERT_STR_EQ(string_stream_data(dst), LOREM);

    stream_destroy(src);
    stream_destroy(dst);
}

/* Fast copy starting mid-stream. */
void test_copy_fast_from_mid(void) {
    stream_t src = create_string_stream(64);
    stream_t dst = create_string_stream(64);
    ASSERT(src && dst);

    string_stream_write(src, "0123456789");

    /* Advance src to position 5. */
    stream_seek(src, 5, SEEK_SET);

    unsigned long n = string_stream_copy_fast(dst, src);
    ASSERT_EQ((size_t)n, 5);
    ASSERT_STR_EQ(string_stream_data(dst), "56789");

    stream_destroy(src);
    stream_destroy(dst);
}

/* Fast copy of empty source writes 0 bytes. */
void test_copy_fast_empty_source(void) {
    stream_t src = create_string_stream(32);
    stream_t dst = create_string_stream(32);
    ASSERT(src && dst);

    unsigned long n = string_stream_copy_fast(dst, src);
    ASSERT_EQ((size_t)n, 0);

    stream_destroy(src);
    stream_destroy(dst);
}

/* Fast copy accumulates in dst when called multiple times. */
void test_copy_fast_accumulates(void) {
    stream_t a = create_string_stream(32);
    stream_t b = create_string_stream(32);
    stream_t dst = create_string_stream(64);
    ASSERT(a && b && dst);

    string_stream_write(a, "ping");
    string_stream_write(b, "pong");

    string_stream_copy_fast(dst, a);
    string_stream_copy_fast(dst, b);

    ASSERT_STR_EQ(string_stream_data(dst), "pingpong");

    stream_destroy(a);
    stream_destroy(b);
    stream_destroy(dst);
}

/* =========================================================================
 * Large data / stress
 * ====================================================================== */

/* Write 1 MiB of 'A's and verify the copy is byte-perfect. */
void test_large_copy(void) {
    const size_t SIZE = 1024 * 1024;

    char* data = malloc(SIZE + 1);
    ASSERT(data);
    memset(data, 'A', SIZE);
    data[SIZE] = '\0';

    stream_t src = create_string_stream(SIZE + 1);
    stream_t dst = create_string_stream(SIZE + 1);
    ASSERT(src && dst);

    string_stream_write(src, data);

    unsigned long n = io_copy(dst, src);
    ASSERT_EQ((size_t)n, SIZE);
    ASSERT_EQ(memcmp(string_stream_data(dst), data, SIZE), 0);

    free(data);
    stream_destroy(src);
    stream_destroy(dst);
}

/* Repeated io_copy_n in a loop sums to the full source length. */
void test_chunked_copy(void) {
    const size_t CHUNK = 16;
    stream_t src = create_string_stream(256);
    stream_t dst = create_string_stream(256);
    ASSERT(src && dst);

    string_stream_write(src, LOREM);
    stream_seek(src, 0, SEEK_SET);

    size_t total = 0;
    size_t src_sz = strlen(LOREM);
    unsigned long copied;

    while (total < src_sz) {
        copied = io_copy_n(dst, src, CHUNK);
        if ((unsigned long)copied == (unsigned long)-1) break;
        total += copied;
        if (copied < CHUNK) break;
    }

    ASSERT_EQ(total, src_sz);

    stream_destroy(src);
    stream_destroy(dst);
}

/* =========================================================================
 * create_string_stream edge cases
 * ====================================================================== */

/* initial_capacity of 0 — implementation must still work. */
void test_string_stream_zero_capacity(void) {
    stream_t s = create_string_stream(0);
    ASSERT(s);

    string_stream_write(s, "growme");
    ASSERT_STR_EQ(string_stream_data(s), "growme");

    stream_destroy(s);
}

/* =========================================================================
 * file_stream_read
 * ====================================================================== */

void test_file_stream_read(void) {
    const char* path = make_tempfile();
    ASSERT(path);

    FILE* fp = fopen(path, "w+");
    ASSERT(fp);
    fwrite("file content", 1, 12, fp);
    fflush(fp);

    stream_t fs = create_file_stream(fp);
    ASSERT(fs);

    char buf[32] = {0};
    size_t r = file_stream_read(fs, buf, 1, 12);
    ASSERT_EQ(r, 12);
    ASSERT_EQ(memcmp(buf, "file content", 12), 0);

    stream_destroy(fs);
}

/* =========================================================================
 * Runner
 * ====================================================================== */

#define RUN(fn)                    \
    do {                           \
        fn();                      \
        printf("PASS  %s\n", #fn); \
    } while (0)

int main(void) {
    /* string_stream_write / data */
    RUN(test_string_stream_basic_write_read);
    RUN(test_string_stream_multiple_writes);
    RUN(test_string_string_write_n);
    RUN(test_string_stream_write_empty);
    RUN(test_string_stream_data_on_file_stream_returns_null);

    /* stream_seek */
    RUN(test_string_stream_seek_set);
    RUN(test_string_stream_seek_beyond_end);
    RUN(test_string_stream_seek_end);
    RUN(test_string_stream_seek_cur);

    /* read_until */
    RUN(test_readuntil_delim_at_start);
    RUN(test_readuntil_delim_at_end);
    RUN(test_readuntil_no_delim);
    RUN(test_readuntil_exact_buffer_fit);
    RUN(test_readuntil_buffer_truncation);
    RUN(test_readuntil_sequential);
    RUN(test_readuntil_file_stream);

    /* io_copy */
    RUN(test_iocopy_string_to_string);
    RUN(test_iocopy_partial_source);
    RUN(test_iocopy_empty_source);
    RUN(test_iocopy_accumulates_in_dst);
    RUN(test_iocopy_file_to_string);
    RUN(test_iocopy_after_rewind);

    /* io_copy_n */
    RUN(test_iocopy_n_zero);
    RUN(test_iocopy_n_partial);
    RUN(test_iocopy_n_exceeds_source);
    RUN(test_iocopy_n_exact_length);

    /* string_stream_copy_fast */
    RUN(test_copy_fast_full);
    RUN(test_copy_fast_from_mid);
    RUN(test_copy_fast_empty_source);
    RUN(test_copy_fast_accumulates);

    /* stress */
    RUN(test_large_copy);
    RUN(test_chunked_copy);

    /* edge cases */
    RUN(test_string_stream_zero_capacity);
    RUN(test_file_stream_read);

    printf("\nAll tests passed.\n");
    return 0;
}
