/**
 * @file stdstreams.h
 * @brief Standard stream handling utilities.
 *
 * Two-layer API:
 *   Layer 1 — type-specialized fast paths  (string_stream_copy_fast, etc.)
 *   Layer 2 — generic fallback             (io_copy, io_copy_n, read_until)
 *
 * Error contract (unified, POSIX-style):
 *   > 0  → bytes read / written
 *     0  → EOF
 *    -1  → error
 *
 * All allocations have been strictly optimized to prevent fragmentation and
 * excessive malloc overhead. String streams reliably guarantee correct
 * trailing NUL-terminators for unsafe downstream processing.
 */

#ifndef E8CAA280_1C10_4862_B560_47F74D754175
#define E8CAA280_1C10_4862_B560_47F74D754175

#include "platform.h"

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Debug / release safety guards — zero overhead in release builds.
 * --------------------------------------------------------------------- */
#ifndef NDEBUG
#include <assert.h>
#define STREAM_ASSERT(x) assert(x)
#else
#define STREAM_ASSERT(x) ((void)0)
#endif

/* -----------------------------------------------------------------------
 * Terminal helpers
 * --------------------------------------------------------------------- */

/**
 * @brief Read a line from stdin, optionally printing a prompt first.
 *
 * Strips the trailing newline. Overflow characters are safely drained
 * from the input buffer.
 *
 * @param prompt     Optional prompt string (can be NULL).
 * @param buffer     Destination buffer.
 * @param buffer_len Total size of the buffer.
 * @return true on success, false on EOF / error.
 */
bool readline(const char* prompt, char* buffer, size_t buffer_len);

/**
 * @brief Read a password from the terminal with echo disabled.
 *
 * Uses termios on POSIX systems, and Console API on Win32.
 *
 * @param prompt     Optional prompt string (can be NULL).
 * @param buffer     Destination buffer.
 * @param buffer_len Total size of the buffer.
 * @return The number of characters stored in @p buffer, or -1 on error.
 */
int getpassword(const char* prompt, char* buffer, size_t buffer_len);

/* -----------------------------------------------------------------------
 * Stream type definitions
 * --------------------------------------------------------------------- */

/**
 * @brief Opaque stream handle.
 *
 * Wraps FILE* and in-memory string buffers behind a uniform interface.
 * Callers must never dereference the pointer directly.
 */
typedef struct stream* stream_t;

/**
 * @struct string_stream
 * @brief String stream internal state structure.
 *
 * Exposed only to allow callers to embed it or calculate offsets. Memory
 * is highly optimized via unified contiguous allocation internally. Always
 * treat every field as strictly private — use the public API instead.
 */
typedef struct string_stream {
    char* data;      /**< Dynamically allocated buffer, always NUL-terminated */
    size_t size;     /**< Current string length in bytes (excluding NUL) */
    size_t capacity; /**< Allocated physical capacity (including space for NUL) */
    size_t pos;      /**< Current read/write seek cursor position */
} string_stream;

/* -----------------------------------------------------------------------
 * Stream result type — mirrors POSIX read/write semantics
 * --------------------------------------------------------------------- */
typedef ssize_t stream_result_t;

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

/**
 * @brief Wrap an existing FILE* in a standardized stream context.
 *
 * Ownership of @p fp is conditionally taken only when it is not one of
 * the standards (stdin / stdout / stderr).
 *
 * @param fp Target file pointer.
 * @return Stream handle, or NULL on allocation failure.
 */
stream_t create_file_stream(FILE* fp);

/**
 * @brief Allocate a new in-memory string stream.
 *
 * Combines allocations for caching efficiency. Automatically manages growth
 * sizes exponentially to skip redundant allocations.
 *
 * @param initial_capacity Desired starting size (can be 0).
 * @return Stream handle, or NULL on allocation failure.
 */
stream_t create_string_stream(size_t initial_capacity);

/**
 * @brief Destroy a stream and release all connected resources.
 *
 * For file streams, the underlying FILE* is safely closed unless it belongs
 * to the standard standard handles (stdin/stdout/stderr).
 *
 * @param stream Stream to destroy.
 */
void stream_destroy(stream_t stream);

/* -----------------------------------------------------------------------
 * Seeking
 * --------------------------------------------------------------------- */

