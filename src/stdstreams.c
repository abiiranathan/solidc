#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 1
#endif

#include "../include/stdstreams.h"

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

// Read a line from stdin with a prompt
bool readline(const char* prompt, char* buffer, size_t buffer_len) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (fgets(buffer, (int)buffer_len, stdin) == NULL) {
        return false;
    }

    buffer[strcspn(buffer, "\n")] = '\0';  // Remove trailing newline

    if (strlen(buffer) >= buffer_len - 1) {
        char c;
        while ((c = (char)getchar()) != EOF) {
            if (c == '\n')
                break;
        }
    }
    return true;
}

// getpassword reads a password from the terminal with echo disabled.
// If the prompt is not NULL, it is printed before reading the password.
// uses termios on Linux and Windows API on Windows.
// The password is stored in the buffer and the function returns the number of
// characters read or -1 on error. The buffer is null-terminated.
int getpassword(const char* prompt, char* buffer, size_t buffer_len) {
#ifdef _WIN32
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    DWORD count;
    DWORD i;

    if (hStdin == INVALID_HANDLE_VALUE) {
        return -1;
    }

    if (!GetConsoleMode(hStdin, &mode)) {
        return -1;
    }

    if (!SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT)) {
        return -1;
    }

    for (i = 0; i < buffer_len - 1; i++) {
        if (!ReadConsoleA(hStdin, &buffer[i], 1, &count, NULL) || count == 0) {
            break;
        }

        if (buffer[i] == '\n' || buffer[i] == '\r') {
            buffer[i] = '\0';
            break;
        }
    }

    buffer[i] = '\0';

    if (!SetConsoleMode(hStdin, mode)) {
        return -1;
    }
    // Add a newline
    printf("\n");
    return i;
#else
    struct termios old, new;
    int nread;

    // Turn off echoing
    if (tcgetattr(fileno(stdin), &old) != 0) {
        return -1;
    }

    new = old;
    new.c_lflag &= (tcflag_t)~ECHO;

    if (tcsetattr(fileno(stdin), TCSAFLUSH, &new) != 0) {
        return -1;
    }

    // Read the password
    nread = readline(prompt, buffer, buffer_len);

    // Restore terminal
    if (tcsetattr(fileno(stdin), TCSAFLUSH, &old) != 0) {
        return -1;
    }
    // Add a newline
    printf("\n");
    return nread;
#endif
}

enum stream_type {
    INVALID_STREAM = -1,
    FILE_STREAM    = 0,
    STRING_STREAM,
};

// stream_t type wraps common FILE* operations to be work with
// strings as a files.
// Any type can implement
struct stream {
    // Function pointer to from the stream
    unsigned long (*read)(void* handle, size_t size, size_t count, void* ptr);

    // Function pointer for reading a single character from the stream
    int (*read_char)(void* handle);

    // Function pointer for writing to the stream (if applicable)
    // similar API as fwrite
    size_t (*write)(const void* ptr, size_t size, size_t count, void* handle);

    // Function pointer to check if the end of the stream is reached
    int (*eof)(void* handle);

    // Function pointer for seeking within the stream (if applicable)
    int (*seek)(void* handle, long offset, int whence);

    // flush the stream
    int (*flush)(void* handle);

    // The concrete stream implementation
    void* handle;

    // Store the type of stream
    enum stream_type type;
};

static size_t file_write(const void* ptr, size_t size, size_t count, void* handle) {
    return fwrite(ptr, size, count, (FILE*)handle);
}

static unsigned long file_read(void* handle, size_t size, size_t count, void* ptr) {
    return fread(ptr, size, count, (FILE*)handle);
}

size_t file_stream_read(stream_t s, void* restrict ptr, size_t size, size_t count) {
    if (s->type == FILE_STREAM) {
        s->seek(s->handle, 0, SEEK_SET);
        return fread(ptr, size, count, (FILE*)s->handle);
    }

    fprintf(stderr, "Reading from a non file stream\n");
    return EOF;
}

