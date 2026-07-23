#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "stdstreams.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#endif

/* Detect POSIX unlocked I/O support for single-threaded stdio acceleration */
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__unix__)
#define HAS_POSIX_UNLOCKED_IO 1
#else
#define HAS_POSIX_UNLOCKED_IO 0
#endif

/* =========================================================================
 * Stream Structure Definition
 * ====================================================================== */

enum stream_type { INVALID_STREAM = -1, FILE_STREAM = 0, STRING_STREAM = 1 };

struct stream {
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
 * Fast Capacity Expansion Helper
 * ====================================================================== */

static STREAM_INLINE size_t calculate_growth(size_t current, size_t needed) {
    if (needed > SIZE_MAX - 1) return 0;  // Overflow guard

    size_t new_cap = current == 0 ? STRING_STREAM_SSO_CAP : current;
    while (new_cap < needed) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = needed;
            break;
        }
        new_cap *= 2;
    }
    return new_cap;
}

static bool string_stream_ensure_capacity(string_stream* ss, size_t needed) {
    if (STREAM_LIKELY(needed <= ss->capacity)) return true;

    size_t new_cap = calculate_growth(ss->capacity, needed);
    if (STREAM_UNLIKELY(new_cap == 0)) return false;

    char* new_data = NULL;
    if (ss->data == ss->inline_buf) {
        // Transition from SSO inline buffer to heap allocation
        new_data = (char*)malloc(new_cap);
        if (STREAM_UNLIKELY(!new_data)) return false;
        memcpy(new_data, ss->inline_buf, ss->size + 1);
    } else {
        new_data = (char*)realloc(ss->data, new_cap);
        if (STREAM_UNLIKELY(!new_data)) return false;
    }

    ss->data = new_data;
    ss->capacity = new_cap;
    return true;
}

/* =========================================================================
 * Terminal I/O Implementations
 * ====================================================================== */

bool readline(const char* prompt, char* buffer, size_t buffer_len) {
    if (STREAM_UNLIKELY(!buffer || buffer_len == 0)) return false;

    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    if (fgets(buffer, (int)buffer_len, stdin) == NULL) return false;

    // Single-pass scan using memchr to strip newline
    char* nl = (char*)memchr(buffer, '\n', buffer_len);
    if (nl) {
        *nl = '\0';
    } else {
        // Drain overflowing input characters if line exceeded buffer_len - 1
        size_t len = strlen(buffer);
        if (len == buffer_len - 1) {
            int c;
#if HAS_POSIX_UNLOCKED_IO
            flockfile(stdin);
            while ((c = getc_unlocked(stdin)) != EOF && c != '\n')
                ;
            funlockfile(stdin);
#else
            while ((c = getchar()) != EOF && c != '\n')
                ;
#endif
        }
    }
    return true;
}

int getpassword(const char* prompt, char* buffer, size_t buffer_len) {
    if (STREAM_UNLIKELY(!buffer || buffer_len == 0)) return -1;

#ifdef _WIN32
    if (prompt) {
        fputs(prompt, stdout);
        fflush(stdout);
    }

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode, count, i = 0;

    if (hStdin == INVALID_HANDLE_VALUE || !GetConsoleMode(hStdin, &mode)) return -1;
    if (!SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT)) return -1;

    for (i = 0; i < (DWORD)(buffer_len - 1); i++) {
        if (!ReadConsoleA(hStdin, &buffer[i], 1, &count, NULL) || count == 0) break;
        if (buffer[i] == '\n' || buffer[i] == '\r') break;
    }
    buffer[i] = '\0';

    SetConsoleMode(hStdin, mode);
    putchar('\n');
    return (int)i;
#else
    struct termios old_t, new_t;
    int fd = fileno(stdin);

    if (tcgetattr(fd, &old_t) != 0) return -1;

    new_t = old_t;
    new_t.c_lflag &= (tcflag_t)~ECHO;

    if (tcsetattr(fd, TCSAFLUSH, &new_t) != 0) return -1;

    bool ok = readline(prompt, buffer, buffer_len);

    tcsetattr(fd, TCSAFLUSH, &old_t);
    putchar('\n');

    return ok ? (int)strlen(buffer) : -1;
