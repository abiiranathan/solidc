#define _POSIX_C_SOURCE 200809L

#include "../include/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// file_t structure that encapsulates file operations
// in a platform-independent way.
typedef struct file_t {
    FILE* file;      // file pointer
    int fd;          // file descriptor
    bool is_open;    // is the file open?
    bool is_locked;  // is the file locked?
    ssize_t size;    // file size
    char* filename;  // file name
#ifdef _WIN32
    HANDLE handle;  // file handle
#endif
} file_t;

// Get the file descriptor for the file.
int file_fileno(file_t* file) {
    return file->fd;
}

// Get the FILE pointer for the file.
FILE* file_fp(file_t* file) {
    return file->file;
}

// Get the file size. Returns -1 on error
// On windows, use _filelength to get the file size.
// On Unix, use fstat to get the file size
static ssize_t internal_file_size(int fd) {
    ssize_t size = -1;
#ifdef _WIN32
    size = _filelength(fd);
#else
    struct stat st;
    if (fstat(fd, &st) == 0) {
        size = st.st_size;
    }
#endif
    return size;
}

// Open a file in a given mode (r, w, a, etc)
// Returns a file handle or NULL on error
file_t* file_open(const char* filename, const char* mode) {
    FILE* fp = fopen(filename, mode);
    if (!fp) {
        return NULL;
    }

    file_t* file = (file_t*)malloc(sizeof(file_t));
    if (!file) {
        fclose(fp);
        return NULL;
    }

    file->file = fp;
    file->is_open = true;
    file->is_locked = false;
    file->filename = strdup(filename);
    if (file->filename == NULL) {
        perror("strdup() failed: unable to copy filename");
        return NULL;
    }
    file->fd = fileno(file->file);
    file->size = internal_file_size(file->fd);
#ifdef _WIN32
    file->handle = (HANDLE)_get_osfhandle(file->fd);
#endif
    return file;
}

// Flush a file to disk
void file_flush(file_t* file) {
    if (file->is_open) {
        int ret = fflush(file->file);
        if (ret != 0) {
            perror("fflush");
        }
    }
}

// Close a file and release resources
void file_close(file_t* file) {
    if (file->is_open) {
        fclose(file->file);
        file->is_open = false;
    }

    if (file->is_locked) {
        file_unlock(file);
    }

    if (file->filename) {
        free(file->filename);
    }

    free(file);
}

// Returns the human-readable size of a file up to TB.
// The size is stored in the buf parameter.
bool filesize_tostring(size_t size, char* buf, size_t len) {
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    size_t suffixCount = sizeof(suffixes) / sizeof(suffixes[0]);
    size_t suffixIndex = 0;
    double readableSize = (double)size;

    while (readableSize >= 1024 && suffixIndex < suffixCount - 1) {
        // Can't avoid floating-point division here for precision
        readableSize /= 1024;
        suffixIndex++;
    }

    // if readableSize has no fractions, print it as an integer
    if (readableSize == (int)readableSize) {
        int written = snprintf(buf, len, "%d %s", (int)readableSize, suffixes[suffixIndex]);
        return written > 0 && (size_t)written < len;
    }

    // otherwise, print it as a float with 2 decimal places
    int written = snprintf(buf, len, "%.2f %s", readableSize, suffixes[suffixIndex]);
    return written > 0 && (size_t)written < len;
}

ssize_t file_size(file_t* file) {
    return file->size;
}

// Return file size for a given file name
ssize_t file_size_char(const char* filename) {
    file_t* file = file_open(filename, "r");
    if (!file) {
        return -1;
    }

    ssize_t size = file->size;
    file_close(file);
    return size;
}

// Read from a file into a buffer.
// @param file: The file to read from
// @param buffer: The buffer to read into
// @param size: The size of each element to read
// @param count: The number of elements to read
// Example:
// char buffer[1024];
// file_read(file, buffer, 1, 1024);
size_t file_read(file_t* file, void* buffer, size_t size, size_t count) {
    if (!file->is_open) {
        fprintf(stderr, "File is not open\n");
        return 0;
    }
    size_t n = fread(buffer, size, count, file->file);
    if (n == 0 && ferror(file->file)) {
        perror("fread");
    }
    return n;
}

