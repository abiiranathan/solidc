// Feature test macros before including headers
#define _POSIX_C_SOURCE 200809L  // For fstat, fileno, pwrite, pread, fcntl, etc.
#define _FILE_OFFSET_BITS 64     // Ensure 64-bit off_t on 32-bit POSIX systems

#include "../include/file.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct file_t {
#ifdef _WIN32
    HANDLE file;  // file handle
#else
    FILE* file;  // file pointer
#endif
    int fd;          // file descriptor
    bool is_open;    // is the file open?
    bool is_locked;  // is the file locked?
    size_t size;     // file size
    char* filename;  // file name
} file_t;

//  -- Constants --
#ifndef HUMAN_SIZE_EPSILON
#define HUMAN_SIZE_EPSILON (1e-4)
#endif
#ifndef FILE_COPY_BUFSIZ
#define FILE_COPY_BUFSIZ (8192)
#endif

// Helper function to query OS for file size given a file descriptor.
static int64_t internal_get_filesize_from_fd(int fd);

// Helper to set errno and return -1
static int internal_set_error(int err_code);

/**
 * @brief Sets errno and returns -1. Utility for error paths.
 * @param err_code The errno value to set.
 * @return Always -1.
 */
static int internal_set_error(int err_code) {
    errno = err_code;
    return -1;
}

/**
 * @brief Get current file size given a file descriptor.
 * @param fd The file descriptor.
 * @return The file size as a 64-bit integer, or -1 on error (errno is set).
 */
static int64_t internal_get_filesize_from_fd(int fd) {
#ifdef _WIN32
    // Use _filelengthi64 for large file support on Windows
    int64_t size = _filelengthi64(fd);
    if (size == -1L) {
        // errno should be set by _filelengthi64 (e.g., EBADF)
        return -1;
    }
    return size;
#else  // POSIX
    struct stat st;
    if (fstat(fd, &st) == 0) {
        // st_size is off_t, ensured to be 64-bit by _FILE_OFFSET_BITS=64
        return (int64_t)st.st_size;
    } else {
        // errno is set by fstat
        return -1;
    }
#endif
}

// --- Public API Implementation ---

file_t* file_open(const char* filename, const char* mode) {
    file_t* file = calloc(1, sizeof(file_t));
    if (!file) {
        perror("calloc");
        return NULL;
    }

    if (!filename || !mode) {
        internal_set_error(EINVAL);
        free(file);
        return NULL;
    }

    FILE* fp = fopen(filename, mode);
    if (!fp) {
        free(file);
        // errno set by fopen
        return NULL;
    }

    // Duplicate filename
    file->filename = strdup(filename);
    if (!file->filename) {
        fclose(fp);
        free(file);
        internal_set_error(ENOMEM);  // errno likely already ENOMEM from strdup
        return NULL;
    }

    // Get file descriptor
    file->fd = fileno(fp);
    if (file->fd == -1) {
        // This can happen for non-regular files or streams
        free(file->filename);
        fclose(fp);
        free(file);
        internal_set_error(EBADF);
        return NULL;
    }

    // Get initial file size (cache it)
    int64_t current_size = internal_get_filesize_from_fd(file->fd);
    if (current_size < 0) {
        free(file->filename);
        fclose(fp);
        free(file);
        return NULL;
    } else {
        file->size = (uint64_t)current_size;
    }

#ifdef _WIN32
    // Get Windows HANDLE
    file->file = (HANDLE)(uintptr_t)_get_osfhandle(file->fd);

    if (file->file == INVALID_HANDLE_VALUE) {
        // Error getting handle, invalidates locking/async on Windows
        free(file->filename);
        fclose(fp);
        free(file);
        return NULL;
    }
#else
    file->file = fp;
#endif
    file->is_open = true;
    return file;
}

/**
 * @brief Flushes buffered data associated with the file stream to the OS.
 * @param file Pointer to the opened file_t structure.
 * @return 0 on success, -1 on failure (errno is set).
 */
int file_flush(file_t* file) {
    if (!file || !file->is_open) {
        return internal_set_error(EBADF);
    }
    if (fflush(file->file) != 0) {
        // errno set by fflush
        return -1;
    }
    return 0;
}

