#ifndef OS_H
#define OS_H

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400  // Required for syncapi
#endif

#else
#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <pthread.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "lock.h"

// ========== Type definitions =========================

// File handle
typedef struct {
    FILE* file;      // file pointer
    int fd;          // file descriptor
    bool is_open;    // is the file open?
    bool is_locked;  // is the file locked?
    ssize_t size;    // file size
    char* filename;  // file name

#ifdef _WIN32
    HANDLE handle;  // file handle
#endif
} File;

// ============ File related functions ============
File* file_open(const char* filename, const char* mode) __attribute__((warn_unused_result));
void file_close(File* file);
ssize_t file_size(File* file);
// Return file size for a given file name
ssize_t pfile_size(const char* filename);
int file_seek(File* file, long offset, int origin);
size_t file_write(File* file, const void* buffer, size_t size, size_t count);
size_t file_write_string(File* file, const char* str);

// Write to a file asynchronously
// Returns the number of bytes written or -1 on error
// Usage:
#if defined(_WIN32)
DWORD file_awrite(File* file, const void* buffer, size_t size, off_t offset);
#else
ssize_t file_awrite(File* file, const void* buffer, size_t size, off_t offset);
#endif

size_t file_read(File* file, void* buffer, size_t size, size_t count);

#if defined(_WIN32)
DWORD file_aread(File* file, void* buffer, size_t size, off_t offset);
#else
// Read from a file asynchronously
ssize_t file_aread(File* file, void* buffer, size_t size, off_t offset);
#endif

void* file_readall(File* file, ssize_t* size);
bool file_lock(File* file);
bool file_unlock(File* file);
int file_rename(File* file, const char* newname);
int file_copy(File* file, File* dst);
void* file_mmap(File* file, size_t length);
int file_munmap(void* addr, size_t length);
bool is_file(const char* path);

// Pipe declarations
// PIPE related function declarations
typedef enum { PIPE_END_READ = 1, PIPE_END_WRITE = 2, PIPE_END_BOTH = 3 } PipeEnd;

typedef struct {
#ifdef _WIN32
    HANDLE hReadPipe;
    HANDLE hWritePipe;
#else
    int fd[2];
#endif
} PIPE;

int pipe_open(PIPE* pPipe);
void pipe_close(PIPE* pPipe, PipeEnd endToClose);
ssize_t pipe_read(PIPE* pPipe, void* buffer, size_t size);
ssize_t pipe_write(PIPE* pPipe, const void* buffer, size_t size);

// =========== END File related functions ===========

typedef struct Process {
#ifdef _WIN32
    DWORD pid;               // Process id is similar to pi.dwProcessId
    STARTUPINFO si;          // Start up info
    PROCESS_INFORMATION pi;  // Process information
#else
    long pid;  // Process ID(long to match DWORD on Windows)
#endif
} Process;

#ifdef _WIN32
// Alias to win32 HANDLE
typedef HANDLE Thread;
#else
// Alias to posix pthread
typedef pthread_t Thread;
#endif

typedef struct {
    void* arg;     // Argument to the start_routine
    void* retval;  // Pointer to the return value
} ThreadData;

// Thread and processes
// Create a new process
int process_create(Process* proc, const char* command, const char* const argv[],
                   const char* const envp[]);
// Wait for a process to exit
int process_wait(Process* proc, int* status);
// Kill a process
int process_kill(Process* proc);

// Create a new thread
int thread_create(Thread* thread, void* (*start_routine)(void*), void* data);
// Join a thread
int thread_join(Thread tid, void** retval);

// Get the current thread id
int thread_self();

int thread_detach(Thread tid);
// Sleep for a number of milliseconds
void sleep_ms(int ms);
// Get the current process id
int get_pid();
// Get the current thread id
int get_tid();
// Get the parent process id
int get_ppid();
// Get the number of CPU cores
int get_ncpus();

#ifdef _WIN32  // No need for these functions on Windows
#else          // Unix
// Get the current user id
int get_uid();
// Get the current group id
int get_gid();
// Get the current user name
char* get_username();
// Get the current group name
char* get_groupname();
#endif

// IMPLEMENT A THREAD POOL
typedef struct ThreadPool {
    struct Task* task_queue;  // Linked list queue of tasks
    int num_threads;          // Total Number of threads
    int num_working_threads;  // Number of threads working
    Thread* threads;          // Array of threads

    // Cross platform Lock and condition variables(lock.h)
    Lock lock;
    Condition task_available;
    Condition all_tasks_completed;

    // 1 if the thread pool is shutting down, 0 otherwise
    bool shutdown;
} ThreadPool;

typedef struct Task {
    void* (*function)(void*);
    void* arg;
    struct Task* next;
} Task;

ThreadPool* threadpool_create(int num_threads);
void threadpool_destroy(ThreadPool* pool);
int threadpool_add_task(ThreadPool* pool, void* (*task)(void*), void* arg);
void threadpool_wait(ThreadPool* pool);

//  ===== Directory related function declarations =====
// Directory handle
typedef struct {
    char* path;  // directory path
#ifdef _WIN32
    HANDLE handle;
    WIN32_FIND_DATAA find_data;  // found in windows.h
#else
    DIR* dir;  // directory pointer found in dirent.h
#endif
} Directory;

// Open a directory
Directory* dir_open(const char* path) __attribute__((warn_unused_result));

// Close a directory
void dir_close(Directory* dir);

