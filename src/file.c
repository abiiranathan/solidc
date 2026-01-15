#include "../include/file.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <pwd.h>
#include <sys/types.h>
#endif

/** Human-readable size formatting precision threshold. */
#define HUMAN_SIZE_EPSILON 1e-4

/** Buffer size for file copy operations. */
#define COPY_BUFSIZE 65536

/** Maximum safe file size for readall operations (1GB). */
#define MAX_READALL_SIZE (1ULL << 30)

/**
 * Gets the native file handle from a FILE* stream.
 * @param stream The FILE* stream.
 * @return Native handle or INVALID_NATIVE_HANDLE on error.
 */
static native_handle_t get_native_handle(FILE* stream) {
    if (!stream) {
        return INVALID_NATIVE_HANDLE;
    }

#ifdef _WIN32
    int fd = _fileno(stream);
    if (fd == -1) {
        return INVALID_NATIVE_HANDLE;
    }
    HANDLE handle = (HANDLE)(uintptr_t)_get_osfhandle(fd);
    return (handle == INVALID_HANDLE_VALUE) ? INVALID_NATIVE_HANDLE : handle;
#else
    int fd = fileno(stream);
    return (fd == -1) ? INVALID_NATIVE_HANDLE : fd;
#endif
}

/**
 * Cross-platform file information retrieval.
 * On Unix: uses lstat to detect symlinks without following them.
 * On Windows: uses GetFileAttributesEx for basic info.
 */
#ifdef _WIN32

/**
 * Converts Windows FILETIME to Unix timestamp.
 * @param ft Windows FILETIME structure.
 * @return Unix timestamp (seconds since epoch).
 */
static time_t filetime_to_unix(const FILETIME* ft) {
    ULARGE_INTEGER ull;
    ull.LowPart  = ft->dwLowDateTime;
    ull.HighPart = ft->dwHighDateTime;
    // Convert from 100-nanosecond intervals since 1601 to seconds since 1970
    return (time_t)((ull.QuadPart / 10000000ULL) - 11644473600ULL);
}

/**
 * Populates FileAttributes structure from a file path (Windows implementation).
 * @param path Full path to the file.
 * @param name Basename of the file.
 * @param attr Output FileAttributes structure to populate.
 * @return 0 on success, -1 on error (errno is set).
 */
int populate_file_attrs(const char* path, FileAttributes* attr) {
    WIN32_FILE_ATTRIBUTE_DATA file_info;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &file_info)) {
        errno = ENOENT;
        return -1;
    }

    // Initialize structure
    *attr = (FileAttributes){
        .attrs = FATTR_NONE,
        .size  = 0,
        .mtime = filetime_to_unix(&file_info.ftLastWriteTime),
    };

    // Calculate file size
    ULARGE_INTEGER file_size;
    file_size.LowPart  = file_info.nFileSizeLow;
    file_size.HighPart = file_info.nFileSizeHigh;
    attr->size         = file_size.QuadPart;

    // Determine file type
    if (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        attr->attrs |= FATTR_DIR;
        attr->size = 0;  // Directories have no meaningful size on Windows
    } else {
        attr->attrs |= FATTR_FILE;
    }

    // Check for reparse points (symlinks, junctions, etc.)
    if (file_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        attr->attrs |= FATTR_SYMLINK;
    }

    // Check for hidden files
    if (file_info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
        attr->attrs |= FATTR_HIDDEN;
    }

    // On Windows, executability is determined by file extension
    const char* ext = strrchr(path, '.');
    if (ext && (strcmp(ext, ".exe") == 0 || strcmp(ext, ".bat") == 0 || strcmp(ext, ".cmd") == 0 ||
                strcmp(ext, ".com") == 0)) {
        attr->attrs |= FATTR_EXECUTABLE;
    }

    return 0;
}

#else  // Unix/Linux/macOS

/**
 * Populates FileAttributes structure from a file path (POSIX implementation).
 * @param path Full path to the file.
 * @param name Basename of the file.
 * @param attr Output FileAttributes structure to populate.
 * @return 0 on success, -1 on error (errno is set).
 */