/**
 * @brief Closes the file, releases resources, and resets the file_t structure.
 * @param file Pointer to the file_t structure.
 */
void file_close(file_t* file) {
    if (!file || !file->is_open) {
        return;  // Nothing to do or invalid input
    }

    // Ensure file is unlocked before closing
    if (file->is_locked) {
        file_unlock(file);  // Ignore unlock error
    }

    // Close the primary handle (FILE* stream)
    // fclose() closes the underlying file descriptor/handle on both platforms.
    if (file->file) {
        fclose(file->file);
        file->file = NULL;
    }

    // Clean up other resources and reset state
    if (file->filename) {
        free(file->filename);
        file->filename = NULL;
    }

    // Reset fields to indicate closed state
    file->fd = -1;
#ifdef _WIN32
    file->file = INVALID_HANDLE_VALUE;
#endif
    file->is_open   = false;
    file->is_locked = false;
    file->size      = 0;  // Reset cached size
}

/**
 * @brief Reads data from the file. Wrapper around fread.
 * @param file Pointer to the opened file_t structure.
 * @param buffer Pointer to the buffer to store read data.
 * @param size Size of each element to read.
 * @param count Number of elements to read.
 * @return Number of elements successfully read. Check ferror or feof for details on partial reads/errors.
 */
size_t file_read(file_t* file, void* buffer, size_t size, size_t count) {
    if (!file || !file->is_open || !buffer) {
        internal_set_error(EINVAL);  // Or EBADF if file invalid
        return 0;
    }

    // clearerr(file->file);  // Optional: Clear error flags before operation
    size_t items_read = fread(buffer, size, count, file->file);
    return items_read;
}

/**
 * @brief Writes data to the file. Wrapper around fwrite. Does NOT automatically flush.
 * @param file Pointer to the opened file_t structure.
 * @param buffer Pointer to the data to write.
 * @param size Size of each element to write.
 * @param count Number of elements to write.
 * @return Number of elements successfully written. Check ferror for details on errors.
 */
size_t file_write(file_t* file, const void* buffer, size_t size, size_t count) {
    if (!file || !file->is_open || !buffer) {
        internal_set_error(EINVAL);  // Or EBADF
        return 0;
    }

    // clearerr(file->file); // Optional
    size_t items_written = fwrite(buffer, size, count, file->file);

    // CRITICAL: Do NOT automatically update file->size here.
    // File size changes are complex (overwrite, append, sparse files).
    // Let the user call file_get_size() if they need an updated size.
    // CRITICAL: Do NOT automatically flush here. Let the user call file_flush().
    // Caller should use file_error() to check status if items_written < count.
    return items_written;
}

/**
 * @brief Writes a null-terminated string to the file (excluding the null terminator).
 * @param file Pointer to the opened file_t structure.
 * @param str The null-terminated string to write.
 * @return Number of bytes successfully written.
 */
size_t file_write_string(file_t* file, const char* str) {
    if (!str) {
        internal_set_error(EINVAL);
        return 0;
    }
    size_t len = strlen(str);
    if (len == 0) {
        return 0;  // Nothing to write
    }
    return file_write(file, str, 1, len);
}

/**
 * @brief Gets the current size of the file.
 * This queries the OS and is generally more up-to-date than the cached file->size.
 *
 * @param file Pointer to the opened file_t structure.
 * @return The current file size in bytes, or -1 on error (errno is set).
 */
int64_t file_get_size(file_t* file) {
    if (!file || !file->is_open) {
        return internal_set_error(EBADF);
    }

    // Query the size using the file descriptor
    int64_t current_size = internal_get_filesize_from_fd(file->fd);

    // Optionally update the cached size on success
    if (current_size >= 0)
        file->size = (uint64_t)current_size;
    return current_size;
}

/**
 * @brief Gets the current size of the file.
 *
 * @param filename Absolute path to the file.
 * @return The current file size in bytes, or -1 on error (errno is set).
 */
int64_t get_file_size(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    int64_t size = internal_get_filesize_from_fd(fd);
    close(fd);
    return size;
}

/**
 * @brief Get the file descriptor associated with the open file.
 */
