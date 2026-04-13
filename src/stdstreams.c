#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "stdstreams.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

/* -------------------------------------------------------------------------
 * Compiler-specific branch prediction macros for hot paths
 * ---------------------------------------------------------------------- */
#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

/* =========================================================================
 * Terminal IO implementation
 * ====================================================================== */

bool readline(const char* prompt, char* buffer, size_t buffer_len) {
    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    if (fgets(buffer, (int)buffer_len, stdin) == NULL) return false;

    buffer[strcspn(buffer, "\n")] = '\0';

    /* Drain any overflow that didn't fit tightly in the buffer. */
    if (strlen(buffer) >= buffer_len - 1) {
        int c;
        while ((c = getchar()) != EOF && c != '\n');
    }
    return true;
}

int getpassword(const char* prompt, char* buffer, size_t buffer_len) {
#ifdef _WIN32
    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode, count, i;

    if (hStdin == INVALID_HANDLE_VALUE) return -1;
    if (!GetConsoleMode(hStdin, &mode)) return -1;
    if (!SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT)) return -1;

    for (i = 0; i < (DWORD)(buffer_len - 1); i++) {
        if (!ReadConsoleA(hStdin, &buffer[i], 1, &count, NULL) || count == 0) break;
        if (buffer[i] == '\n' || buffer[i] == '\r') {
            buffer[i] = '\0';
            break;
        }
    }
    buffer[i] = '\0';

    if (!SetConsoleMode(hStdin, mode)) return -1;
    putchar('\n');
    return (int)i;
#else
    struct termios old_t, new_t;

    if (tcgetattr(fileno(stdin), &old_t) != 0) return -1;

    new_t = old_t;
    new_t.c_lflag &= (tcflag_t)~ECHO;

    if (tcsetattr(fileno(stdin), TCSAFLUSH, &new_t) != 0) return -1;

    bool ok = readline(prompt, buffer, buffer_len);

    if (tcsetattr(fileno(stdin), TCSAFLUSH, &old_t) != 0) return -1;

    putchar('\n');
    return ok ? (int)strlen(buffer) : -1;
#endif
}

/* =========================================================================
 * Stream internals definitions
 * ====================================================================== */

enum stream_type {
    INVALID_STREAM = -1,  // Internal use only — indicates uninitialized or error state
    FILE_STREAM = 0,      // Standard file stream wrapper around FILE*
    STRING_STREAM = 1,    // In-memory string stream with dynamic resizing and null-termination guarantees
};

struct stream {
    /* POSIX-style: returns bytes transferred (>0), 0 for EOF, -1 for error. */
    ssize_t (*read)(void* handle, void* ptr, size_t n);
    ssize_t (*write)(void* handle, const void* ptr, size_t n);
    int (*read_char)(void* handle);
    int (*eof)(void* handle);
    int (*seek)(void* handle, long offset, int whence);
    int (*flush)(void* handle);

    void* handle;
    enum stream_type type;
};

/* =========================================================================
 * Public seek wrapper
 * ====================================================================== */

int stream_seek(stream_t stream, long offset, int whence) {
    STREAM_ASSERT(stream);
    return stream->seek(stream->handle, offset, whence);
}

/* =========================================================================
 * FILE stream vtable
 * ====================================================================== */

static ssize_t file_read_impl(void* handle, void* ptr, size_t n) {
    size_t r = fread(ptr, 1, n, (FILE*)handle);
    if (r == 0) return ferror((FILE*)handle) ? -1 : 0;
    return (ssize_t)r;
}

static ssize_t file_write_impl(void* handle, const void* ptr, size_t n) {
    size_t w = fwrite(ptr, 1, n, (FILE*)handle);
    return (w == 0 && ferror((FILE*)handle)) ? -1 : (ssize_t)w;
}