// Read the next entry in the directory.
char* dir_next(Directory* dir) __attribute__((warn_unused_result));

// Create a directory. Returns 0 if successful, -1 otherwise
int dir_create(const char* path);

// Remove a directory. Returns 0 if successful, -1 otherwise
int dir_remove(const char* path);

// Rename a directory. Returns 0 if successful, -1 otherwise
int dir_rename(const char* oldpath, const char* newpath);

// Change the current working directory
int dir_chdir(const char* path);

// List files in a directory, returns a pointer to a list of file names or NULL on error
// The caller is responsible for freeing the memory.
// The number of files is stored in the count parameter.
// Note: This algorithm walks the directory tree recursively
// and may be slow for large directories.
char** dir_list(const char* path, size_t* count) __attribute__((warn_unused_result));

// Returns true if the path is a directory
bool is_dir(const char* path);

// Create a directory recursively
bool makedirs(const char* path);

// Get path to platform's TEMP directory.
char* get_tempdir() __attribute__((warn_unused_result));

// Create a temporary file.
char* make_tempfile() __attribute__((warn_unused_result));

// Create a temporary directory
char* make_tempdir() __attribute__((warn_unused_result));

// Returns true if path is a symbolic link.
bool is_symlink(const char* path);

typedef enum WalkDirOption {
    DirContinue,  // Continue walking the directory recursively
    DirStop,      // Stop traversal of this directory
} WalkDirOption;

// Walk the directory path, for each entry call the callback.
// with path, name and user data pointer.
// Returns 0 to continue or non-zero to stop the walk.
typedef WalkDirOption (*WalkDirCallback)(const char* path, const char* name, void* data);

// Walk the directory path, for each entry call the callback
// with path, name and user data pointer.
// Return from the callback 0 to continue or non-zero to stop the walk.
// Returns 0 if successful, -1 otherwise
int dir_walk(const char* path, WalkDirCallback callback, void* data);

// Find the size of the directory.
// This is slow on large directories since it walks the directory.
ssize_t dir_size(const char* path);

// Returns the path to the current working directory.
char* get_cwd() __attribute__((warn_unused_result));

//  ===== Filepath related function declarations =====

// get the file's basename.
void filepath_basename(const char* path, char* basename, size_t size);

// Get the directory name of a path
void filepath_dirname(const char* path, char* dirname, size_t size);

// Get the file extension
void filepath_extension(const char* path, char* ext, size_t size);

// Get the file name(basename) without the extension
void filepath_nameonly(const char* path, char* name, size_t size);

// Get the absolute path of a file Returns a pointer to the absolute path
// or NULL on error.
// The caller is responsible for freeing the memory.
char* filepath_absolute(const char* path) __attribute__((warn_unused_result));

// Delete or unlink file or directory.
int filepath_remove(const char* path);

// Rename file or directory.
int filepath_rename(const char* oldpath, const char* newpath);

// Expand user home directory.
char* filepath_expanduser(const char* path) __attribute__((warn_unused_result));

// Join path1 and path2 using standard os specific separator.
char* filepath_join(const char* path1, const char* path2) __attribute__((warn_unused_result));

bool filepath_join_buf(const char* path1, const char* path2, char* abspath, size_t len);

// Split a file path into directory and basename.
// The dir and name parameters must be pre-allocated buffers or
// pointers to pre-allocated buffers.
// dir_size and name_size are the size of the dir and name
// buffers.
void filepath_split(const char* path, char* dir, char* name, size_t dir_size, size_t name_size);

#if defined(__cplusplus)
}
#endif

// ***************** IMPLEMENTATION *************************
#ifdef OS_IMPL

// Open a file
File* file_open(const char* filename, const char* mode) {
    FILE* fp = fopen(filename, mode);
    if (!fp) {
        return NULL;
    }

    File* file = (File*)malloc(sizeof(File));
    if (!file) {
        fclose(fp);
        return NULL;
    }

    file->file      = fp;
    file->is_open   = true;
    file->is_locked = false;
    file->filename  = strdup(filename);
    if (file->filename == NULL) {
        perror("strdup() failed: unable to copy filename");
        return NULL;
    }
    file->fd   = fileno(file->file);
    file->size = file_size(file);
#ifdef _WIN32
    file->handle = (HANDLE)_get_osfhandle(file->fd);
#endif
    return file;
}

// Close a file and release resources
void file_close(File* file) {
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
}

// cross platform file size
ssize_t file_size(File* file) {
    ssize_t size = -1;
#ifdef _WIN32
    size = _filelength(file->fd);
#else
    struct stat st;
    if (fstat(file->fd, &st) == 0) {
        size = st.st_size;
    }
#endif
    return size;
}