int file_getfd(file_t* file) {
    return file->fd;
}

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
void* file_readall(file_t* file) {
    if (!file || !file->is_open) {
        internal_set_error(EBADF);
        return NULL;
    }

    // Get current size (more reliable than cached size)
    int64_t current_size64 = file_get_size(file);
    if (current_size64 < 0) {
        // Error getting size (errno set by file_get_size)
        return NULL;
    }
    if (current_size64 == 0) {
        // Return NULL or empty buffer? Returning NULL seems safer.
        return NULL;
    }
    // Check if size fits in size_t
    if ((uint64_t)current_size64 > SIZE_MAX) {
        internal_set_error(EFBIG);  // Or ENOMEM? EFBIG seems appropriate.
        return NULL;
    }
    size_t current_size = (size_t)current_size64;

    // Allocate buffer
    void* buffer = malloc(current_size);
    if (!buffer) {
        internal_set_error(ENOMEM);  // errno likely set by malloc
        return NULL;
    }

    // Rewind before reading
    if (file_rewind(file) != 0) {
        free(buffer);
        // errno set by file_rewind (via fseek)
        return NULL;
    }

    // Read the data
    size_t bytes_read = file_read(file, buffer, 1, current_size);

    if (bytes_read != current_size) {
        // Check if it was an error or just unexpected EOF (file changed?)
        if (file_error(file)) {
            // Read error (errno should be set by underlying fread/system call)
        } else {
            // Unexpected EOF or short read, set a generic error?
            internal_set_error(EIO);
        }
        free(buffer);
        return NULL;
    }

    return buffer;  // Success, caller must free()
}

/**
 * @brief Attempts to acquire an advisory, non-blocking, exclusive (write) lock on the entire file.
 *
 * @param file Pointer to the opened file_t structure.
 * @return true if the lock was acquired successfully or was already held, false otherwise.
 *         On failure to acquire (e.g., file already locked by another process), returns false
 *         and sets errno to EACCES or EAGAIN (POSIX) or similar (Windows).
 */
bool file_lock(file_t* file) {
    if (!file || !file->is_open) {
        internal_set_error(EBADF);
        return false;
    }
    if (file->is_locked) {
        return true;  // Already locked by us
    }

#ifdef _WIN32
    // Use LockFileEx for non-blocking attempt. Lock the whole file range.
    OVERLAPPED overlapped = {0};
    // Lock maximum possible range (approximate whole file locking)
    // Note: Locking beyond EOF is generally permitted.
    if (LockFileEx(
            file->file, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped)) {
        file->is_locked = true;
        return true;
    } else {
        // Could fail due to lock conflict (ERROR_LOCK_VIOLATION) or other reasons
        DWORD error = GetLastError();
        if (error == ERROR_LOCK_VIOLATION) {
            errno = EACCES;  // Map to POSIX-like error
        } else {
            errno = EIO;  // Generic I/O error for other failures
        }
        return false;
    }
#else  // POSIX
    struct flock fl;
    fl.l_type   = F_WRLCK;   // Exclusive (write) lock
    fl.l_whence = SEEK_SET;  // Relative to start of file
    fl.l_start  = 0;         // Start at offset 0
    fl.l_len    = 0;         // Lock until EOF (special meaning for 0)
    fl.l_pid    = 0;         // Not needed for F_SETLK

    // Use F_SETLK for a non-blocking attempt
    if (fcntl(file->fd, F_SETLK, &fl) == 0) {
        // Lock acquired successfully
        file->is_locked = true;
        return true;
    } else {
        // fcntl failed. errno is set (e.g., EACCES, EAGAIN if locked, EBADF, etc.)
        // No need to call internal_set_error here, fcntl sets it.
        return false;
    }
#endif
}

/**
 * @brief Releases an advisory lock held on the file.
 * @param file Pointer to the opened file_t structure.
 * @return true if the lock was released successfully or if no lock was held, false on error.
 */