stream_t create_file_stream(FILE* fp) {
    stream_t stream = malloc(sizeof(struct stream));
    if (!stream) {
        return NULL;
    }

    stream->read      = file_read;
    stream->write     = file_write;
    stream->flush     = (int (*)(void*))fflush;
    stream->seek      = (int (*)(void*, long int, int))fseek;
    stream->eof       = (int (*)(void*))feof;
    stream->read_char = (int (*)(void*))fgetc;
    stream->handle    = fp;
    stream->type      = FILE_STREAM;
    return stream;
}

ssize_t read_until(stream_t stream, int delim, char* buffer, size_t buffer_size) {
    ssize_t bytes_read = 0;
    int ch;
    while ((ch = stream->read_char(stream->handle)) != EOF && ch != delim && bytes_read < (int)buffer_size - 1) {
        buffer[bytes_read++] = (char)ch;
    }

    // Handle errors (EOF or read error)
    if (ch == EOF && stream->eof(stream->handle)) {
        return -1;  // Indicate error
    }

    // Null-terminate the string if there's space
    if (bytes_read < (int)(buffer_size - 1)) {
        buffer[bytes_read] = '\0';
    } else {
#ifndef NDEBUG
        // Handle buffer overflow
        puts("[read_upto_char]: Buffer overflow");
        printf("read %zd bytes, buffer size %zu\n", bytes_read, buffer_size);
#endif
        buffer[buffer_size - 1] = '\0';  // truncate the string
    }
    return bytes_read;
}

unsigned long io_copy(stream_t writer, stream_t reader) {
    char buffer[4096];
    unsigned long nread         = 0;
    unsigned long total_written = 0;
    reader->seek(reader->handle, 0, SEEK_SET);

    while ((nread = reader->read(reader->handle, 1, sizeof(buffer), buffer)) > 0) {
        total_written += writer->write(buffer, 1, nread, writer->handle);
    }

    // Flush the destination stream
    writer->flush(writer->handle);
    return total_written;
}

// Copy contents of one reader into writer, writing up to n bytes into the
// writer.
unsigned long io_copy_n(stream_t writer, stream_t reader, size_t n) {
    char buffer[4096];
    unsigned long nread         = 0;
    unsigned long total_written = 0;
    reader->seek(reader->handle, 0, SEEK_SET);

    while (n > 0 && (nread = reader->read(reader->handle, 1, sizeof(buffer), buffer)) > 0) {
        if ((nread > n)) {
            nread = n;
        }

        total_written += writer->write(buffer, 1, nread, writer->handle);
        n -= nread;
    }

    // Flush the destination stream
    writer->flush(writer->handle);
    return total_written;
}

// concrete implementation for reading from a string
static unsigned long inner_string_stream_read(void* handle, size_t size, size_t count, void* ptr) {
    (void)size;  // size is always 1 for a string

    string_stream* ss = (string_stream*)handle;
    size_t string_len = str_len(ss->str);

    if (count == 0 || ss->pos >= string_len) {
        return EOF;
    }

    // Check for size_t overflow when calculating bytes_to_read
    if (count > SIZE_MAX) {
        // Prevent overflow, return 0 as we can't handle such a large read
        return EOF;
    }

    // make sure we don't read past the end of the string
    if (ss->pos + count > string_len) {
        count = string_len - ss->pos;  // read only the remaining bytes
    }

    // Perform the read operation
    char* data = (char*)str_data(ss->str);
    memcpy(ptr, data + ss->pos, count);
    ss->pos += count;

    return count;
}

// implementation for reading a single character from a string
static int string_stream_read_char(void* handle) {
    string_stream* ss = (string_stream*)handle;
    if (ss->pos >= str_len(ss->str)) {
        return EOF;
    }
    return str_data(ss->str)[ss->pos++];
}

// Returns whether the end of the string has been reached
static int string_stream_eof(void* handle) {
    string_stream* ss = (string_stream*)handle;
    return ss->pos >= str_len(ss->str);
}