#endif
}

/* =========================================================================
 * Stream Implementations
 * ====================================================================== */

int stream_seek(stream_t stream, long offset, int whence) {
    STREAM_ASSERT(stream);
    return stream->seek(stream->handle, offset, whence);
}

/* -------------------------------------------------------------------------
 * File Stream VTable Implementations
 * ---------------------------------------------------------------------- */

static ssize_t file_read_impl(void* handle, void* ptr, size_t n) {
    FILE* fp = (FILE*)handle;
#if HAS_POSIX_UNLOCKED_IO
    flockfile(fp);
    size_t r = fread_unlocked(ptr, 1, n, fp);
    funlockfile(fp);
#else
    size_t r = fread(ptr, 1, n, fp);
#endif
    if (r == 0) return ferror(fp) ? -1 : 0;
    return (ssize_t)r;
}

static ssize_t file_write_impl(void* handle, const void* ptr, size_t n) {
    FILE* fp = (FILE*)handle;
#if HAS_POSIX_UNLOCKED_IO
    flockfile(fp);
    size_t w = fwrite_unlocked(ptr, 1, n, fp);
    funlockfile(fp);
#else
    size_t w = fwrite(ptr, 1, n, fp);
#endif
    return (w == 0 && ferror(fp)) ? -1 : (ssize_t)w;
}

static int file_read_char_impl(void* handle) {
    FILE* fp = (FILE*)handle;
#if HAS_POSIX_UNLOCKED_IO
    return getc_unlocked(fp);
#else
    return fgetc(fp);
#endif
}

static int file_eof_impl(void* handle) {
    return feof((FILE*)handle);
}

static int file_flush_impl(void* handle) {
    return fflush((FILE*)handle);
}

static int file_seek_impl(void* handle, long offset, int whence) {
    return fseek((FILE*)handle, offset, whence);
}

stream_t create_file_stream(FILE* fp) {
    if (STREAM_UNLIKELY(!fp)) return NULL;

    stream_t s = (stream_t)malloc(sizeof(struct stream));
    if (STREAM_UNLIKELY(!s)) return NULL;

    s->read = file_read_impl;
    s->write = file_write_impl;
    s->flush = file_flush_impl;
    s->seek = file_seek_impl;
    s->eof = file_eof_impl;
    s->read_char = file_read_char_impl;
    s->handle = fp;
    s->type = FILE_STREAM;
    return s;
}

size_t file_stream_read(stream_t s, void* STREAM_RESTRICT ptr, size_t size, size_t count) {
    STREAM_ASSERT(s && s->type == FILE_STREAM);
    FILE* fp = (FILE*)s->handle;
    fseek(fp, 0, SEEK_SET);
    return fread(ptr, size, count, fp);
}

/* -------------------------------------------------------------------------
 * String Stream VTable Implementations
 * ---------------------------------------------------------------------- */

static ssize_t string_read_impl(void* handle, void* ptr, size_t n) {
    string_stream* ss = (string_stream*)handle;
    if (ss->pos >= ss->size) return 0;

    size_t avail = ss->size - ss->pos;
    if (n > avail) n = avail;

    memcpy(ptr, ss->data + ss->pos, n);
    ss->pos += n;
    return (ssize_t)n;
}

static ssize_t string_write_impl(void* handle, const void* ptr, size_t n) {
    string_stream* ss = (string_stream*)handle;
    if (STREAM_UNLIKELY(n == 0)) return 0;

    if (STREAM_UNLIKELY(n > SIZE_MAX - ss->pos - 1)) return -1;
    size_t needed_cap = ss->pos + n + 1;

    if (!string_stream_ensure_capacity(ss, needed_cap)) return -1;

    memcpy(ss->data + ss->pos, ptr, n);
    ss->pos += n;

    if (ss->pos > ss->size) { ss->size = ss->pos; }
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
    ssize_t new_pos = 0;

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = (ssize_t)ss->pos + offset;
            break;
        case SEEK_END:
            new_pos = (ssize_t)ss->size + offset;
            break;
        default:
            return -1;
    }

    if (new_pos < 0 || (size_t)new_pos > ss->size) return -1;

    ss->pos = (size_t)new_pos;
    return 0;
}