bool file_unlock(file_t* file) {
    if (!file || !file->is_open) {
        internal_set_error(EBADF);
        return false;
    }
    if (!file->is_locked) {
        return true;  // Not locked, success
    }

#ifdef _WIN32
    OVERLAPPED overlapped = {0};
    // Unlock the same range we locked (entire file approximation)
    if (UnlockFileEx(file->file, 0, 0xFFFFFFFF, 0xFFFFFFFF, &overlapped)) {
        file->is_locked = false;
        return true;
    } else {
        // Error unlocking
        errno = EIO;  // Or map GetLastError()
        return false;
    }
#else  // POSIX
    struct flock fl;
    fl.l_type   = F_UNLCK;  // Unlock
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;  // Unlock entire file (relative to l_start)
    fl.l_pid    = 0;

    // Use F_SETLK to release the lock
    if (fcntl(file->fd, F_SETLK, &fl) == 0) {
        file->is_locked = false;
        return true;
    } else {
        // Error unlocking (errno set by fcntl)
        return false;
    }
#endif
}

/**
 * @brief Sets the file position indicator. Wrapper around fseek.
 * @param file Pointer to the opened file_t structure.
 * @param offset Offset value.
 * @param origin Position from where offset is added (SEEK_SET, SEEK_CUR, SEEK_END).
 * @return 0 on success, -1 on failure (errno is set).
 */
int file_seek(file_t* file, long offset, int origin) {
    if (!file || !file->is_open) {
        return internal_set_error(EBADF);
    }
    // Use fseek for standard C streams
    if (fseek(file->file, offset, origin) != 0) {
        // errno is set by fseek
        return -1;
    }
    // clearerr(file->file); // fseek clears EOF, but maybe clear error flag too?
    return 0;
}

/**
 * @brief Sets the file position indicator to the beginning of the file.
 * @param file Pointer to the opened file_t structure.
 * @return 0 on success, -1 on failure (errno is set).
 */
int file_rewind(file_t* file) {
    // rewind() has no return value, use fseek instead for error checking
    return file_seek(file, 0L, SEEK_SET);
}

/**
 * @brief Gets the current value of the file position indicator. Wrapper around ftell.
 * @param file Pointer to the opened file_t structure.
 * @return The current file position, or -1L on error (errno is set).
 */
long file_tell(file_t* file) {
    if (!file || !file->is_open) {
        internal_set_error(EBADF);
        return -1L;
    }
    long pos = ftell(file->file);
    // if (pos == -1L) { errno is set by ftell }
    return pos;
}

/**
 * @brief Checks the end-of-file indicator for the stream. Wrapper around feof.
 * @param file Pointer to the opened file_t structure.
 * @return Non-zero if EOF indicator is set, 0 otherwise.
 */
int file_eof(file_t* file) {
    if (!file || !file->is_open) {
        // What to return? 0 seems safest, indicating not EOF.
        // Or maybe 1 to indicate an invalid state? Let's go with 0.
        return 0;
    }
    return feof(file->file);
}

/**
 * @brief Checks the error indicator for the stream. Wrapper around ferror.
 * @param file Pointer to the opened file_t structure.
 * @return Non-zero if error indicator is set, 0 otherwise.
 */
int file_error(file_t* file) {
    if (!file || !file->is_open) {
        return 1;  // Indicate error state for invalid file struct
    }
    return ferror(file->file);
}

/**
 * @brief Clears the end-of-file and error indicators for the stream. Wrapper around clearerr.
 * @param file Pointer to the opened file_t structure.
 */
void file_clearerr(file_t* file) {
    if (!file || !file->is_open) {
        return;
    }
    clearerr(file->file);
}

/**
 * @brief Removes the file associated with the file_t structure.
 * The file is closed first if it is open.
 *
 * @param file Pointer to the file_t structure.
 * @return 0 on success, -1 on failure (errno is set by remove()).
 */
int file_remove(file_t* file) {
    if (!file || !file->filename) {
        return internal_set_error(EINVAL);
    }

    // Ensure file is closed before removing
    if (file->is_open) {
        file_close(file);
    }

    int ret = remove(file->filename);

    // We can free the filename memory regardless of success/failure of remove().
    if (file->filename) {
        free(file->filename);
        file->filename = NULL;
    }
    file = NULL;
    return ret;
}

/**
 * @brief Renames the file associated with the file_t structure.
 * The file is closed first if it is open. The internal filename is updated on success.
 *
 * @param file Pointer to the file_t structure.
 * @param newname The new path/name for the file.
 * @return 0 on success, -1 on failure (errno may be set).
 */