/**
 * @brief Seek within a stream exactly matching POSIX fseek(3) semantics.
 *
 * @param stream Stream handle.
 * @param offset Relative byte offset.
 * @param whence Target relative origin (SEEK_SET, SEEK_CUR, SEEK_END).
 * @return 0 on success, -1 on bounds violation or error.
 */
int stream_seek(stream_t stream, long offset, int whence);

/* -----------------------------------------------------------------------
 * File-stream helpers
 * --------------------------------------------------------------------- */

/**
 * @brief Read up to @p count objects of @p size bytes from a *file* stream
 *        into @p ptr, rewinding to the beginning first.
 *
 * @param s     Source file stream.
 * @param ptr   Destination buffer pointer.
 * @param size  Size in bytes of an individual unit block.
 * @param count Amount of items to fetch.
 * @return Number of objects read, 0 on EOF, (size_t)-1 on error.
 */
size_t file_stream_read(stream_t s, void* restrict ptr, size_t size, size_t count);

/* -----------------------------------------------------------------------
 * String-stream helpers
 * --------------------------------------------------------------------- */

/**
 * @brief Append the NUL-terminated string @p str to @p stream.
 *
 * The internal position cursor is explicitly *not* advanced (append
 * semantics). Safe to execute irrespective of current seek states.
 *
 * @param stream Target string stream.
 * @param str    NUL-terminated string literal.
 * @return The number of bytes appended, or -1 on allocation failure.
 */
int string_stream_write(stream_t stream, const char* str);

/**
 * @brief Append the NUL-terminated string @p str to @p stream.
 *  This is faster than string_stream_write when the length of @p str is already known.
 * The internal position cursor is explicitly *not* advanced (append
 * semantics). Safe to execute irrespective of current seek states.
 *
 * @param stream Target string stream.
 * @param str    NUL-terminated string literal.
 * @param n      Maximum number of bytes to append from @p str (excluding NUL).
 * @return The number of bytes appended, or -1 on allocation failure.
 */
int string_stream_write_len(stream_t stream, const char* str, size_t n);

/**
 * @brief Fetch a read-only view of the underlying strictly NUL-terminated
 *        string data.
 *
 * @param stream String stream handle.
 * @return Direct buffer pointer or NULL if type invalid.
 */
const char* string_stream_data(stream_t stream);

/* -----------------------------------------------------------------------
 * Delimited read
 * --------------------------------------------------------------------- */

/**
 * @brief Read from @p stream into @p buffer until @p delim is found.
 *
 * Heavily optimized out internally if the target matches a string stream.
 * The buffer is reliably NUL-terminated. Delimiter is excluded from results.
 *
 * @param stream       Source stream.
 * @param delim        Delimiter character boundary trigger.
 * @param buffer       Destination array — continuously NUL-terminated.
 * @param buffer_size  Total cap of @p buffer.
 * @return characters stored (≥ 0), or -1 on error / initial EOF.
 */
ssize_t read_until(stream_t stream, int delim, char* buffer, size_t buffer_size);

/* -----------------------------------------------------------------------
 * Layer 1 — type-specialised fast paths
 * --------------------------------------------------------------------- */

/**
 * @brief Direct memory-bound string to string copy without generic dispatch.
 *
 * Copy all bytes linearly from @p src string to @p dst string.
 *
 * @param dst Destination string stream.
 * @param src Target source string stream.
 * @return Bytes copied, or (unsigned long)-1 on allocation failure.
 */
unsigned long string_stream_copy_fast(stream_t dst, stream_t src);

/* -----------------------------------------------------------------------
 * Layer 2 — generic copy
 * --------------------------------------------------------------------- */

/**
 * @brief Copy contents universally from @p reader into @p writer.
 *
 * Uses optimal vtable-bypassing fast-paths seamlessly if types match.
 *
 * @param writer Destination generalized stream.
 * @param reader Target generalized source stream.
 * @return Total bytes written, or (unsigned long)-1 on error.
 */
unsigned long io_copy(stream_t writer, stream_t reader);

/**
 * @brief Copy tightly capped chunk up to @p n bytes.
 *
 * @param writer Destination generalized stream.
 * @param reader Target generalized source stream.
 * @param n      Maximum ceiling of bytes to shift.
 * @return Total bytes written (≤ n), or (unsigned long)-1 on error.
 */
unsigned long io_copy_n(stream_t writer, stream_t reader, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* E8CAA280_1C10_4862_B560_47F74D754175 */