stream_t create_file_stream(FILE* fp) {
    STREAM_ASSERT(fp);
    stream_t s = malloc(sizeof(struct stream));
    if (!s) return NULL;

    s->read = file_read_impl;
    s->write = file_write_impl;
    s->flush = (int (*)(void*))fflush;
    s->seek = (int (*)(void*, long, int))fseek;
    s->eof = (int (*)(void*))feof;
    s->read_char = (int (*)(void*))fgetc;
    s->handle = fp;
    s->type = FILE_STREAM;
    return s;
}

size_t file_stream_read(stream_t s, void* restrict ptr, size_t size, size_t count) {
    STREAM_ASSERT(s);
    STREAM_ASSERT(s->type == FILE_STREAM);
    fseek((FILE*)s->handle, 0, SEEK_SET);
    return fread(ptr, size, count, (FILE*)s->handle);
}

/* =========================================================================
 * String stream capacity helper (Layer 1 Fast Math Allocation)
 * ====================================================================== */

static bool string_stream_ensure_capacity(string_stream* ss, size_t needed) {
    if (needed <= ss->capacity) return true;

    size_t new_cap = ss->capacity == 0 ? 64 : ss->capacity;
    while (new_cap < needed) {
        new_cap *= 2;  // Exponential expansion limits malloc overhead calls
    }

    char* new_data = (char*)realloc(ss->data, new_cap);
    if (!new_data) return false;

    ss->data = new_data;
    ss->capacity = new_cap;
    return true;
}

/* =========================================================================
 * String stream vtable
 * ====================================================================== */

static ssize_t string_read_impl(void* handle, void* ptr, size_t n) {
    string_stream* ss = (string_stream*)handle;

    if (ss->pos >= ss->size) return 0; /* EOF */

    size_t avail = ss->size - ss->pos;
    if (n > avail) n = avail;

    memcpy(ptr, ss->data + ss->pos, n);
    ss->pos += n;
    return (ssize_t)n;
}

static ssize_t string_write_impl(void* handle, const void* ptr, size_t n) {
    string_stream* ss = (string_stream*)handle;

    size_t needed_cap = ss->pos + n + 1;  // Factor in the NUL boundary

    if (!string_stream_ensure_capacity(ss, needed_cap)) return -1;

    memcpy(ss->data + ss->pos, ptr, n);
    ss->pos += n;

    if (ss->pos > ss->size) {
        ss->size = ss->pos;
    }

    /* Strictly maintain null termination integrity at boundary */
    ss->data[ss->size] = '\0';

    return (ssize_t)n;
}

static int string_read_char_impl(void* handle) {
    string_stream* ss = (string_stream*)handle;
    if (ss->pos >= ss->size) return EOF;
    return (unsigned char)ss->data[ss->pos++];
}

static int string_eof_impl(void* handle) {
    string_stream* ss = (string_stream*)handle;
    return ss->pos >= ss->size;
}

static int string_seek_impl(void* handle, long offset, int whence) {
    string_stream* ss = (string_stream*)handle;
    size_t new_pos;

    switch (whence) {
        case SEEK_SET:
            if (offset < 0 || (size_t)offset > ss->size) return -1;
            new_pos = (size_t)offset;
            break;
        case SEEK_CUR:
            if (offset < 0 && (size_t)(-offset) > ss->pos) return -1;
            if (offset > 0 && ss->pos + (size_t)offset > ss->size) return -1;
            new_pos = (size_t)((ssize_t)ss->pos + offset);
            break;
        case SEEK_END:
            if (offset > 0 || (size_t)(-offset) > ss->size) return -1;
            new_pos = (size_t)((ssize_t)ss->size + offset);
            break;
        default:
            return -1;
    }

    ss->pos = new_pos;
    return 0;
}

static int string_flush_impl(void* handle) {
    (void)handle;
    return 0;
}

/* -------------------------------------------------------------------------
 * Public string-stream helpers
 * ---------------------------------------------------------------------- */