int file_rename(file_t* file, const char* newname) {
    if (!file || !file->filename || !newname) {
        return internal_set_error(EINVAL);
    }

    // Ensure file is closed before renaming
    if (file->is_open) {
        file_close(file);
    }

    // Perform the rename operation
    int ret = rename(file->filename, newname);
    if (ret != 0) {
        // errno set by rename
        return -1;
    }

    // Rename succeeded, update internal filename
    char* oldname  = file->filename;
    file->filename = strdup(newname);
    if (!file->filename) {
        // Failed to allocate new name - critical error state.
        // Try to rename back? Or just report memory error? Report error.
        file->filename = oldname;  // Restore pointer (memory leak if oldname was freed)
                                   // Better: don't free oldname until strdup succeeds.
        free(oldname);             // Now free the original name string
        file->filename = NULL;     // Indicate invalid state
        return internal_set_error(ENOMEM);
    }
    free(oldname);  // Free the old filename string now

    // File remains closed after rename. User must reopen if needed.
    return 0;
}

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
int file_copy(file_t* src, file_t* dst) {
    if (!src || !src->is_open || !dst || !dst->is_open) {
        return internal_set_error(EBADF);
    }

    char buffer[FILE_COPY_BUFSIZ];
    size_t bytes_read;
    int error_occurred = 0;

    // Clear errors before starting
    file_clearerr(src);
    file_clearerr(dst);

    // Read from src, write to dst
    while ((bytes_read = file_read(src, buffer, 1, FILE_COPY_BUFSIZ)) > 0) {
        if (file_write(dst, buffer, 1, bytes_read) != bytes_read) {
            error_occurred = 1;  // Write error
            break;
        }
    }

    // Check for read error after the loop
    if (file_error(src)) {
        error_occurred = 1;
    }

    // Flush destination file
    if (file_flush(dst) != 0) {
        error_occurred = 1;  // Flush error
    }

    // Optionally: Update destination size cache? Risky. Better to query.
    // int64_t final_dst_size = file_get_size(dst);
    // if (final_dst_size >= 0) dst->size = (uint64_t)final_dst_size;

    return error_occurred ? -1 : 0;
}

/**
 * @brief Truncates or extends the file to a specified length.
 * Requires the file to be opened in a mode that permits writing.
 *
 * @param file Pointer to the opened file_t structure.
 * @param length The desired length of the file in bytes.
 * @return 0 on success, -1 on failure (errno is set).
 */
int file_truncate(file_t* file, int64_t length) {
    if (!file || !file->is_open) {
        return internal_set_error(EBADF);
    }
    if (length < 0) {
        return internal_set_error(EINVAL);
    }

#ifdef _WIN32
    // Need to move file pointer, then set end of file
    LARGE_INTEGER li_offset;
    li_offset.QuadPart = length;

    if (!SetFilePointerEx(file->file, li_offset, NULL, FILE_BEGIN)) {
        // errno = EIO; // Map GetLastError()
        return -1;
    }
    if (!SetEndOfFile(file->file)) {
        // errno = EIO; // Map GetLastError()
        return -1;
    }
    // Need to resync the C library stream position if using mixed IO?
    // Or maybe just flush?
    // fflush(file->file); // Maybe needed?
#else  // POSIX
    // Use ftruncate with the file descriptor
    // off_t is 64-bit due to _FILE_OFFSET_BITS=64
    if (ftruncate(file->fd, (off_t)length) != 0) {
        // errno is set by ftruncate
        return -1;
    }
#endif
    // Update cached size after truncate
    file->size = (uint64_t)length;
    return 0;
}

