#ifndef CB4BB606_713F_4BBD_9F44_DB223464EB5A
#define CB4BB606_713F_4BBD_9F44_DB223464EB5A

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifdef _WIN32    // Windows
#include <io.h>  // For _get_osfhandle, _filelengthi64
#include <windows.h>
// Define ssize_t for Windows if not using a recent SDK/compiler that does
#if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
typedef intptr_t ssize_t;
#define _SSIZE_T_DEFINED
#endif

#else  // Posix
#include <fcntl.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// file_t structure that encapsulates file operations
// in a platform-independent way.
// Adds support for locking, unlocking, asyncronous read and write etc.
typedef struct file_t file_t;

/**
 * @brief Opens a file and allocated & populate the file_t structure.
 *
 * @param filename The path to the file to open.
 * @param mode The mode string (e.g., "r", "wb+", "a"). See fopen documentation.
 * @return Pointer to allocated file_t structure on success, NULL on failure (errno may be set).
 */
file_t* file_open(const char* filename, const char* mode);

/**
 * @brief Gets the current size of the file.
 * This queries the OS and is generally more up-to-date than the cached file->size.
 *
 * @param file Pointer to the opened file_t structure.
 * @return The current file size in bytes, or -1 on error (errno is set).
 */
int64_t file_get_size(file_t* file);

/**
 * @brief Get the file descriptor associated with the open file.
 */
int file_getfd(file_t* file);

/**
 * @brief Gets the current size of the file.
 *
 * @param filename Absolute path to the file.
 * @return The current file size in bytes, or -1 on error (errno is set).
 */
int64_t get_file_size(const char* filename);

/**
 * @brief Checks the error indicator for the stream. Wrapper around ferror.
 * @param file Pointer to the opened file_t structure.
 * @return Non-zero if error indicator is set, 0 otherwise.
 */
int file_error(file_t* file);

/**
 * @brief Closes the file, releases resources, and resets the file_t structure.
 * @param file Pointer to the file_t structure.
 */
void file_close(file_t* file);

/**
 * @brief Flushes buffered data associated with the file stream to the OS.
 * @param file Pointer to the opened file_t structure.
 * @return 0 on success, -1 on failure (errno is set).
 */
int file_flush(file_t* file);

/**
 * @brief Truncates or extends the file to a specified length.
 * Requires the file to be opened in a mode that permits writing.
 *
 * @param file Pointer to the opened file_t structure.
 * @param length The desired length of the file in bytes.
 * @return 0 on success, -1 on failure (errno is set).
 */
int file_truncate(file_t* file, int64_t length);

// Returns the human-readable size of a file up to TB.
// The size is stored in the buf parameter.
bool filesize_tostring(size_t size, char* buf, size_t len);

/**
 * @brief Sets the file position indicator. Wrapper around fseek.
 * @param file Pointer to the opened file_t structure.
 * @param offset Offset value.
 * @param origin Position from where offset is added (SEEK_SET, SEEK_CUR, SEEK_END).
 * @return 0 on success, -1 on failure (errno is set).
 */
int file_seek(file_t* file, long offset, int origin);

/**
 * @brief Gets the current value of the file position indicator. Wrapper around ftell.
 * @param file Pointer to the opened file_t structure.
 * @return The current file position, or -1L on error (errno is set).
 */
long file_tell(file_t* file);

/**
 * @brief Sets the file position indicator to the beginning of the file.
 * @param file Pointer to the opened file_t structure.
 * @return 0 on success, -1 on failure (errno is set).
 */
int file_rewind(file_t* file);

/**
 * @brief Reads data from the file. Wrapper around fread.
 * @param file Pointer to the opened file_t structure.
 * @param buffer Pointer to the buffer to store read data.
 * @param size Size of each element to read.
 * @param count Number of elements to read.
 * @return Number of elements successfully read. Check ferror or feof for details on partial reads/errors.
 */
size_t file_read(file_t* file, void* buffer, size_t size, size_t count);

/**
 * @brief Writes data to the file. Wrapper around fwrite. Does NOT automatically flush.
 * @param file Pointer to the opened file_t structure.
 * @param buffer Pointer to the data to write.
 * @param size Size of each element to write.
 * @param count Number of elements to write.
 * @return Number of elements successfully written. Check ferror for details on errors.
 */
size_t file_write(file_t* file, const void* buffer, size_t size, size_t count);

/**
 * @brief Writes a null-terminated string to the file (excluding the null terminator).
 * @param file Pointer to the opened file_t structure.
 * @param str The null-terminated string to write.
 * @return Number of bytes successfully written.
 */