static int string_flush_impl(void* handle) {
    (void)handle;
    return 0;
}

stream_t create_string_stream(size_t initial_capacity) {
    // Single combined allocation for struct stream + string_stream struct
    stream_t s = (stream_t)malloc(sizeof(struct stream) + sizeof(string_stream));
    if (STREAM_UNLIKELY(!s)) return NULL;

    string_stream* ss = (string_stream*)(s + 1);

    if (initial_capacity <= STRING_STREAM_SSO_CAP) {
        // Use inline SSO buffer
        ss->data = ss->inline_buf;
        ss->capacity = STRING_STREAM_SSO_CAP;
    } else {
        ss->data = (char*)malloc(initial_capacity);
        if (STREAM_UNLIKELY(!ss->data)) {
            free(s);
            return NULL;
        }
        ss->capacity = initial_capacity;
    }

    ss->data[0] = '\0';
    ss->size = 0;
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

/* -------------------------------------------------------------------------
 * Fast String Helpers
 * ---------------------------------------------------------------------- */

int string_stream_write(stream_t stream, const char* str) {
    if (STREAM_UNLIKELY(!stream || stream->type != STRING_STREAM || !str)) return -1;

    size_t len = strlen(str);
    return string_stream_write_len(stream, str, len);
}

int string_stream_write_len(stream_t stream, const char* str, size_t n) {
    if (STREAM_UNLIKELY(!stream || stream->type != STRING_STREAM || !str)) return -1;

    string_stream* ss = (string_stream*)stream->handle;
    if (STREAM_UNLIKELY(n > SIZE_MAX - ss->size - 1)) return -1;

    size_t needed_cap = ss->size + n + 1;
    if (!string_stream_ensure_capacity(ss, needed_cap)) return -1;

    memcpy(ss->data + ss->size, str, n);
    ss->size += n;
    ss->data[ss->size] = '\0';

    return (int)n;
}

const char* string_stream_data(stream_t stream) {
    if (STREAM_UNLIKELY(!stream || stream->type != STRING_STREAM)) return NULL;
    return ((string_stream*)stream->handle)->data;
}

/* =========================================================================
 * Delimited Read (read_until)
 * ====================================================================== */

ssize_t read_until(stream_t stream, int delim, char* buffer, size_t buffer_size) {
    if (STREAM_UNLIKELY(!stream || !buffer || buffer_size == 0)) return -1;

    /* --- SIMD-accelerated Fast Path for String Streams --- */
    if (STREAM_LIKELY(stream->type == STRING_STREAM)) {
        string_stream* ss = (string_stream*)stream->handle;
        if (STREAM_UNLIKELY(ss->pos >= ss->size)) return -1;

        size_t avail = ss->size - ss->pos;
        size_t max_read = buffer_size - 1;
        size_t check_len = avail < max_read ? avail : max_read;

        const char* p = ss->data + ss->pos;
        const char* match = (const char*)memchr(p, delim, check_len);

        size_t copy_len = 0;
        if (match) {
            copy_len = (size_t)(match - p);
            memcpy(buffer, p, copy_len);
            ss->pos += copy_len + 1;  // Consume delimiter
        } else {
            copy_len = check_len;
            memcpy(buffer, p, copy_len);
            ss->pos += copy_len;
        }

        buffer[copy_len] = '\0';
        return (ssize_t)copy_len;
    }

    /* --- Unlocked Buffered Fallback for FILE_STREAM --- */
    FILE* fp = (FILE*)stream->handle;
    ssize_t bytes = 0;
    size_t max_bytes = buffer_size - 1;

#if HAS_POSIX_UNLOCKED_IO
    flockfile(fp);
    while ((size_t)bytes < max_bytes) {
        int ch = getc_unlocked(fp);
        if (ch == EOF || ch == delim) break;
        buffer[bytes++] = (char)ch;
    }
    funlockfile(fp);
#else
    while ((size_t)bytes < max_bytes) {
        int ch = fgetc(fp);
        if (ch == EOF || ch == delim) break;
        buffer[bytes++] = (char)ch;
    }
#endif

    if (bytes == 0 && feof(fp)) return -1;
    buffer[bytes] = '\0';
    return bytes;
}

/* =========================================================================
 * Stream Copy Routines
 * ====================================================================== */

unsigned long string_stream_copy_fast(stream_t dst, stream_t src) {
    STREAM_ASSERT(dst && src && dst->type == STRING_STREAM && src->type == STRING_STREAM);

    string_stream* s = (string_stream*)src->handle;
    string_stream* d = (string_stream*)dst->handle;

    if (s->pos >= s->size) return 0;

    size_t n = s->size - s->pos;
    if (STREAM_UNLIKELY(n > SIZE_MAX - d->pos - 1)) return (unsigned long)-1;

    size_t needed_cap = d->pos + n + 1;
    if (!string_stream_ensure_capacity(d, needed_cap)) return (unsigned long)-1;

    memcpy(d->data + d->pos, s->data + s->pos, n);

    if (d->pos + n > d->size) { d->size = d->pos + n; }
    d->data[d->size] = '\0';

    d->pos += n;
    s->pos += n;
    return (unsigned long)n;
}

unsigned long io_copy(stream_t writer, stream_t reader) {
    if (STREAM_UNLIKELY(!writer || !reader)) return (unsigned long)-1;

    // Direct String-to-String bypass
    if (writer->type == STRING_STREAM && reader->type == STRING_STREAM) {
        return string_stream_copy_fast(writer, reader);
    }

    // Pre-allocate capacity if copying from FILE to STRING_STREAM
    if (writer->type == STRING_STREAM && reader->type == FILE_STREAM) {
        FILE* fp = (FILE*)reader->handle;
        long curr = ftell(fp);
        if (curr >= 0 && fseek(fp, 0, SEEK_END) == 0) {
            long end = ftell(fp);
            fseek(fp, curr, SEEK_SET);
            if (end > curr) { string_stream_ensure_capacity((string_stream*)writer->handle, (size_t)(end - curr) + 1); }
        }
    }

    char buf[16384];
    ssize_t nread = 0;
    unsigned long total = 0;

    while ((nread = reader->read(reader->handle, buf, sizeof(buf))) > 0) {
        ssize_t w = writer->write(writer->handle, buf, (size_t)nread);
        if (w < 0) return (unsigned long)-1;
        total += (unsigned long)w;
    }

    if (nread < 0) return (unsigned long)-1;

    writer->flush(writer->handle);
    return total;
}

unsigned long io_copy_n(stream_t writer, stream_t reader, size_t n) {
    if (STREAM_UNLIKELY(!writer || !reader)) return (unsigned long)-1;

    char buf[16384];
    ssize_t nread = 0;
    unsigned long total = 0;

    while (n > 0) {
        size_t want = n < sizeof(buf) ? n : sizeof(buf);
        nread = reader->read(reader->handle, buf, want);
        if (nread <= 0) break;

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
 * Stream Deallocation
 * ====================================================================== */

void stream_destroy(stream_t stream) {
    if (!stream) return;

    if (stream->type == FILE_STREAM) {
        FILE* fp = (FILE*)stream->handle;
        if (fp && fp != stdout && fp != stderr && fp != stdin) { fclose(fp); }
        free(stream);
    } else if (stream->type == STRING_STREAM) {
        string_stream* ss = (string_stream*)stream->handle;
        if (ss && ss->data && ss->data != ss->inline_buf) { free(ss->data); }
        // Free single contiguous block allocation containing both stream and string_stream
        free(stream);
    }
}