// --- Memory Mapping ---

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
void* file_mmap(file_t* file, size_t length, bool read_access, bool write_access) {
    if (!file || !file->is_open || length == 0 || (!read_access && !write_access)) {
        internal_set_error(EINVAL);  // Or EBADF
        return NULL;
    }

    void* addr = NULL;

#ifdef _WIN32
    DWORD flProtect       = 0;
    DWORD dwDesiredAccess = 0;

    if (write_access) {
        flProtect       = PAGE_READWRITE;
        dwDesiredAccess = FILE_MAP_WRITE;  // Includes read access
    } else if (read_access) {
        flProtect       = PAGE_READONLY;
        dwDesiredAccess = FILE_MAP_READ;
    } else {
        internal_set_error(EINVAL);  // Should have caught earlier
        return NULL;
    }

    // Create mapping object (size determined by file size or arguments?)
    // CreateFileMapping uses file size if dwMaximumSizeHigh/Low are 0.
    // Let's map only the requested length explicitly.
    // Note: CreateFileMapping needs size as two 32-bit values.
    DWORD size_high = (DWORD)((uint64_t)length >> 32);
    DWORD size_low  = (DWORD)((uint64_t)length & 0xFFFFFFFF);

    HANDLE hMapFile = CreateFileMapping(file->file, NULL, flProtect, size_high, size_low, NULL);
    if (hMapFile == NULL) {
        // errno = EIO; // Map GetLastError()
        return NULL;
    }

    // Map the view
    addr = MapViewOfFile(hMapFile, dwDesiredAccess, 0, 0, length);
    // Can close the mapping handle now, view stays valid until UnmapViewOfFile
    CloseHandle(hMapFile);

    if (addr == NULL) {
        // errno = EIO; // Map GetLastError()
        return NULL;
    }
#else  // POSIX
    int prot = 0;
    if (read_access)
        prot |= PROT_READ;
    if (write_access)
        prot |= PROT_WRITE;

    addr = mmap(NULL,        // Preferred start address (NULL lets kernel choose)
                length,      // Length of mapping
                prot,        // Protection flags (read/write)
                MAP_SHARED,  // Share changes with the underlying file
                file->fd,    // File descriptor
                0);          // Offset within the file (start at beginning)

    if (addr == MAP_FAILED) {
        // errno set by mmap
        perror("mmap");
        return NULL;
    }
#endif
    return addr;
}

/**
 * @brief Unmaps a previously memory-mapped region.
 *
 * @param addr Pointer to the start of the mapped region (returned by file_mmap).
 * @param length The length of the mapping (must match the length used in file_mmap on POSIX).
 * @return 0 on success, -1 on failure (errno may be set).
 */
int file_munmap(void* addr, size_t length) {
    if (addr == NULL) {
        return internal_set_error(EINVAL);
    }

    int ret = -1;
#ifdef _WIN32
    (void)length;  // Length is not needed for UnmapViewOfFile
    if (UnmapViewOfFile(addr)) {
        ret = 0;
    } else {
        // errno = EIO; // Map GetLastError()
        ret = -1;
    }
#else  // POSIX
    ret = munmap(addr, length);
    // if (ret != 0) { errno set by munmap }
#endif
    return ret;
}

// --- Positional/Async (Misnomer on POSIX) I/O ---

// NOTE: The POSIX versions are NOT asynchronous. They are positional (atomic seek+read/write).
//       The Windows versions simulate blocking asynchronous I/O.

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
ssize_t file_pread(file_t* file, void* buffer, size_t size, int64_t offset) {
    if (!file || !file->is_open || !buffer || offset < 0) {
        return internal_set_error(EINVAL);  // Or EBADF
    }

#ifdef _WIN32
    // Simulate using OVERLAPPED (blocks until complete)
    DWORD dwBytesRead = 0;
    OVERLAPPED overlapped;
    BOOL bResult;

    ZeroMemory(&overlapped, sizeof(overlapped));
    // Correctly set 64-bit offset
    overlapped.Offset     = (DWORD)(offset & 0xFFFFFFFF);
    overlapped.OffsetHigh = (DWORD)(offset >> 32);

    // Using file handle directly. Event object not strictly needed for blocking simulation,
    // but required if checking ERROR_IO_PENDING and waiting. Let's include it.
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        return internal_set_error(EIO);  // Event creation failed
    }

    bResult = ReadFile(file->file,
                       buffer,
                       (DWORD)size,  // Cast size_t to DWORD carefully
                       &dwBytesRead,
                       &overlapped);

    if (!bResult) {
        if (GetLastError() == ERROR_IO_PENDING) {
            // Wait for the operation to complete
            if (!GetOverlappedResult(file->file, &overlapped, &dwBytesRead, TRUE)) {
                // Operation failed after pending
                CloseHandle(overlapped.hEvent);
                // errno = EIO; // Map GetLastError()
                return -1;
            }
            // Success after pending
        } else {
            // ReadFile failed immediately (e.g., invalid handle, permissions)
            // Check for EOF explicitly? ERROR_HANDLE_EOF might occur.
            if (GetLastError() == ERROR_HANDLE_EOF) {
                dwBytesRead = 0;  // Indicate EOF
            } else {
                CloseHandle(overlapped.hEvent);
                // errno = EIO; // Map GetLastError()
                return -1;
            }
        }
    }
    // Success (either immediate or after pending)
    CloseHandle(overlapped.hEvent);
    return (ssize_t)dwBytesRead;

