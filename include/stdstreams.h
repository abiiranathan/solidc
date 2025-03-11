#ifndef E8CAA280_1C10_4862_B560_47F74D754175
#define E8CAA280_1C10_4862_B560_47F74D754175
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "cstr.h"

// Read a line from stdin with a prompt
bool readline(const char* prompt, char* buffer, size_t buffer_len);

// getpassword() reads a password from the terminal with echo disabled.
// uses termios on Linux and Windows API on Windows.
// The password is stored in the buffer and the function returns the number
// of characters read. The buffer is null-terminated.
int getpassword(const char* prompt, char* buffer, size_t buffer_len);

// stream_t type wraps common FILE* operations to be work with strings as a
// files. Any type can implement read, write, seek, eof, flush, read_char.
typedef struct stream* stream_t;

// Create a stream from a file pointer
stream_t create_file_stream(FILE* fp);

// Free a stream created with create_string_stream or create_file_stream
// and close the underlying file FILE* streams.
// If the underlying FILE* is stdin/stdout/stderr, they are not closed.
void stream_destroy(stream_t stream);

// Read a character from the stream until a delimiter is found or the buffer
// is full. Returns the number of characters read or -1 on error. The buffer
// is null-terminated. Create implementation for reading from a file using
// create_file_stream helper function and for reading from a string using
// create_string_stream.
ssize_t read_until(stream_t stream, int delim, char* buffer, size_t buffer_size);

typedef struct string_stream {
    Arena* arena;
    cstr* str;
    size_t pos;
} string_stream;

// Create and initialize a stream from a string. Free the stream with
// free_string_stream.
stream_t create_string_stream(size_t initial_capacity);

// copy the string to the stream. The position is not incremented.
int string_stream_write(stream_t stream, const char* str);

// Return the data in the string stream.
const char* string_stream_data(stream_t stream);

// Copy the contents of one stream to another.
// Inspired by the golang io.Copy function.
// Both streams must be valid. Invalid streams are checked with
// is_valid_stream if the program is compiled with assertions enabled.
// Compile with -DNDEBUG to disable assertions. Returns the number of bytes
// copied or -1 on error.
unsigned long io_copy(stream_t writer, stream_t reader);

// Copy contents of one reader into writer, writing up to n bytes into the
// writer.
unsigned long io_copy_n(stream_t writer, stream_t reader, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* E8CAA280_1C10_4862_B560_47F74D754175 */
