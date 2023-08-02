// Directives to allow use of fseeko64 and ftello64
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#ifndef __OS_H__
#define __OS_H__

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct File File;

/**
 * @brief Open a file with name filename in given mode.
 * File * is allocated on the heap and wraps around the standard FILE*
 * pointer, tracks the name of the file, file descriptor and stats.
 *
 * Free this pointer with file_free.
 * @param filename
 * @param mode
 * @return File*
 */
File* file_open(const char* filename, const char* mode);

/**
 * @brief Attempt to obtain an advisory lock on the file.
 * If the lock is obtained, subsequent attempts to lock the file
 * will block until the lock is released.
 *
 * @param file The file to lock.
 * @param offset Offset where the lock begins.
 * @param length Size of the locked area; zero means until EOF.
 * @return Returns 0 if the lock was obtained successfully, or -1 if an error occurred.
 * Check errno for the specific error that occurred.
 * This function will fail with EAGAIN if the file is already locked by another process.
 * This function will block until the lock is obtained.
 *
 */
int file_lock(File* file, off_t offset, off_t length);

/**
 * @brief Release the advisory lock on the file.
 * If there are other processes waiting to lock the file, one of them
 * will obtain the lock and proceed.
 *
 * @param file The file to unlock.
 * @param offset Offset where the lock begins.
 * @param length Size of the locked area; zero means until EOF.
 * @return Returns 0 if the lock was released successfully, or -1 if an error occurred.
 * Check errno for the specific error that occurred.
 *
 */
int file_unlock(File* file, off_t offset, off_t length);

// Get pointer to the underlying standard FILE object.
FILE* file_get_ptr(File* file);

// Get underlying file descriptor.
int file_get_fd(File* file);

// Close the file.
int file_close(File* file);

/**
 * @brief Free memory pointed to by file.
 *If file is null, this function does nothing.
 *If the underlying file descriptor is open, it will be closed.
 *
 * @param file
 * @return void
 */
void file_free(File* file);

/**
 * @brief Copy contents of src into dst.
 *
 * @param src Source file.
 * @param dst Destination file.
 * @return long The number of bytes copied.
 */
long file_copy(File* src, File* dst);

/**
 * @brief Move file into destination folder.
 *
 * @param file File to move.
 * @param newdir Destination directory.
 * @return Returns 0 is file was moved successfully or > 0 if an error occured.
 */
int file_move(File* file, const char* newdir);

/**
 * @brief Returns 0 if file stats set successfully.
 *
 * @param file
 * @param stats
 * @return 0 if successful.
 */
int file_stats(File* file, struct stat* stats);

/**
 * @brief Truncate file from offset.
 *
 * @param file
 * @param length
 * @return int
 */
int file_truncate(File* file, off_t offset);

/**
 * @brief Read contents of the file into buffer.
 * Read up to bufsize.
 *
 * @param file
 * @param buffer
 * @param bufsize
 * @return Returns -1 for error or number of bytes read.
 */
int file_read(File* file, char* buffer, long bufsize);

// Read content of the file all at once.
// Returns allocated file bytes or NULL. Must be freed after use.
// This is inefficient(and may fail) and will waste memory for very large files.
char* file_readall(File* file);

/**
 * @brief Write nbytes of data to file.
 *
 * @param file
 * @param data
 * @return number of bytes written if write was successful or -1 if it failed.
 */
size_t file_write(File* file, char* data);

/**
 * @brief Returns the file size in bytes.
 * The max size returned is correct up to 2GB since ftell returns
 * a long int. If you want to get very large file, use @ref #file_size_large
 *
 * @param file
 * @return long
 */
long file_size(File* file);

// Set file cursor to the beginning of file.
void file_rewind(File* file);

// Get file size for a large file.
__off64_t file_size_large(File* file);

// Delete the file. Returns 0 if successful.
int file_remove(File* file);

#endif /* __OS_H__ */