// Read the entire file into a buffer equal to file size.
// Returns a pointer to the buffer or NULL on error
// remember to free the buffer after use
void* file_readall(file_t* file) {
    if (!file->is_open) {
        return 0;
    }

    void* buffer = malloc(file->size);
    if (!buffer) {
        perror("malloc");
        return NULL;
    }

    // Read the entire file into the buffer
    ssize_t bytes_read = (ssize_t)file_read(file, buffer, 1, file->size);
    if (bytes_read != file->size) {
        fprintf(stderr, "Error reading file: Read %zd bytes, expected %zd\n", bytes_read,
                file->size);
        free(buffer);
        return NULL;
    }
    return buffer;
}

/* Write to a file

@param file: The file to write to.
@param buffer: The buffer to write from.
@param size: The size of each element to write.
@param count: The number of elements to write.

Returns the number of elements written.
Example:
char *buffer = "Hello, World!";
file_write(file, buffer, 1, strlen(buffer));

After a write operation, the file is flushed to disk and the file size is updated.
*/
size_t file_write(file_t* file, const void* buffer, size_t size, size_t count) {
    if (!file->is_open) {
        fprintf(stderr, "File is not open\n");
        return 0;
    }

    ssize_t n = (ssize_t)fwrite(buffer, size, count, file->file);
    if (n > 0) {
        file->size += n;
        if (fflush(file->file) != 0) {
            perror("fflush");
            return 0;
        }
    }
    return n;
}

// Write a string to a file
size_t file_write_string(file_t* file, const char* str) {
    if (!file->is_open) {
        fprintf(stderr, "file %s is not open\n", file->filename);
        return 0;
    }
    return fwrite(str, 1, strlen(str), file->file);
}

// Lock a file. Returns true if successful, false otherwise.
// If the file is already locked, it returns true.
// On Windows, it uses LockFile to lock the file.
// On Unix, it uses fcntl to lock the file.
bool file_lock(file_t* file) {
    if (!file->is_open) {
        return false;
    }
    if (file->is_locked) {
        return true;
    }
#ifdef _WIN32
    if (LockFile(file->handle, 0, 0, file->size, 0)) {
        file->is_locked = true;
        return true;
    }
#else
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(file->fd, F_SETLK, &fl) == 0) {
        file->is_locked = true;
        return true;
    }
#endif
    return false;
}

// Unlock a file. Returns true if successful, false otherwise
// If the file is not locked, it returns true.
// On Windows, it uses UnlockFile to unlock the file.
// On Unix, it uses fcntl to unlock the file.
bool file_unlock(file_t* file) {
    if (!file->is_open) {
        return false;
    }
    if (!file->is_locked) {
        return true;
    }
#ifdef _WIN32
    if (UnlockFile(file->handle, 0, 0, file->size, 0)) {
        file->is_locked = false;
        return true;
    }
#else
    struct flock fl;
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(file->fd, F_SETLK, &fl) == 0) {
        file->is_locked = false;
        return true;
    }

#endif
    return false;
}

// file seek
int file_seek(file_t* file, long offset, int origin) {
    return fseek(file->file, offset, origin);
}

// rewind file
void file_rewind(file_t* file) {
    rewind(file->file);
}

// file remove
int file_remove(file_t* file) {
    if (file->is_open) {
        fclose(file->file);
    }

    int ret = remove(file->filename);
    if (ret == 0) {
        file->is_open = false;
    }
    return ret;
}

// Rename a file. Returns 0 if successful, -1 otherwise
int file_rename(file_t* file, const char* newname) {
    if (file->is_open) {
        fclose(file->file);
    }
    int ret = rename(file->filename, newname);
    if (ret == 0) {
        free(file->filename);
        file->filename = strdup(newname);
    }
    return ret;
}

// file copy. Bith file and dst must be open
// Returns 0 if successful, -1 otherwise
// The files are not closed after copying.
// The dest file is flushed and rewinded after copying.
int file_copy(file_t* file, file_t* dst) {
    char buffer[BUFSIZ];
    size_t bytes;
    while ((bytes = file_read(file, buffer, 1, BUFSIZ)) > 0) {
        file_write(dst, buffer, 1, bytes);
    }

    // Check for errors
    if (ferror(file->file) || ferror(dst->file)) {
        return -1;
    }

    // flush the file
    fflush(dst->file);
    rewind(dst->file);

    // update the file size
    dst->size = file->size;
    return 0;
}