int string_stream_write(stream_t stream, const char* str) {
    STREAM_ASSERT(stream && stream->type == STRING_STREAM);

    string_stream* ss = (string_stream*)stream->handle;
    size_t len = strlen(str);
    size_t needed_cap = ss->size + len + 1;  // +1 explicitly limits NUL constraint breach

    if (!string_stream_ensure_capacity(ss, needed_cap)) return -1;

    memcpy(ss->data + ss->size, str, len);
    ss->size += len;
    ss->data[ss->size] = '\0'; /* Null term is secured */

    return (int)len;
}

int string_stream_write_len(stream_t stream, const char* str, size_t n) {
    STREAM_ASSERT(stream && stream->type == STRING_STREAM);

    string_stream* ss = (string_stream*)stream->handle;
    size_t needed_cap = ss->size + n + 1;  // +1 explicitly limits NUL constraint breach

    if (!string_stream_ensure_capacity(ss, needed_cap)) return -1;

    memcpy(ss->data + ss->size, str, n);
    ss->size += n;
    ss->data[ss->size] = '\0'; /* Null term is secured */

    return (int)n;
}

const char* string_stream_data(stream_t stream) {
    if (!stream || stream->type != STRING_STREAM) return NULL;
    return ((string_stream*)stream->handle)->data;
}

stream_t create_string_stream(size_t initial_capacity) {
    /* Perform combined single-block memory allocation to drastically trim overhead */
    stream_t s = malloc(sizeof(struct stream) + sizeof(string_stream));
    if (!s) return NULL;

    // The string_stream struct is allocated directly after the stream struct in the same block
    // for cache locality and reduced malloc calls.
    string_stream* ss = (string_stream*)(s + 1);

    size_t cap = initial_capacity > 0 ? initial_capacity : 1;
    ss->data = malloc(cap);
    if (!ss->data) {
        free(s);
        return NULL;
    }

    ss->data[0] = '\0';
    ss->size = 0;
    ss->capacity = cap;
    ss->pos = 0;

    s->read = string_read_impl;
    s->write = string_write_impl;
    s->flush = string_flush_impl;
    s->read_char = string_read_char_impl;
    s->eof = string_eof_impl;
    s->seek = string_seek_impl;

    s->handle = ss;
    s->type = STRING_STREAM;
    return s;
}

/* =========================================================================
 * Delimited read
 * ====================================================================== */

ssize_t read_until(stream_t stream, int delim, char* buffer, size_t buffer_size) {
    STREAM_ASSERT(stream && buffer && buffer_size > 0);

    /* --- Vectorized SIMD Fast Path for String Streams --- */
    if (LIKELY(stream->type == STRING_STREAM)) {
        string_stream* ss = (string_stream*)stream->handle;
        if (UNLIKELY(ss->pos >= ss->size)) return -1;

        size_t avail = ss->size - ss->pos;
        size_t max_read = buffer_size - 1;
        size_t check_len = avail < max_read ? avail : max_read;

        const char* p = ss->data + ss->pos;

        // memchr is often optimized with SIMD instructions for large buffers
        const char* match = memchr(p, delim, check_len);

        size_t copy_len;
        if (match) {
            copy_len = (size_t)(match - p);
            memcpy(buffer, p, copy_len);
            ss->pos += copy_len + 1; /* Consume delimiter */
        } else {
            copy_len = check_len;
            memcpy(buffer, p, copy_len);
            ss->pos += copy_len;
        }

        buffer[copy_len] = '\0';
        return (ssize_t)copy_len;
    }

    /* --- Standard fallback for traditional FILE_STREAM --- */
    ssize_t bytes = 0;
    int ch;
    while (bytes < (ssize_t)(buffer_size - 1)) {
        ch = stream->read_char(stream->handle);
        if (ch == EOF || ch == delim) break;
        buffer[bytes++] = (char)ch;
    }

    if (ch == EOF && stream->eof(stream->handle) && bytes == 0) return -1;
    buffer[bytes] = '\0';
    return bytes;
}

/* =========================================================================
 * Layer 1 — fast string→string copy
 * ====================================================================== */

