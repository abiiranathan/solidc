#ifndef CB4BB606_713F_4BBD_9F44_DB223464EB5A
#define CB4BB606_713F_4BBD_9F44_DB223464EB5A

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// file_t structure that encapsulates file operations
// in a platform-independent way.
// Adds support for locking, unlocking, asyncronous read and write etc.
typedef struct file_t file_t;

// Open a file with the given mode.
file_t* file_open(const char* filename, const char* mode) __attribute__((warn_unused_result));

// Close the file.
void file_close(file_t* file);

// Get the file descriptor for the file.
int file_fileno(file_t* file);

// Get the FILE pointer for the file.
FILE* file_fp(file_t* file);

// Flush a file to disk
void file_flush(file_t* file);

// Returns the human-readable size of a file up to TB.
// The size is stored in the buf parameter.
bool filesize_tostring(size_t size, char* buf, size_t len);

// Get the file size in bytes for the given filename.
ssize_t file_size_char(const char* filename);

// Get the file size of the file.
ssize_t file_size(file_t* file);

// Seek to the given offset in the file.
int file_seek(file_t* file, long offset, int origin);

// Read data from the file synchronously into the buffer.
// The parameters are the same as fread.
size_t file_read(file_t* file, void* buffer, size_t size, size_t count);

// Write data in buffer to the file.
// The parameters are the same as fwrite.
size_t file_write(file_t* file, const void* buffer, size_t size, size_t count);

// Write a string to the file.
size_t file_write_string(file_t* file, const char* str);

#if defined(_WIN32)
// Write to a file asynchronously
// Returns the number of bytes written or -1 on error.
// You should call file_flush to ensure the data is written to disk before reading or before
// multiple writes.
DWORD file_awrite(file_t* file, const void* buffer, size_t size, off_t offset);
#else
// Write to a file asynchronously
// Returns the number of bytes written or -1 on error
ssize_t file_awrite(file_t* file, const void* buffer, size_t size, off_t offset);
#endif

#if defined(_WIN32)
// Read from a file asynchronously
DWORD file_aread(file_t* file, void* buffer, size_t size, off_t offset);
#else
// Read from a file asynchronously.
// Returns the number of bytes read or -1 on error.
// The parameters are the same as pread.
ssize_t file_aread(file_t* file, void* buffer, size_t size, off_t offset);
#endif

// Read the entire file into a buffer. This may be inefficient for large files.
void* file_readall(file_t* file);

// Lock a file. Returns true if successful, false otherwise.
// If the file is already locked, it returns true.
// On Windows, it uses LockFile to lock the file.
// On Unix, it uses fcntl to lock the file.
bool file_lock(file_t* file);

// Unlock the file.
bool file_unlock(file_t* file);

// Rename the file.
int file_rename(file_t* file, const char* newname);

// Copy the file to the destination.
int file_copy(file_t* file, file_t* dst);

// map the file into memory, returns the address of the mapped memory
// length is the length of the file to map, 0 means the whole file
void* file_mmap(file_t* file, size_t length);

// unmap the file from memory
int file_munmap(void* addr, size_t length);

// Returns true is path is a regular file.
bool is_file(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* CB4BB606_713F_4BBD_9F44_DB223464EB5A */