// Memory map a file. Returns a pointer to the mapped memory
// The caller is responsible for unmapping the memory
void* file_mmap(file_t* file, size_t length) {
    if (!file->is_open) {
        fprintf(stderr, "File is not open\n");
        return NULL;
    }
    void* addr = NULL;
#ifdef _WIN32
    HANDLE hMapFile = CreateFileMapping(file->handle, NULL, PAGE_READWRITE, 0, length, NULL);
    if (hMapFile == NULL) {
        fprintf(stderr, "CreateFileMapping failed\n");
        return NULL;
    }
    addr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, length);
    CloseHandle(hMapFile);
#else
    addr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, file->fd, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "mmap failed\n");
        return NULL;
    }
#endif
    return addr;
}

// Unmap a memory mapped file
int file_munmap(void* addr, size_t length) {
    int ret = -1;
#ifdef _WIN32
    ret = UnmapViewOfFile(addr);
    (void)length;
#else
    ret = munmap(addr, length);
#endif
    return ret;
}

#ifdef _WIN32
// Read from a file asynchronously
// Returns the number of bytes read or -1 on error
DWORD file_aread(file_t* file, void* buffer, size_t size, off_t offset) {
    DWORD dwBytesRead;
    OVERLAPPED overlapped;
    BOOL bResult;

    if (!file->is_open) {
        return -1;
    }

    ZeroMemory(&overlapped, sizeof(overlapped));
    overlapped.Offset = (DWORD)offset;
    overlapped.OffsetHigh = (DWORD)((DWORDLONG)offset >> 32);  // Set the high part of offset

    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        return -1;
    }

    bResult = ReadFile(file->handle, buffer, size, &dwBytesRead, &overlapped);
    if (!bResult) {
        if (GetLastError() == ERROR_IO_PENDING) {
            GetOverlappedResult(file->handle, &overlapped, &dwBytesRead, TRUE);
        } else {
            printf("Error reading file: %ld\n", GetLastError());
        }
    }

    CloseHandle(overlapped.hEvent);
    return dwBytesRead;
}
#else
// Read from a file asynchronously
// Returns the number of bytes read or -1 on error
ssize_t file_aread(file_t* file, void* buffer, size_t size, off_t offset) {
    if (!file->is_open) {
        fprintf(stderr, "File is not open\n");
        return -1;
    }
    ssize_t bytes = -1;
    bytes = pread(file->fd, buffer, size, offset);
    return bytes;
}
#endif

#ifdef _WIN32
DWORD file_awrite(file_t* file, const void* buffer, size_t size, off_t offset) {
    DWORD dwBytesWritten = 0;
    OVERLAPPED overlapped;
    BOOL bResult;

    if (!file->is_open) {
        return (DWORD)-1;
    }

    ZeroMemory(&overlapped, sizeof(overlapped));
    overlapped.Offset = (DWORD)offset;
    overlapped.OffsetHigh = (DWORD)((DWORDLONG)offset >> 32);

    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        return (DWORD)-1;
    }

    bResult = WriteFile(file->handle, buffer, size, &dwBytesWritten, &overlapped);
    if (!bResult) {
        if (GetLastError() == ERROR_IO_PENDING) {
            // Write operation is pending, wait for it to complete
            if (GetOverlappedResult(file->handle, &overlapped, &dwBytesWritten, TRUE)) {
                // Write completed successfully
                bResult = TRUE;
            } else {
                // Error occurred during asynchronous operation
                dwBytesWritten = 0;
            }
        } else {
            // Error occurred during synchronous write
            dwBytesWritten = 0;
        }
    }

    CloseHandle(overlapped.hEvent);
    return dwBytesWritten;
}
#else
ssize_t file_awrite(file_t* file, const void* buffer, size_t size, off_t offset) {
    return pwrite(file->fd, buffer, size, offset);
}
#endif