unsigned long string_stream_copy_fast(stream_t dst, stream_t src) {
    STREAM_ASSERT(dst && src);
    STREAM_ASSERT(dst->type == STRING_STREAM && src->type == STRING_STREAM);

    string_stream* s = (string_stream*)src->handle;
    string_stream* d = (string_stream*)dst->handle;

    if (s->pos >= s->size) return 0; /* nothing to copy */

    size_t n = s->size - s->pos;
    size_t needed_cap = d->pos + n + 1;  // Protect NUL requirement constraints

    /* Grow destination securely caching exponential capacity checks. */
    if (!string_stream_ensure_capacity(d, needed_cap)) return (unsigned long)-1;

    memcpy(d->data + d->pos, s->data + s->pos, n);

    if (d->pos + n > d->size) {
        d->size = d->pos + n;
    }

    /* Enforce rigid null termination constraint directly */
    d->data[d->size] = '\0';

    d->pos += n;
    s->pos += n;
    return (unsigned long)n;
}

/* =========================================================================
 * Layer 2 — generic copy
 * ====================================================================== */

unsigned long io_copy(stream_t writer, stream_t reader) {
    STREAM_ASSERT(writer && reader);

    /* ---- FAST PATH: Native String → String bypass ---- */
    if (writer->type == STRING_STREAM && reader->type == STRING_STREAM) return string_stream_copy_fast(writer, reader);

    /* L1/L2 cache tuned 16 KiB buffer chunking limits vtable dispatch overhead */
    char buf[16384];
    ssize_t nread;
    unsigned long total = 0;

    while ((nread = reader->read(reader->handle, buf, sizeof(buf))) > 0) {
        STREAM_ASSERT((size_t)nread <= sizeof(buf));
        ssize_t w = writer->write(writer->handle, buf, (size_t)nread);
        if (w < 0) return (unsigned long)-1;
        total += (unsigned long)w;
    }

    if (nread < 0) return (unsigned long)-1;

    writer->flush(writer->handle);
    return total;
}

unsigned long io_copy_n(stream_t writer, stream_t reader, size_t n) {
    STREAM_ASSERT(writer && reader);

    // L1/L2 cache tuned 16 KiB buffer chunking limits vtable dispatch overhead
    char buf[16384];
    ssize_t nread;
    unsigned long total = 0;

    while (n > 0) {
        size_t want = n < sizeof(buf) ? n : sizeof(buf);
        nread = reader->read(reader->handle, buf, want);
        if (nread <= 0) break;

        STREAM_ASSERT((size_t)nread <= sizeof(buf));

        ssize_t w = writer->write(writer->handle, buf, (size_t)nread);
        if (w < 0) return (unsigned long)-1;

        total += (unsigned long)w;
        n -= (size_t)nread;
    }

    if (nread < 0) return (unsigned long)-1;

    writer->flush(writer->handle);
    return total;
}

/* =========================================================================
 * Destroy
 * ====================================================================== */

static void free_file_stream(stream_t s) {
    if (!s) return;
    FILE* fp = (FILE*)s->handle;
    if (fp != stdout && fp != stderr && fp != stdin) fclose(fp);
    free(s);
}

static void free_string_stream(stream_t s) {
    if (!s) return;

    /* s->handle effectively points to memory inherently linked beside 's' buffer due to creation step. */
    string_stream* ss = (string_stream*)s->handle;
    if (ss && ss->data) {
        free(ss->data);
    }

    /* Only requires a single free operation targeting 's' due to contiguous inline setup wrapper mechanism! */
    free(s);
}

void stream_destroy(stream_t stream) {
    if (!stream) return;
    switch (stream->type) {
        case FILE_STREAM:
            free_file_stream(stream);
            break;
        case STRING_STREAM:
            free_string_stream(stream);
            break;
        case INVALID_STREAM:
            fprintf(stderr, "[stream_destroy]: warning: attempted to destroy invalid stream\n");
            break;
    }
}