int populate_file_attrs(const char* path, FileAttributes* attr) {
    if (!attr) {
        errno = EINVAL;
        return -1;
    }

    struct stat st;
    if (lstat(path, &st) != 0) {
        return -1;  // errno is set by lstat
    }

    // Initialize structure
    *attr = (FileAttributes){
        .attrs = FATTR_NONE,
        .size  = (uint64_t)st.st_size,
        .mtime = st.st_mtime,
    };

    // Determine file type
    if (S_ISREG(st.st_mode)) {
        attr->attrs |= FATTR_FILE;
    } else if (S_ISDIR(st.st_mode)) {
        attr->attrs |= FATTR_DIR;
        attr->size = 0;  // Directory size is not meaningful
    } else if (S_ISLNK(st.st_mode)) {
        attr->attrs |= FATTR_SYMLINK;
    }

#ifdef S_ISCHR
    if (S_ISCHR(st.st_mode)) {
        attr->attrs |= FATTR_CHARDEV;
    }
#endif

#ifdef S_ISBLK
    if (S_ISBLK(st.st_mode)) {
        attr->attrs |= FATTR_BLOCKDEV;
    }
#endif

#ifdef S_ISFIFO
    if (S_ISFIFO(st.st_mode)) {
        attr->attrs |= FATTR_FIFO;
    }
#endif

#ifdef S_ISSOCK
    if (S_ISSOCK(st.st_mode)) {
        attr->attrs |= FATTR_SOCKET;
    }
#endif

    // Check if hidden (starts with '.' on Unix)
    const char* name = path;

    if (path) {
        const char* slash = strrchr(path, '/');
        if (slash && slash[1] != '\0') {
            name = slash + 1;
        }
    }

    if (name[0] == '.') {
        attr->attrs |= FATTR_HIDDEN;
    }

    return 0;
}

#endif  // _WIN32

file_result_t file_open(file_t* file, const char* filename, const char* mode) {
    if (!filename || !mode) {
        errno = EINVAL;
        return FILE_ERROR_INVALID_ARGS;
    }

    // Initialize structure to safe state
    file->stream        = NULL;
    file->native_handle = INVALID_NATIVE_HANDLE;

    // Open the file stream
    file->stream = fopen(filename, mode);
    if (!file->stream) {
        return FILE_ERROR_OPEN_FAILED;  // errno set by fopen
    }

    // Get native handle
    file->native_handle = get_native_handle(file->stream);
    if (file->native_handle == INVALID_NATIVE_HANDLE) {
        fclose(file->stream);
        file->stream = NULL;
        errno        = EBADF;
        return FILE_ERROR_OPEN_FAILED;
    }

    // Populate file attributes.
    if (populate_file_attrs(filename, &file->attr) != 0) {
        fclose(file->stream);
        file->stream = NULL;
        errno        = EBADF;
        return FILE_ERROR_OPEN_FAILED;
    }

    return FILE_SUCCESS;
}

void file_close(file_t* file) {
    if (file->stream) {
        fclose(file->stream);
        file->stream = NULL;
    }
    file->native_handle = INVALID_NATIVE_HANDLE;
}

file_result_t file_truncate(file_t* file, int64_t length) {
    if (length < 0) {
        errno = EINVAL;
        return FILE_ERROR_INVALID_ARGS;
    }

    // Flush any pending writes before truncation
    if (fflush(file->stream) != 0) {
        return FILE_ERROR_IO_FAILED;
    }

#ifdef _WIN32
    LARGE_INTEGER li = {.QuadPart = length};
    if (!SetFilePointerEx(file->native_handle, li, NULL, FILE_BEGIN) || !SetEndOfFile(file->native_handle)) {
        errno = EIO;
        return FILE_ERROR_IO_FAILED;
    }
#else
    if (ftruncate(file->native_handle, (off_t)length) != 0) {
        return FILE_ERROR_IO_FAILED;  // errno already set
    }
#endif

    return FILE_SUCCESS;
}

file_result_t filesize_tostring(uint64_t size, char* buf, size_t len) {
    if (!buf || len < 8) {  // Minimum for "1024.00 B"
        return FILE_ERROR_INVALID_ARGS;
    }

    if (size == 0) {
        int written = snprintf(buf, len, "0 B");
        return (written > 0 && (size_t)written < len) ? FILE_SUCCESS : FILE_ERROR_INVALID_ARGS;
    }

    static const char* const units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    static const size_t num_units    = sizeof(units) / sizeof(units[0]);

    size_t unit_index = 0;
    double value      = (double)size;

    while (value >= 1024.0 && unit_index < num_units - 1) {
        value /= 1024.0;
        unit_index++;
    }

    double rounded = round(value);
    int written    = -1;

    if (fabs(value - rounded) < HUMAN_SIZE_EPSILON) {
        written = snprintf(buf, len, "%.0f %s", rounded, units[unit_index]);
    } else {
        written = snprintf(buf, len, "%.2f %s", value, units[unit_index]);
    }

    return (written > 0 && (size_t)written < len) ? FILE_SUCCESS : FILE_ERROR_INVALID_ARGS;
}