#else  // POSIX
    // Use standard pread (atomic, synchronous positional read)
    ssize_t bytes_read = pread(file->fd, buffer, size, (off_t)offset);
    // pread returns bytes read, 0 for EOF, -1 for error (errno is set)
    return bytes_read;
#endif
}

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
ssize_t file_pwrite(file_t* file, const void* buffer, size_t size, int64_t offset) {
    if (!file || !file->is_open || !buffer || offset < 0) {
        return internal_set_error(EINVAL);  // Or EBADF
    }

#ifdef _WIN32
    DWORD dwBytesWritten = 0;
    OVERLAPPED overlapped;
    BOOL bResult;

    ZeroMemory(&overlapped, sizeof(overlapped));
    overlapped.Offset     = (DWORD)(offset & 0xFFFFFFFF);
    overlapped.OffsetHigh = (DWORD)(offset >> 32);

    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        return internal_set_error(EIO);
    }

    bResult = WriteFile(file->file, buffer, (DWORD)size, &dwBytesWritten, &overlapped);

    if (!bResult) {
        if (GetLastError() == ERROR_IO_PENDING) {
            if (!GetOverlappedResult(file->file, &overlapped, &dwBytesWritten, TRUE)) {
                CloseHandle(overlapped.hEvent);
                // errno = EIO; // Map GetLastError()
                return -1;
            }
        } else {
            CloseHandle(overlapped.hEvent);
            // errno = EIO; // Map GetLastError()
            return -1;
        }
    }
    // Success
    CloseHandle(overlapped.hEvent);
    return (ssize_t)dwBytesWritten;

#else  // POSIX
    ssize_t bytes_written = pwrite(file->fd, buffer, size, (off_t)offset);
    // pwrite returns bytes written, or -1 on error (errno set)
    return bytes_written;
#endif
}

/**
 * @brief Converts a file size in bytes into a human-readable string.
 *
 * Formats the size using appropriate suffixes (B, KB, MB, GB, TB, PB, EB)
 * up to Exabytes. Uses two decimal places unless the value is effectively
 * an integer (within a small epsilon).
 *
 * @param size The file size in bytes (supports up to UINT64_MAX).
 * @param buf The character buffer to store the resulting string.
 * @param len The maximum number of characters (including null terminator)
 *            that can be written to buf.
 *
 * @return true if the conversion and writing to the buffer were successful
 *         (including fitting within the buffer length), false otherwise.
 */
bool filesize_tostring(uint64_t size, char* buf, size_t len) {
    if (buf == NULL || len < 4) {  // Need space for "0 B\0" at minimum
        if (buf != NULL && len > 0)
            buf[0] = '\0';
        internal_set_error(EINVAL);
        return false;
    }

    if (size == 0) {
        int written = snprintf(buf, len, "0 B");
        return written > 0 && (size_t)written < len;
    }

    const char* suffixes[]   = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    const size_t suffixCount = sizeof(suffixes) / sizeof(suffixes[0]);
    const double divisor     = 1024.0;
    size_t suffixIndex       = 0;
    double readableSize      = (double)size;

    while (readableSize >= divisor && suffixIndex < suffixCount - 1) {
        readableSize /= divisor;
        suffixIndex++;
    }

    int written;
    double roundedInt = round(readableSize);
    if (fabs(readableSize - roundedInt) < HUMAN_SIZE_EPSILON) {
        written = snprintf(buf, len, "%.0f %s", roundedInt, suffixes[suffixIndex]);
    } else {
        written = snprintf(buf, len, "%.2f %s", readableSize, suffixes[suffixIndex]);
    }
    return written > 0 && (size_t)written < len;
}