// implementation for writing to a string. For a char *, size must be 1.
static size_t inner__string_stream_write(const void* ptr, size_t size, size_t count, void* handle) {
    string_stream* ss    = (string_stream*)handle;
    size_t bytes_written = 0;
    size_t total_bytes   = size * count;
    bool resized         = false;

    resized = str_resize(&ss->str, str_len(ss->str) + total_bytes);
    if (!resized) {
        return 0;
    }

    char* data = (char*)str_data(ss->str);
    for (size_t i = 0; i < total_bytes; i++) {
        data[ss->pos++] = ((char*)ptr)[i];
        bytes_written++;
    }
    return bytes_written / size;
}

static int string_stream_seek(void* handle, long offset, int whence) {
    string_stream* ss = (string_stream*)handle;
    size_t length     = str_len(ss->str);
    size_t new_pos    = ss->pos;

    switch (whence) {
        case SEEK_SET:
            if (offset < 0 || (size_t)offset > length)
                return EOF;
            new_pos = (size_t)offset;
            break;
        case SEEK_CUR:
            if ((offset < 0 && (size_t)(-offset) > ss->pos) || (offset > 0 && ss->pos + (size_t)offset > length))
                return EOF;
            new_pos += (size_t)offset;
            break;
        case SEEK_END:
            if ((offset < 0 && (size_t)(-offset) > length) || (offset > 0))
                return EOF;
            new_pos = length + (size_t)offset;
            break;
        default:
            return EOF;
    }

    ss->pos = new_pos;
    return 0;
}

static int string_flush(void* handle) {
    (void)handle;
    return 0;
}

// copy the string to the stream. The position is not incremented.
int string_stream_write(stream_t stream, const char* str) {
    // copy over the string to the stream.
    // we don't have to increment the position.
    string_stream* ss = (string_stream*)stream->handle;
    if (!str_append(&ss->str, str)) {
        return -1;
    }
    return (int)strlen(str);
}

const char* string_stream_data(stream_t stream) {
    if (stream->type == STRING_STREAM) {
        string_stream* ss = (string_stream*)stream->handle;
        return str_data(ss->str);
    } else {
        return NULL;
    }
}

static void inner__free_file_stream(stream_t stream) {
    if (!stream)
        return;
    FILE* fp = (FILE*)stream->handle;

    if (!(fp == stdout || fp == stderr || fp == stdin)) {
        fclose(fp);
    }

    free(stream);
    stream = NULL;
}

static void inner__free_string_stream(stream_t stream) {
    if (!stream)
        return;

    if (stream->handle) {
        free(stream->handle);
        stream->handle = NULL;
    }

    free(stream);
    stream = NULL;
}

void stream_destroy(stream_t stream) {
    switch (stream->type) {
        case FILE_STREAM:
            inner__free_file_stream(stream);
            break;
        case STRING_STREAM:
            inner__free_string_stream(stream);
            break;
        case INVALID_STREAM:
            // Should never happen
            fprintf(stderr, "Invalid stream");
            exit(EXIT_FAILURE);
    }
}

// Create a stream from a string from a char *
stream_t create_string_stream(size_t initial_capacity) {
    stream_t stream = malloc(sizeof(struct stream));
    if (!stream) {
        return NULL;
    }

    string_stream* ss = (string_stream*)malloc(sizeof(string_stream));
    if (ss == NULL) {
        free(stream);
        return NULL;
    }

    ss->str = str_new(initial_capacity);
    if (ss->str == NULL) {
        free(stream);
        free(ss);
        return NULL;
    }

    ss->pos           = 0;
    stream->read      = inner_string_stream_read;
    stream->flush     = (int (*)(void*))string_flush;
    stream->read_char = string_stream_read_char;
    stream->eof       = string_stream_eof;
    stream->write     = inner__string_stream_write;
    stream->seek      = string_stream_seek;
    stream->handle    = ss;
    stream->type      = STRING_STREAM;
    return stream;
}