size_t file_read(const file_t* file, void* buffer, size_t size, size_t count) {
    if (!buffer || size == 0 || count == 0) {
        return 0;
    }
    return fread(buffer, size, count, file->stream);
}

size_t file_write(file_t* file, const void* buffer, size_t size, size_t count) {
    if (!buffer || size == 0 || count == 0) {
        return 0;
    }
    return fwrite(buffer, size, count, file->stream);
}

size_t file_write_string(file_t* file, const char* str) {
    if (!str) {
        return 0;
    }
    size_t len = strlen(str);
    return (len > 0) ? fwrite(str, 1, len, file->stream) : 0;
}

ssize_t file_pread(const file_t* file, void* buffer, size_t size, int64_t offset) {
    if (!buffer || size == 0 || offset < 0) {
        errno = EINVAL;
        return -1;
    }

#ifdef _WIN32
    OVERLAPPED ov = {.Offset = (DWORD)(offset & 0xFFFFFFFF), .OffsetHigh = (DWORD)(offset >> 32), .hEvent = NULL};

    DWORD bytes_read;
    if (!ReadFile(file->native_handle, buffer, (DWORD)size, &bytes_read, &ov)) {
        DWORD error = GetLastError();
        if (error == ERROR_HANDLE_EOF) {
            return 0;  // EOF
        }
        errno = EIO;
        return -1;
    }
    return (ssize_t)bytes_read;
#else
    return pread(file->native_handle, buffer, size, (off_t)offset);
#endif
}

ssize_t file_pwrite(file_t* file, const void* buffer, size_t size, int64_t offset) {
    if (!buffer || size == 0 || offset < 0) {
        errno = EINVAL;
        return -1;
    }

#ifdef _WIN32
    OVERLAPPED ov = {.Offset = (DWORD)(offset & 0xFFFFFFFF), .OffsetHigh = (DWORD)(offset >> 32), .hEvent = NULL};

    DWORD bytes_written;
    if (!WriteFile(file->native_handle, buffer, (DWORD)size, &bytes_written, &ov)) {
        errno = EIO;
        return -1;
    }
    return (ssize_t)bytes_written;
#else
    return pwrite(file->native_handle, buffer, size, (off_t)offset);
#endif
}

void* file_readall(const file_t* file, size_t* size_out) {
    size_t size = file->attr.size;
    if (size == 0) {
        if (size_out) *size_out = 0;
        // Return a valid pointer for zero-sized files
        return malloc(1);
    }

    void* buffer = malloc((size_t)size);
    if (!buffer) {
        errno = ENOMEM;
        return NULL;
    }

    // Save current position and seek to beginning
    int64_t orig_pos = file_tell(file);
    if (fseek(file->stream, 0, SEEK_SET) != 0) {
        free(buffer);
        return NULL;
    }

    size_t bytes_read = file_read(file, buffer, 1, (size_t)size);

    // Restore original position (best effort)
    if (orig_pos >= 0) {
        fseek(file->stream, (long)orig_pos, SEEK_SET);
    }

    if (bytes_read != (size_t)size) {
        free(buffer);
        errno = (ferror(file->stream)) ? EIO : EINVAL;
        return NULL;
    }

    if (size_out) {
        *size_out = bytes_read;
    }
    return buffer;
}

file_result_t file_lock(const file_t* file) {
#ifdef _WIN32
    OVERLAPPED overlapped = {0};
    if (LockFileEx(file->native_handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD,
                   &overlapped)) {
        return FILE_SUCCESS;
    }

    DWORD error = GetLastError();
    if (error == ERROR_LOCK_VIOLATION) {
        errno = EACCES;
        return FILE_ERROR_LOCK_FAILED;
    }
    errno = EIO;
    return FILE_ERROR_SYSTEM_ERROR;
#else
    struct flock fl = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};

    if (fcntl(file->native_handle, F_SETLK, &fl) == 0) {
        return FILE_SUCCESS;
    }

    if (errno == EACCES || errno == EAGAIN) {
        return FILE_ERROR_LOCK_FAILED;
    }
    return FILE_ERROR_SYSTEM_ERROR;
#endif
}

file_result_t file_unlock(const file_t* file) {
#ifdef _WIN32
    OVERLAPPED overlapped = {0};
    if (UnlockFileEx(file->native_handle, 0, MAXDWORD, MAXDWORD, &overlapped)) {
        return FILE_SUCCESS;
    }
    errno = EIO;
    return FILE_ERROR_SYSTEM_ERROR;
#else
    struct flock fl = {.l_type = F_UNLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};

    if (fcntl(file->native_handle, F_SETLK, &fl) == 0) {
        return FILE_SUCCESS;
    }
    return FILE_ERROR_SYSTEM_ERROR;
#endif
}