// Return file size for a given file name
ssize_t pfile_size(const char* filename) {
    File* file = file_open(filename, "r");
    if (!file) {
        return -1;
    }
    ssize_t size = file_size(file);
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
size_t file_read(File* file, void* buffer, size_t size, size_t count) {
    if (!file->is_open) {
        fprintf(stderr, "File is not open\n");
        return 0;
    }
    return fread(buffer, size, count, file->file);
}

// ReadAll reads the entire file into a buffer
// Returns the number of elements read.
// The size of the buffer is stored in the size parameter
// The caller is responsible for freeing the buffer
void* file_readall(File* file, ssize_t* size) {
    if (!file->is_open) {
        return 0;
    }

    fseek(file->file, 0, SEEK_END);
    *size = ftell(file->file);
    fseek(file->file, 0, SEEK_SET);
    void* buffer = malloc(*size);
    if (!buffer) {
        perror("malloc");
        return NULL;
    }

    // Read the entire file into the buffer
    ssize_t bytes_read = file_read(file, buffer, 1, *size);
    if (bytes_read != *size) {
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
*/
size_t file_write(File* file, const void* buffer, size_t size, size_t count) {
    if (!file->is_open) {
        fprintf(stderr, "File is not open\n");
        return 0;
    }
    int n = fwrite(buffer, size, count, file->file);
    if (n > 0) {
        file->size += n;
        if (fflush(file->file) != 0) {
            perror("fflush");
            return 0;
        }
    }
    return n;
}

size_t file_write_string(File* file, const char* str) {
    if (!file->is_open) {
        fprintf(stderr, "File is not open\n");
        return 0;
    }
    return fwrite(str, 1, strlen(str), file->file);
}

// Lock a file. Returns true if successful, false otherwise
bool file_lock(File* file) {
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
    fl.l_type   = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    if (fcntl(file->fd, F_SETLK, &fl) == 0) {
        file->is_locked = true;
        return true;
    }
#endif
    return false;
}

// Unlock a file. Returns true if successful, false otherwise
bool file_unlock(File* file) {
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
    fl.l_type   = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    if (fcntl(file->fd, F_SETLK, &fl) == 0) {
        file->is_locked = false;
        return true;
    }

#endif
    return false;
}

// file seek
int file_seek(File* file, long offset, int origin) {
    return fseek(file->file, offset, origin);
}

// rewind file
void file_rewind(File* file) {
    rewind(file->file);
}

// file remove
int file_remove(File* file) {
    if (file->is_open) {
        fclose(file->file);
    }

    int ret = remove(file->filename);
    if (ret == 0) {
        file->is_open = false;
    }
    return ret;
}

// file rename
int file_rename(File* file, const char* newname) {
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
int file_copy(File* file, File* dst) {
    char buffer[BUFSIZ];
    size_t bytes;
    while ((bytes = file_read(file, buffer, 1, BUFSIZ)) > 0) {
        file_write(dst, buffer, 1, bytes);
    }

    // Check if the read was successful
    if (bytes < 0) {
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
void* file_mmap(File* file, size_t length) {
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
#else
    ret = munmap(addr, length);
#endif
    return ret;
}

#ifdef _WIN32
// Read from a file asynchronously
// Returns the number of bytes read or -1 on error
DWORD file_aread(File* file, void* buffer, size_t size, off_t offset) {
    DWORD dwBytesRead;
    OVERLAPPED overlapped;
    BOOL bResult;

    if (!file->is_open) {
        printf("File is not open!\n");
        return -1;
    }

    ZeroMemory(&overlapped, sizeof(overlapped));
    overlapped.Offset     = (DWORD)offset;
    overlapped.OffsetHigh = (DWORD)((DWORDLONG)offset >> 32);  // Set the high part of offset

    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        printf("Error creating event!\n");
        return -1;
    }

    bResult = ReadFile(file->handle, buffer, size, &dwBytesRead, &overlapped);
    if (!bResult) {
        if (GetLastError() == ERROR_IO_PENDING) {
            // printf("Read operation is pending...\n");
            GetOverlappedResult(file->handle, &overlapped, &dwBytesRead, TRUE);
            // printf("Read completed asynchronously!\n");
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
ssize_t file_aread(File* file, void* buffer, size_t size, off_t offset) {
    if (!file->is_open) {
        fprintf(stderr, "File is not open\n");
        return -1;
    }
    ssize_t bytes = -1;
    bytes         = pread(file->fd, buffer, size, offset);
    return bytes;
}
#endif

#ifdef _WIN32
DWORD file_awrite(File* file, const void* buffer, size_t size, off_t offset) {
    DWORD dwBytesWritten;
    OVERLAPPED overlapped;
    BOOL bResult;

    if (!file->is_open) {
        printf("File is not open!\n");
        return -1;
    }

    ZeroMemory(&overlapped, sizeof(overlapped));
    overlapped.Offset     = (DWORD)offset;
    overlapped.OffsetHigh = (DWORD)((DWORDLONG)offset >> 32);  // Set the high part of offset

    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        printf("Error creating event!\n");
        return -1;
    }

    bResult = WriteFile(file->handle, buffer, size, &dwBytesWritten, &overlapped);
    if (!bResult) {
        if (GetLastError() == ERROR_IO_PENDING) {
            // printf("Write operation is pending...\n");
            GetOverlappedResult(file->handle, &overlapped, &dwBytesWritten, TRUE);
            // printf("Write completed asynchronously!\n");
        } else {
            printf("Error writing file: %ld\n", GetLastError());
        }
    }

    CloseHandle(overlapped.hEvent);

    return dwBytesWritten;
}

#else
ssize_t file_awrite(File* file, const void* buffer, size_t size, off_t offset) {
    ssize_t bytes = -1;
    bytes         = pwrite(file->fd, buffer, size, offset);
    return bytes;
}
#endif

// filepath related functions
void filepath_basename(const char* path, char* basename, size_t size) {
    const char* base = strrchr(path, '/');  // Unix
    if (!base) {
        base = strrchr(path, '\\');  // Windows
    }
    if (!base) {
        base = path;
    } else {
        base++;  // Skip the slash
    }
    strncpy(basename, base, size - 1);
    basename[size - 1] = '\0';
}

// Get the directory name of a path
void filepath_dirname(const char* path, char* dirname, size_t size) {
    const char* base = strrchr(path, '/');  // Unix
    if (!base) {
        base = strrchr(path, '\\');  // Windows
    }
    if (!base) {
        dirname[0] = '\0';
    } else {
        size_t len = base - path;
        if (len >= size) {
            printf("Warning: dirname buffer is too small\n");
            return;
        }

        strncpy(dirname, path, len);
        dirname[len] = '\0';
    }
}

// Get the file extension
void filepath_extension(const char* path, char* ext, size_t size) {
    const char* dot = strrchr(path, '.');
    if (!dot) {
        ext[0] = '\0';
    } else {
        strncpy(ext, dot, size);
    }
}

// Get the file name(basename) without the extension
void filepath_nameonly(const char* path, char* name, size_t size) {
    char base[FILENAME_MAX];
    filepath_basename(path, base, FILENAME_MAX);
    char* dot = strrchr(base, '.');
    if (!dot) {
        strncpy(name, base, size);
    } else {
        size_t len = dot - base;
        strncpy(name, base, len);
        name[len] = '\0';
    }
}

// Get the absolute path of a file. path must be a valid path.
// Returns a pointer to the absolute path or NULL on error.
// The caller is responsible for freeing the memory
char* filepath_absolute(const char* path) {
#ifdef _WIN32
    char* abs = _fullpath(NULL, path, 0);
    if (!abs) {
        perror("_fullpath");
        return NULL;
    }
    return abs;
#else
    char* abs = realpath(path, NULL);
    if (!abs) {
        perror("realpath");
        return NULL;
    }
    return abs;
#endif
}

// Get the current working directory
char* get_cwd() {
    char* cwd = NULL;
#ifdef _WIN32
    cwd = _getcwd(NULL, 0);
#else
    cwd = getcwd(NULL, 0);
    if (!cwd) {
        perror("getcwd");
        return NULL;
    }
#endif
    return cwd;
}

// Remove a file
int filepath_remove(const char* path) {
    int ret = -1;
#ifdef _WIN
    ret = _unlink(path);
#else
    ret = unlink(path);
#endif
    return ret;
}

// Rename a file
int filepath_rename(const char* oldpath, const char* newpath) {
    return rename(oldpath, newpath);
}

// expand user home directory
char* filepath_expanduser(const char* path) {
#ifdef _WIN32
    char* home = getenv("USERPROFILE");
#else
    char* home = (char*)secure_getenv("HOME");
#endif
    if (!home) {
        fprintf(stderr, "USERPROFILE/HOME environment variable not set\n");
        return NULL;
    }

    // If the path is not a home directory path, return it as is
    if (path[0] != '~') {
        return strdup(path);
    }

    // If path is just "~" or ~/, return the home directory
    size_t pathLen = strlen(path);
    bool isHome    = pathLen == 1 || (pathLen == 2 && path[1] == '/');
    if (isHome) {
        return strdup(home);
    }

    // 1 for the separator and 1 for the null terminator
    size_t len = strlen(home) + pathLen + 2;

    // Allocate memory for the expanded path
    char* expanded = (char*)malloc(len);
    if (!expanded) {
        perror("malloc");
        return NULL;
    }
#ifdef _WIN32
    _snprintf(expanded, len, "%s\\%s", home, path);
#else
    snprintf(expanded, len, "%s/%s", home, path);
#endif
    return expanded;
}

// join path
char* filepath_join(const char* path1, const char* path2) {
    size_t len =
        strlen(path1) + strlen(path2) + 2;  // 1 for the separator and 1 for the null terminator
    char* joined = (char*)malloc(len);
    if (!joined) {
        perror("malloc");
        return NULL;
    }
#ifdef _WIN32
    _snprintf(joined, len, "%s\\%s", path1, path2);
#else
    snprintf(joined, len, "%s/%s", path1, path2);
#endif
    return joined;
}

/**
 Join path1 and path2 using standard os specific separator.
    @param path1: The first path
    @param path2: The second path
    @param abspath: The buffer to store the joined path
    @param len: The size of the buffer
*/
bool filepath_join_buf(const char* path1, const char* path2, char* abspath, size_t len) {
    size_t newlen = strlen(path1) + strlen(path2) + 2;
    if (newlen > len) {
        fprintf(stderr, "Buffer too small\n");
        return false;
    }

#ifdef _WIN32
    _snprintf(abspath, len, "%s\\%s", path1, path2);
#else
    snprintf(abspath, len, "%s/%s", path1, path2);
#endif
    return true;
}

/*
 * Split a file path into directory and file name.
 * The dir and name parameters must be pre-allocated buffers or pointers to pre-allocated
 * buffers. dir_size and name_size are the size of the dir and name buffers.
 */
void filepath_split(const char* path, char* dir, char* name, size_t dir_size, size_t name_size) {
    const char* p = strrchr(path, '/');
    if (!p) {
        p = strrchr(path, '\\');
    }

    if (!p) {
        dir[0] = '\0';
        strncpy(name, path, name_size);
        name[name_size - 1] = '\0';  // Ensure null-termination
    } else {
        size_t len = p - path;
        if (len >= dir_size) {
            len = dir_size - 1;
        }
        strncpy(dir, path, len);
        dir[len] = '\0';
        strncpy(name, p + 1, name_size);
        name[name_size - 1] = '\0';  // Ensure null-termination
    }
}

//  ===== Directory related functions =====

// Open a directory
Directory* dir_open(const char* path) {
    Directory* dir = (Directory*)malloc(sizeof(Directory));
    if (!dir) {
        perror("malloc");
        return NULL;
    }
    dir->path = strdup(path);
    if (!dir->path) {
        perror("strdup");
        free(dir);
        return NULL;
    }
#ifdef _WIN32
    char search_path[FILENAME_MAX];
    _snprintf(search_path, FILENAME_MAX, "%s\\*", path);
    dir->handle = FindFirstFileA(search_path, &dir->find_data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        perror("FindFirstFileA");
        free(dir->path);
        free(dir);
        return NULL;
    }
#else
    dir->dir = opendir(path);
    if (!dir->dir) {
        perror("opendir");
        free(dir->path);
        free(dir);
        return NULL;
    }
#endif
    return dir;
}

// Close a directory
void dir_close(Directory* dir) {
    if (dir) {
#ifdef _WIN32
        FindClose(dir->handle);
#else
        closedir(dir->dir);
#endif
        free(dir->path);
        free(dir);
    }
}

// Read the next entry in the directory.
// Returns the name of the next entry (not allocated on the heap)
// or NULL if there are no more entries
char* dir_next(Directory* dir) {
    if (!dir) {
        return NULL;
    }
#ifdef _WIN32
    if (FindNextFileA(dir->handle, &dir->find_data)) {
        return dir->find_data.cFileName;
    }
#else
    struct dirent* entry = readdir(dir->dir);
    if (entry) {
        return entry->d_name;
    }
#endif
    return NULL;
}

// Create a directory. Returns 0 if successful, -1 otherwise
// @param path: The path of the directory to create
// If the directory already exists, return 0.
// On Windows, the path must be in ANSI encoding.
int dir_create(const char* path) {
    int ret = -1;
#ifdef _WIN32
    ret = CreateDirectoryA(path, NULL);
    if (ret == 0) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            return 0;
        }
    }
#else
    ret = mkdir(path, 0755);
    // If alread exists, return 0
    if (ret == -1 && errno == EEXIST) {
        return 0;
    }
#endif

    return ret;
}

// Remove a directory. Returns 0 if successful, -1 otherwise
int dir_remove(const char* path) {
    int ret = -1;
#ifdef _WIN32
    ret = RemoveDirectoryA(path);
#else
    ret = rmdir(path);
#endif
    return ret;
}

// Rename a directory. Returns 0 if successful, -1 otherwise
int dir_rename(const char* oldpath, const char* newpath) {
    return rename(oldpath, newpath);
}

// Change the current working directory
int dir_chdir(const char* path) {
    return chdir(path);
}

// List files in a directory, returns a pointer to a list of file names or NULL on error
// The caller is responsible for freeing the memory.
// The number of files is stored in the count parameter.
// Note: This algorithm walks the directory tree recursively
// and may be slow for large directories.
char** dir_list(const char* path, size_t* count) {
    // Uses the dir_open and dir_next functions which are defined above
    Directory* dir = dir_open(path);
    if (!dir) {
        return NULL;
    }

    char** list = NULL;
    size_t size = 0;
    char* name;
    while ((name = dir_next(dir)) != NULL) {
        char** tmp = (char**)realloc(list, (size + 1) * sizeof(char*));
        if (!tmp) {
            perror("realloc");
            break;
        }
        list       = tmp;
        list[size] = strdup(name);
        if (!list[size]) {
            perror("strdup");
            break;
        }
        size++;
    }
    dir_close(dir);
    *count = size;
    return list;
}

bool is_dir(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
#endif
}

bool is_file(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
#endif
}

// Is the path a symbolic link?
bool is_symlink(const char* path) {
#ifdef _WIN32
    return false;  // Windows does not have symbolic links
#else
    struct stat st;
    if (lstat(path, &st) != 0) {
        return false;
    }
    return S_ISLNK(st.st_mode);
#endif
}

// Walk a directory tree recursively and call the callback function for each file
// The callback function should return 0 to continue or non-zero to stop the walk.
// dir_walk skips the "." and ".." directories.
//
// @param path: The directory to walk
// @param callback: The callback function to call for each file
// @param data: User data pointer to pass to the callback function
// Returns 0 if successful, -1 otherwise
int dir_walk(const char* path, WalkDirCallback callback, void* data) {
    Directory* dir = dir_open(path);
    if (!dir) {
        return -1;
    }

    char* name        = NULL;
    WalkDirOption ret = DirContinue;
    int success       = -1;

    while ((name = dir_next(dir)) != NULL) {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        char fullpath[FILENAME_MAX];
        if (!filepath_join_buf(path, name, fullpath, FILENAME_MAX)) {
            perror("filepath_join_buf() failed");
            break;
        };

        ret = callback(fullpath, name, data);
        if (ret == DirStop) {
            break;
        } else if (ret == DirContinue && is_dir(fullpath)) {
            success = dir_walk(fullpath, callback, data);
            if (success != 0) {
                break;
            }
        }
    }

    dir_close(dir);
    return 0;
}

static WalkDirOption dir_size_callback(const char* path, const char* name, void* data) {
    (void)name;  // Suppress unused variable warning

    ssize_t* size = (ssize_t*)data;
    if (is_file(path)) {
        File* file = file_open(path, "rb");
        if (file) {
            *size += file_size(file);
            file_close(file);
        }
    }
    return DirContinue;
}

// Get the directory size in bytes.
// Returns the size of the directory or -1 on error.
// Walks the directory tree recursively and sums the size of all files.
// @param path: The directory to walk.
// Warning: This function may be slow for large directories
ssize_t dir_size(const char* path) {
    ssize_t size = 0;
    int ret      = dir_walk(path, dir_size_callback, &size);
    if (ret != 0) {
        return -1;
    }
    return size;
}

// Create a directory recursively
bool makedirs(const char* path) {
    // if the directory already exists, return true
    if (is_dir(path)) {
        return true;
    }

#ifdef _WIN32
    char* p = strdup(path);
    if (!p) {
        perror("strdup");
        return false;
    }
    for (char* c = p; *c; c++) {
        if (*c == '/') {
            *c = '\\';
        }
    }

    // surround the path with quotes to handle spaces
    char cmd[FILENAME_MAX];
    _snprintf(cmd, FILENAME_MAX, "mkdir \"%s\"", p);
    int ret = system(cmd);
    free(p);
    return ret == 0;
#else
    Process proc;
    int status         = -1;
    const char* p      = path;
    const char* argv[] = {"mkdir", "-p", (char*)p, NULL};
    int ret            = process_create(&proc, "/bin/mkdir", argv, NULL);
    if (ret != 0) {
        return false;
    }
    ret = process_wait(&proc, &status);
    return status == 0;
#endif
}

// Returns the PATH for temporary files and directories
// in a platform specific manner.
// The caller is responsible for freeing the memory returned.
char* get_tempdir() {
#ifdef _WIN32
    char* tmp = getenv("TEMP");
    if (!tmp) {
        tmp = getenv("TMP");
    }
    if (!tmp) {
        tmp = "C:\\Windows\\Temp";
    }
    return strdup(tmp);
#else
    return strdup("/tmp");
#endif
}

// Make a temporary file
// Returns a pointer to the temporary file or NULL on error
// The caller is responsible for freeing the memory
char* make_tempfile() {
    char* tmpdir = get_tempdir();
    if (!tmpdir) {
        return NULL;
    }
    char* tmpfile = filepath_join(tmpdir, "tmpfileXXXXXX");
    if (!tmpfile) {
        free(tmpdir);
        return NULL;
    }
#ifdef _WIN32
    _mktemp(tmpfile);
#else
    mkstemp(tmpfile);
#endif
    free(tmpdir);
    return tmpfile;
}

// Make a temporary directory
// Returns a pointer to the temporary directory or NULL on error
// The caller is responsible for freeing the memory
char* make_tempdir() {
    char* tmpdir = get_tempdir();
    if (!tmpdir) {
        return NULL;
    }
    char* tmp = filepath_join(tmpdir, "tmpdirXXXXXX");
    if (!tmp) {
        free(tmpdir);
        return NULL;
    }
#ifdef _WIN32
    _mktemp(tmp);
#else
    mkdtemp(tmp);
#endif
    free(tmpdir);
    return tmp;
}

/*
Create a new process.
@param proc: Pointer to process.
The process startup info and process info will be zeroed for you
before calling CreateProcess on windows.
@param command: The command to execute
@param argv: The command line arguments
@param envp: The environment variables
@param pid: A pointer to an integer to store the process id
argv and envp are ignored on windows.
@return 0 if successful, -1 otherwise
*/
int process_create(Process* proc, const char* command, const char* const argv[],
                   const char* const envp[]) {
    int ret = -1;
#ifdef _WIN32

    ZeroMemory(&proc->si, sizeof(proc->si));
    proc->si.cb = sizeof(proc->si);
    ZeroMemory(&proc->pi, sizeof(proc->pi));

    // create the process
    if (CreateProcess(NULL, (LPSTR)command, NULL, NULL, FALSE, 0, NULL, NULL, &proc->si,
                      &proc->pi)) {

        proc->pid = proc->pi.dwProcessId;
        ret       = 0;
    } else {
        DWORD error = GetLastError();
        fprintf(stderr, "CreateProcess failed with error %lu\n", error);
    }

#else
    // Unix
    pid_t p = fork();
    if (p == 0) {
        // Child process
        execve(command, (char* const*)argv, (char* const*)envp);
        perror("execve");
        exit(1);  // Should not reach here
    } else if (p > 0) {
        // Parent process
        proc->pid = p;
        ret       = 0;
    }
#endif
    return ret;
}

/*
Wait for a process to exit.

@param pid: The process id
@param status: A pointer to an integer to store the exit status

@return 0 if successful, -1 otherwise
*/
int process_wait(Process* proc, int* status) {
    int ret = -1;
#ifdef _WIN32
    DWORD dw = WaitForSingleObject(proc->pi.hProcess, INFINITE);
    if (dw == WAIT_OBJECT_0) {
        DWORD exit_code;
        if (GetExitCodeProcess(proc->pi.hProcess, &exit_code)) {
            if (status) {
                *status = exit_code;
            }
            ret = 0;
        }
    }
    CloseHandle(proc->pi.hProcess);
    CloseHandle(proc->pi.hThread);

#else
    // Unix
    int stat;
    if (waitpid(proc->pid, &stat, 0) != -1) {
        if (WIFEXITED(stat)) {
            if (status) {
                *status = WEXITSTATUS(stat);
            }
            ret = 0;
        }
    }
#endif
    return ret;
}

/*

Kill a process.
@param pid: The process id
@return 0 if successful, -1 otherwise
*/
int process_kill(Process* proc) {
    int ret = -1;
#ifdef _WIN32
    // Windows
    if (TerminateProcess(proc->pi.hProcess, 0)) {
        ret = 0;
    }
    CloseHandle(proc->pi.hProcess);
    CloseHandle(proc->pi.hThread);
#else
    // Unix
    if (kill(proc->pid, SIGKILL) == 0) {
        ret = 0;
    }
#endif
    return ret;
}

// Create a new thread
// Returns 0 if successful, -1 otherwise
int thread_create(Thread* thread, void* (*start_routine)(void*), void* data) {
    int ret = -1;
#ifdef _WIN32
    // use CreateThread
    HANDLE t = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, data, 0, NULL);
    if (t) {
        *thread = t;
        ret     = 0;
    }
#else
    if (pthread_create(thread, NULL, start_routine, data) == 0) {
        ret = 0;
    }
#endif
    return ret;
}

// Join a thread using it's id
// Returns 0 if successful, -1 otherwise
// The retval parameter is used to store the return value of the thread
// If retval is NULL, the return value is discarded
int thread_join(Thread tid, void** retval) {
    int ret = -1;
#ifdef _WIN32
    if (WaitForSingleObject(tid, INFINITE) == WAIT_OBJECT_0) {
        if (retval) {
            GetExitCodeThread(tid, (DWORD*)retval);
        }
        CloseHandle(tid);
        ret = 0;
    }

#else
    // Unix
    if (pthread_join(tid, retval) == 0) {
        ret = 0;
    }
#endif
    return ret;
}

// Get the current thread id
int thread_self() {
    int id = -1;
#ifdef _WIN32
    id = GetCurrentThreadId();
#else
    id = pthread_self();
#endif
    return id;
}

// Detach a thread. Returns 0 if successful, -1 otherwise
// This does nothing on Windows.
int thread_detach(Thread tid) {
    int ret = -1;
#ifdef _WIN32
    // Windows does not have pthread_detach
    ret = 0;
#else
    // Unix
    if (pthread_detach(tid) == 0) {
        ret = 0;
    }
#endif
    return ret;
}

// Sleep for a number of milliseconds
void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

// Get the current process id
int get_pid() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

// Get the current thread id
int get_tid() {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}

// Get the parent process id
int get_ppid() {
#ifdef _WIN32
    return -1;  // Windows does not have parent process id
#else
    return getppid();
#endif
}

// Get the number of CPU cores
int get_ncpus() {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

#ifdef _WIN32
// No need for these functions on Windows
#else  // Unix
// Get the current user id
int get_uid() {
    return getuid();
}

// Get the current group id
int get_gid() {
    return getgid();
}

// Get the current user name
char* get_username() {
    return getpwuid(getuid())->pw_name;
}

// Get the current group name
char* get_groupname() {
    return getgrgid(getgid())->gr_name;
}
#endif

// =========== END Thread and process related functions ===========

#ifdef _WIN32
int pipe_open(PIPE* pPipe) {
    return CreatePipe(&(pPipe->hReadPipe), &(pPipe->hWritePipe), NULL, 0) ? 0 : -1;
}

void pipe_close(PIPE* pPipe, PipeEnd endToClose) {
    if (endToClose == PIPE_END_READ || endToClose == PIPE_END_BOTH) {
        CloseHandle(pPipe->hReadPipe);
    }
    if (endToClose == PIPE_END_WRITE || endToClose == PIPE_END_BOTH) {
        CloseHandle(pPipe->hWritePipe);
    }
}

ssize_t pipe_read(PIPE* pPipe, void* buffer, size_t size) {
    DWORD bytesRead;
    if (ReadFile(pPipe->hReadPipe, buffer, size, &bytesRead, NULL)) {
        return (ssize_t)bytesRead;
    }
    return -1;
}

ssize_t pipe_write(PIPE* pPipe, const void* buffer, size_t size) {
    DWORD bytesWritten;
    if (WriteFile(pPipe->hWritePipe, buffer, size, &bytesWritten, NULL)) {
        FlushFileBuffers(pPipe->hWritePipe);
        return (ssize_t)bytesWritten;
    }
    return -1;
}
#else
int pipe_open(PIPE* pPipe) {
    return pipe(pPipe->fd);
}

void pipe_close(PIPE* pPipe, PipeEnd endToClose) {
    if (endToClose == PIPE_END_READ || endToClose == PIPE_END_BOTH) {
        close(pPipe->fd[0]);
    }
    if (endToClose == PIPE_END_WRITE || endToClose == PIPE_END_BOTH) {
        close(pPipe->fd[1]);
    }
}

ssize_t pipe_read(PIPE* pPipe, void* buffer, size_t size) {
    return read(pPipe->fd[0], buffer, size);
}

ssize_t pipe_write(PIPE* pPipe, const void* buffer, size_t size) {
    return write(pPipe->fd[1], buffer, size);
}
#endif

// =========== END PIPE related functions ===========

#ifdef _WIN32
static DWORD WINAPI threadpool_worker(ThreadData* data) {
    ThreadPool* pool = (ThreadPool*)data->arg;
    free(data);

    while (1) {
        EnterCriticalSection(&pool->lock);
        // Wait for a task to be available or the pool to be shut down
        while (pool->task_queue == NULL && !pool->shutdown) {
            SleepConditionVariableCS(&pool->task_available, &pool->lock, INFINITE);
        }

        // If the pool is shut down, exit the thread
        if (pool->shutdown) {
            LeaveCriticalSection(&pool->lock);
            return 0;
        }

        // Get the next task
        pool->num_working_threads++;
        Task* task = pool->task_queue;
        if (task) {
            pool->task_queue = task->next;
        }

        // Signal that a task is available
        LeaveCriticalSection(&pool->lock);
        if (task != NULL) {
            task->function(task->arg);  // Execute the task
            free(task);                 // Free the task
            task = NULL;
        }

        // Signal that the task is completed
        EnterCriticalSection(&pool->lock);
        pool->num_working_threads--;

        // Signal that all tasks are completed
        if (pool->num_working_threads == 0) {
            WakeAllConditionVariable(&pool->all_tasks_completed);
        }
        LeaveCriticalSection(&pool->lock);  // Release the lock
    }
    return 0;
}
#else
static void* threadpool_worker(ThreadData* data) {
    ThreadPool* pool = (ThreadPool*)data->arg;
    free(data);

    while (1) {
        pthread_mutex_lock(&pool->lock);
        while (pool->task_queue == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->task_available, &pool->lock);
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }

        Task* task = pool->task_queue;
        if (task != NULL) {
            // Remove the task from the queue
            pool->task_queue = task->next;

            // Increment the number of working threads
            pool->num_working_threads++;

            // Unlock the pool
            pthread_mutex_unlock(&pool->lock);

            // printf("Thread %ld executing task\n", thread_self());
            // Execute the task
            task->function(task->arg);

            free(task);
            task = NULL;

            // Decrement the number of working threads
            pthread_mutex_lock(&pool->lock);
            pool->num_working_threads--;

            // Signal that a task has completed
            if (pool->num_working_threads == 0) {
                pthread_cond_signal(&pool->all_tasks_completed);
            }
            pthread_mutex_unlock(&pool->lock);
        } else {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }
    }

    return NULL;
}
#endif

ThreadPool* threadpool_create(int num_threads) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (!pool) {
        perror("malloc");
        return NULL;
    }
    pool->num_threads         = num_threads;
    pool->num_working_threads = 0;
    pool->shutdown            = false;
    pool->task_queue          = NULL;

    lock_init(&pool->lock);
    cond_init(&pool->task_available);
    cond_init(&pool->all_tasks_completed);

    Thread* threads = (Thread*)calloc(num_threads, sizeof(Thread));
    if (!threads) {
        perror("malloc");
        free(pool);
        return NULL;
    }

    // create worker threads
    for (int i = 0; i < num_threads; i++) {
        ThreadData* data = (ThreadData*)malloc(sizeof(ThreadData));
        if (!data) {
            perror("malloc");
            threadpool_destroy(pool);
            return NULL;
        }
        data->arg    = pool;
        data->retval = NULL;

        if (thread_create(&threads[i], (void* (*)(void*))threadpool_worker, data) != 0) {
            perror("thread_create");
            free(data);
            threadpool_destroy(pool);
            return NULL;
        }
    }
    return pool;
}

// Add a task to the thread pool.
// Returns 0 if successful, -1 otherwise
// The threadpool with free the memory for arg after the task is completed.
int threadpool_add_task(ThreadPool* pool, void* (*task)(void*), void* arg) {
    if (pool == NULL || task == NULL) {
        return -1;
    }

    Task* new_task = (Task*)malloc(sizeof(Task));
    if (!new_task) {
        perror("malloc");
        return -1;
    }

    new_task->function = task;
    new_task->arg      = arg;
    new_task->next     = NULL;

    // Lock the pool
    lock_acquire(&pool->lock);

    // Add the task to the queue
    if (pool->task_queue == NULL) {
        pool->task_queue = new_task;
    } else {
        Task* current_task = pool->task_queue;
        while (current_task->next != NULL) {
            current_task = current_task->next;
        }
        current_task->next = new_task;
    }

    // Signal that a task is available
    cond_signal(&pool->task_available);

    // Unlock the pool
    lock_release(&pool->lock);
    return 0;
}

void threadpool_wait(ThreadPool* pool) {
    if (pool == NULL)
        return;

    // Lock the pool
    lock_acquire(&pool->lock);

    // Wait for all threads to complete
    while (pool->task_queue != NULL || pool->num_working_threads > 0) {
        cond_wait(&pool->all_tasks_completed, &pool->lock);
    }
    // Unlock the pool
    lock_release(&pool->lock);
}

void threadpool_destroy(ThreadPool* pool) {
    if (!pool)
        return;

    if (pool->shutdown) {
        return;
    }

    lock_acquire(&pool->lock);
    pool->shutdown = true;                  // Signal all threads to shut down
    cond_broadcast(&pool->task_available);  // Signal all threads to wake up

    // Wait for all threads to complete
    while (pool->num_working_threads > 0) {
        cond_wait(&pool->all_tasks_completed, &pool->lock);
    }

    // Unlock the pool
    lock_release(&pool->lock);
    lock_free(&pool->lock);
    cond_free(&pool->task_available);
    cond_free(&pool->all_tasks_completed);

    free(pool->threads);
    free(pool);
    pool = NULL;
}
#endif  // OS_IMPL

#endif  // OS_H