size_t file_write_string(file_t* file, const char* str);

// --- Positional/Async (Misnomer on POSIX) I/O ---
// NOTE: The POSIX versions are NOT asynchronous. They are positional (atomic seek+read/write).
//       The Windows versions simulate blocking asynchronous I/O.

/**
 * @brief Writes data to a specific offset in the file without changing the current file pointer.
 *        On POSIX, this uses pwrite (atomic, synchronous).
 *        On Windows, this simulates blocking asynchronous write using WriteFile + OVERLAPPED.
 *
 * @param file Pointer to the opened file_t structure.
 * @param buffer Buffer containing data to write.
 * @param size Number of bytes to write.
 * @param offset Offset in the file to write to.
 * @return Number of bytes successfully written, or -1 on error (errno is set).
 */
ssize_t file_pwrite(file_t* file, const void* buffer, size_t size, int64_t offset);

/**
 * @brief Reads data from a specific offset in the file without changing the current file pointer.
 *        On POSIX, this uses pread (atomic, synchronous).
 *        On Windows, this simulates blocking asynchronous read using ReadFile + OVERLAPPED.
 *
 * @param file Pointer to the opened file_t structure.
 * @param buffer Buffer to store read data.
 * @param size Number of bytes to read.
 * @param offset Offset in the file to read from.
 * @return Number of bytes successfully read, or -1 on error (errno is set).
 *         Returns 0 on EOF if offset is at or beyond EOF.
 */
ssize_t file_pread(file_t* file, void* buffer, size_t size, int64_t offset);

/**
 * @brief Reads the entire file content into a newly allocated buffer.
 *
 * Gets the current file size, allocates memory, reads the content, and returns the buffer.
 * The file pointer is usually left at the end of the file after this operation.
 *
 * @param file Pointer to the opened file_t structure.
 * @return A pointer to the allocated buffer containing the file content,
 *         or NULL on error (e.g., allocation failure, read error, zero size).
 *         The caller is responsible for free()ing the returned buffer.
 *         errno may be set.
 */
void* file_readall(file_t* file);

/**
 * @brief Attempts to acquire an advisory, non-blocking, exclusive (write) lock on the entire file.
 *
 * @param file Pointer to the opened file_t structure.
 * @return true if the lock was acquired successfully or was already held, false otherwise.
 *         On failure to acquire (e.g., file already locked by another process), returns false
 *         and sets errno to EACCES or EAGAIN (POSIX) or similar (Windows).
 */
bool file_lock(file_t* file);

/**
 * @brief Releases an advisory lock held on the file.
 * @param file Pointer to the opened file_t structure.
 * @return true if the lock was released successfully or if no lock was held, false on error.
 */
bool file_unlock(file_t* file);

/**
 * @brief Renames the file associated with the file_t structure.
 * The file is closed first if it is open. The internal filename is updated on success.
 *
 * @param file Pointer to the file_t structure.
 * @param newname The new path/name for the file.
 * @return 0 on success, -1 on failure (errno may be set).
 */
int file_rename(file_t* file, const char* newname);

/**
 * @brief Copies the content of one open file to another open file.
 * Reads from the current position of 'src' until EOF, writes to 'dst'.
 * Flushes 'dst' after copying. Leaves file pointers at the end.
 * Does NOT manage opening/closing files.
 *
 * @param src Pointer to the source file_t structure (must be open for reading).
 * @param dst Pointer to the destination file_t structure (must be open for writing).
 * @return 0 on success, -1 on error (errno may be set).
 */
int file_copy(file_t* file, file_t* dst);

/**
 * @brief Memory maps a portion of the file.
 * File should be open. The required access (read/write) depends on usage.
 *
 * @param file Pointer to the opened file_t structure.
 * @param length The number of bytes to map, starting from the beginning of the file.
 * @param read_access If true, request read access to the mapping.
 * @param write_access If true, request write access to the mapping.
 * @return Pointer to the mapped memory region, or NULL on failure (errno may be set).
 */
void* file_mmap(file_t* file, size_t length, bool read_access, bool write_access);

/**
 * @brief Unmaps a previously memory-mapped region.
 *
 * @param addr Pointer to the start of the mapped region (returned by file_mmap).
 * @param length The length of the mapping (must match the length used in file_mmap on POSIX).
 * @return 0 on success, -1 on failure (errno may be set).
 */
int file_munmap(void* addr, size_t length);

// Returns true is path is a regular file.
bool is_file(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* CB4BB606_713F_4BBD_9F44_DB223464EB5A */