file_result_t file_copy(const file_t* src, file_t* dst) {
    char buffer[COPY_BUFSIZE] = {0};
    size_t bytes_read         = 0;

    // Clear any previous errors
    clearerr(src->stream);
    clearerr(dst->stream);

    while ((bytes_read = file_read(src, buffer, 1, COPY_BUFSIZE)) > 0) {
        if (file_write(dst, buffer, 1, bytes_read) != bytes_read) {
            return FILE_ERROR_IO_FAILED;
        }
    }

    // Check for read error
    if (ferror(src->stream)) {
        return FILE_ERROR_IO_FAILED;
    }

    // Flush destination
    if (fflush(dst->stream) != 0) {
        return FILE_ERROR_IO_FAILED;
    }

    return FILE_SUCCESS;
}

void* file_mmap(const file_t* file, size_t length, bool read_access, bool write_access) {
    if (length == 0 || (!read_access && !write_access)) {
        errno = EINVAL;
        return NULL;
    }

#ifdef _WIN32
    DWORD protect = write_access ? PAGE_READWRITE : PAGE_READONLY;
    DWORD access  = write_access ? FILE_MAP_WRITE : FILE_MAP_READ;

    HANDLE mapping = CreateFileMapping(file->native_handle, NULL, protect, (DWORD)(length >> 32), (DWORD)length, NULL);
    if (!mapping) {
        errno = EIO;
        return NULL;
    }

    void* addr = MapViewOfFile(mapping, access, 0, 0, length);
    CloseHandle(mapping);

    if (!addr) {
        errno = EIO;
    }
    return addr;
#else
    int prot = 0;
    if (read_access) prot |= PROT_READ;
    if (write_access) prot |= PROT_WRITE;

    void* addr = mmap(NULL, length, prot, MAP_SHARED, file->native_handle, 0);
    return (addr == MAP_FAILED) ? NULL : addr;
#endif
}

file_result_t file_munmap(void* addr, size_t length) {
    if (!addr) {
        return FILE_ERROR_INVALID_ARGS;
    }

#ifdef _WIN32
    (void)length;  // Unused on Windows
    return UnmapViewOfFile(addr) ? FILE_SUCCESS : FILE_ERROR_SYSTEM_ERROR;
#else
    return (munmap(addr, length) == 0) ? FILE_SUCCESS : FILE_ERROR_SYSTEM_ERROR;
#endif
}

file_result_t file_flush(file_t* file) {
    return (fflush(file->stream) == 0) ? FILE_SUCCESS : FILE_ERROR_IO_FAILED;
}

int64_t file_tell(const file_t* file) {
#ifdef _WIN32
    LARGE_INTEGER zero = {0};
    LARGE_INTEGER pos;
    if (SetFilePointerEx(file->native_handle, zero, &pos, FILE_CURRENT)) {
        return (int64_t)pos.QuadPart;
    }
    errno = EIO;
    return -1;
#else
    off_t pos = lseek(file->native_handle, 0, SEEK_CUR);
    return (pos == (off_t)-1) ? -1 : (int64_t)pos;
#endif
}

file_result_t file_seek(file_t* file, int64_t offset, int whence) {
    // Flush any pending writes before seeking
    if (fflush(file->stream) != 0) {
        return FILE_ERROR_IO_FAILED;
    }

#ifdef _WIN32
    DWORD move_method;
    switch (whence) {
        case SEEK_SET:
            move_method = FILE_BEGIN;
            break;
        case SEEK_CUR:
            move_method = FILE_CURRENT;
            break;
        case SEEK_END:
            move_method = FILE_END;
            break;
        default:
            errno = EINVAL;
            return FILE_ERROR_INVALID_ARGS;
    }

    LARGE_INTEGER li = {.QuadPart = offset};
    if (SetFilePointerEx(file->native_handle, li, NULL, move_method)) {
        // Sync the FILE* stream position
        fseek(file->stream, 0, SEEK_CUR);
        return FILE_SUCCESS;
    }
    errno = EIO;
    return FILE_ERROR_IO_FAILED;
#else
    if (lseek(file->native_handle, (off_t)offset, whence) == (off_t)-1) {
        return FILE_ERROR_IO_FAILED;  // errno already set
    }

    // Sync the FILE* stream position
    if (fseek(file->stream, 0, SEEK_CUR) != 0) {
        return FILE_ERROR_IO_FAILED;
    }

    return FILE_SUCCESS;
#endif
}
