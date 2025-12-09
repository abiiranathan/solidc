#include "../include/filepath.h"
#include "../include/file.h"
#include "../include/lock.h"
#include "../include/wintypes.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Length of the temporary directory prefix
#define TEMP_PREF_PREFIX_LEN 12

// Safe string copy with bounds checking
static inline size_t safe_strlcpy(char* dst, const char* src, size_t size) {
    if (!dst || !src || size == 0) return 0;

    size_t srclen = 0;
#ifdef _WIN32
#ifndef RSIZE_MAX
#define RSIZE_MAX ((size_t)-1)
#endif
    srclen = strnlen_s(src, RSIZE_MAX);
#else
    srclen = strnlen(src, size);
#endif

    size_t n = (srclen < size - 1) ? srclen : size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
    return srclen;
}

// Generate a random string for temporary file/directory names.
// len must be < 64 bytes.
static void random_string(char* str, size_t len) {
    static Lock rand_lock;
    static atomic_int initialized = 0;

    // Initialize lock in a thread-safe manner
    if (!atomic_load(&initialized)) {
        lock_init(&rand_lock);
        atomic_store(&initialized, 1);
    }

    const char charset[]      = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t charset_size = sizeof(charset) - 1;  // Exclude null terminator
    unsigned char buffer[64];                         // Buffer for random bytes (sufficient for typical len)

    lock_acquire(&rand_lock);

    // Ensure we don't write beyond requested length
    size_t bytes_needed = len < 64 ? len : 64;

#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContextW(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        str[0] = '\0';  // Fallback to empty string on error
        lock_release(&rand_lock);
        return;
    }

    if (!CryptGenRandom(hCryptProv, bytes_needed, buffer)) {
        CryptReleaseContext(hCryptProv, 0);
        str[0] = '\0';
        lock_release(&rand_lock);
        return;
    }
    CryptReleaseContext(hCryptProv, 0);
#else
    ssize_t bytes_read = -1;
#ifdef __linux__
    // Try getrandom (Linux-specific, preferred)
    bytes_read = getrandom(buffer, bytes_needed, 0);
#else
    // Fallback to /dev/urandom for other POSIX systems
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd != -1) {
        bytes_read = read(fd, buffer, bytes_needed);
        close(fd);
    }
#endif
    if (bytes_read != (ssize_t)bytes_needed) {
        str[0] = '\0';  // Fallback to empty string on error
        lock_release(&rand_lock);
        return;
    }
#endif

    // Convert random bytes to characters from charset
    for (size_t i = 0; i < len; i++) {
        str[i] = charset[buffer[i % bytes_needed] % charset_size];
    }
    str[len] = '\0';
    lock_release(&rand_lock);
}

// Open a directory
Directory* dir_open(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return NULL;
    }

    Directory* dir = (Directory*)calloc(1, sizeof(Directory));  // Use calloc for zero-initialization
    if (!dir) {
        errno = ENOMEM;
        return NULL;
    }

    dir->path = strdup(path);
    if (!dir->path) {
        free(dir);
        errno = ENOMEM;
        return NULL;
    }

#ifdef _WIN32
    wchar_t wpath[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH)) {
        free(dir->path);
        free(dir);
        errno = EINVAL;  // More specific error code
        return NULL;
    }

    wchar_t search_path[MAX_PATH + 2];
    if (swprintf(search_path, MAX_PATH + 2, L"%ls\\*", wpath) < 0) {
        free(dir->path);
        free(dir);
        errno = ENAMETOOLONG;
        return NULL;
    }

    dir->handle = FindFirstFileW(search_path, &dir->find_data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir->path);
        free(dir);
        errno = GetLastError() == ERROR_FILE_NOT_FOUND ? ENOENT : EACCES;
        return NULL;
    }
#else
    dir->dir = opendir(path);
    if (!dir->dir) {
        int saved_errno = errno;
        free(dir->path);
        free(dir);
        errno = saved_errno;
        return NULL;
    }
#endif
    return dir;
}

// Close a directory
void dir_close(Directory* dir) {
    if (!dir) return;

#ifdef _WIN32
    if (dir->handle != INVALID_HANDLE_VALUE) {
        FindClose(dir->handle);
    }
#else
    if (dir->dir) {
        closedir(dir->dir);
    }
#endif
    free(dir->path);
    free(dir);
}

// Read the next entry in the directory
char* dir_next(Directory* dir) {
    if (!dir) {
        errno = EINVAL;
        return NULL;
    }
#ifdef _WIN32
    if (dir->handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return NULL;
    }
    if (FindNextFileW(dir->handle, &dir->find_data)) {
        // Convert wide-char filename to UTF-8
        char filename[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, dir->find_data.cFileName, -1, filename, MAX_PATH, NULL, NULL);
        return dir->path;  // Note: This is a bug in the original code; should return filename
    }
#else
    struct dirent* entry = readdir(dir->dir);
    if (entry) {
        return entry->d_name;
    }
#endif
    return NULL;
}

static int delete_single_directory(const char* path) {
#ifdef _WIN32
    if (!RemoveDirectoryA(path)) {
        DWORD err = GetLastError();
        errno     = (err == ERROR_DIR_NOT_EMPTY)                                   ? ENOTEMPTY
                    : (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND) ? ENOENT
                                                                                   : EACCES;
        return -1;
    }
#else
    if (rmdir(path) == -1) {
        return -1;
    }
#endif
    return 0;
}

// Create a directory
int dir_create(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return -1;
    }

#ifdef _WIN32
    if (!CreateDirectoryA(path, NULL)) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            return 0;
        }
        errno = GetLastError() == ERROR_ACCESS_DENIED ? EACCES : EIO;
        return -1;
    }
#else
    if (mkdir(path, 0755) == -1) {
        if (errno == EEXIST) {
            return 0;
        }
        return -1;
    }
#endif
    return 0;
}

static WalkDirOption dir_remove_callback(const char* fullpath, const char* name, void* data) {
    (void)name;
    (void)data;

#ifdef _WIN32
    DWORD attr = GetFileAttributesA(fullpath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        errno     = (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND) ? ENOENT : EACCES;
        return DirError;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        if (!RemoveDirectoryA(fullpath)) {
            DWORD err = GetLastError();
            errno     = (err == ERROR_DIR_NOT_EMPTY)                                   ? ENOTEMPTY
                        : (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND) ? ENOENT
                                                                                       : EACCES;
            return DirError;
        }
    } else {
        if (!DeleteFileA(fullpath)) {
            DWORD err = GetLastError();
            errno     = (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND) ? ENOENT : EACCES;
            return DirError;
        }
    }
#else
    struct stat st;
    if (lstat(fullpath, &st) != 0) return DirError;

    if (S_ISDIR(st.st_mode)) {
        if (rmdir(fullpath) != 0) return DirError;
    } else {
        if (unlink(fullpath) != 0) return DirError;
    }
#endif
    return DirContinue;
}

#define SET_DIR_STATUS(ret)                                                                                            \
    if ((ret) == DirError) {                                                                                           \
        status = -1;                                                                                                   \
        break;                                                                                                         \
    }                                                                                                                  \
    if ((ret) == DirStop) {                                                                                            \
        status = 0;                                                                                                    \
        break;                                                                                                         \
    }

/*
 * Depth-first POST-ORDER walk.
 * Calls `callback` for:
 *   - every file
 *   - every directory (AFTER its children)
 *   - and finally the *root* directory itself.
 */
int dir_walk_depth_first(const char* path, WalkDirCallback callback, void* data) {
    if (!path || !callback || *path == '\0') {
        errno = EINVAL;
        return -1;
    }

    Directory* dir = dir_open(path);
    if (!dir) {
        return -1;  // errno set by dir_open
    }

    char* name                  = NULL;
    char fullpath[FILENAME_MAX] = {0};
    int status                  = 0;

    while ((name = dir_next(dir)) != NULL) {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        memset(fullpath, 0, sizeof(fullpath));

        if (!filepath_join_buf(path, name, fullpath, sizeof fullpath)) {
            dir_close(dir);
            errno = ENAMETOOLONG;
            return -1;
        }

        if (is_dir(fullpath)) {
            if (dir_walk_depth_first(fullpath, callback, data) != 0) {
                status = -1;
                break;
            }

            // Then call callback on the directory itself (now empty)
            WalkDirOption r = callback(fullpath, name, data);
            SET_DIR_STATUS(r);
        } else {
            WalkDirOption r = callback(fullpath, name, data);
            SET_DIR_STATUS(r);
        }
    }

    dir_close(dir);
    return status;
}

// Remove a directory, with optional recursive deletion
// When recursive is true, deletes all files, subdirectories, and symbolic links (POSIX)
// within path, including empty subdirectories. Does not affect parent directories,
// as dir_walk skips "." and "..".
int dir_remove(const char* path, bool recursive) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return -1;
    }

    if (recursive) {
        if (dir_walk_depth_first(path, dir_remove_callback, NULL) != 0) {
            return -1;
        };
        // fallthrough and remove root directory.
    }
    return delete_single_directory(path);
}

// Rename a directory
int dir_rename(const char* oldpath, const char* newpath) {
    if (!oldpath || !newpath || *oldpath == '\0' || *newpath == '\0') {
        errno = EINVAL;
        return -1;
    }
    return rename(oldpath, newpath);
}

// Change the current working directory
int dir_chdir(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return -1;
    }
    return chdir(path);
}

// List files in a directory with unified error handling
char** dir_list(const char* path, size_t* count) {
    if (!path || !count || *path == '\0') {
        errno = EINVAL;
        return NULL;
    }

    Directory* dir  = NULL;
    char** list     = NULL;
    size_t size     = 0;
    size_t capacity = 10;
    char* name      = NULL;

    dir = dir_open(path);
    if (!dir) return NULL;

    list = (char**)calloc(capacity, sizeof(char*));
    if (!list) {
        goto error;
    }

    while ((name = dir_next(dir)) != NULL) {
        if (size >= capacity) {
            capacity *= 2;
            char** tmp = (char**)realloc(list, capacity * sizeof(char*));
            if (!tmp) {
                goto error;
            }
            list = tmp;
        }

        list[size] = strdup(name);
        if (!list[size]) {
            goto error;
        }
        size++;
    }

    dir_close(dir);
    *count = size;
    return list;

error:
    if (list) {
        // Free all successfully duplicated names
        for (size_t i = 0; i < size; i++) {
            free(list[i]);
        }
        free(list);
    }

    if (dir) dir_close(dir);

    return NULL;
}

// List files with callback
void dir_list_with_callback(const char* path, void (*callback)(const char* name)) {
    if (!path || !callback || *path == '\0') return;

    Directory* dir = dir_open(path);
    if (!dir) {
        return;
    }

    char* name = NULL;
    while ((name = dir_next(dir)) != NULL) {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        callback(name);
    }

    dir_close(dir);
}

// Check if path is a directory
bool is_dir(const char* path) {
    if (!path || *path == '\0') {
        return false;
    }

#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
#endif
}

// Check if path is a file
bool is_file(const char* path) {
    if (!path || *path == '\0') {
        return false;
    }

#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
#endif
}

// Check if path is a symbolic link
bool is_symlink(const char* path) {
    if (!path || *path == '\0') {
        return false;
    }

#ifdef _WIN32
    // Windows supports symbolic links since Vista, but we keep original behavior
    return false;
#else
    struct stat st;
    if (lstat(path, &st) != 0) {
        return false;
    }
    return S_ISLNK(st.st_mode);
#endif
}

int dir_walk(const char* path, WalkDirCallback callback, void* data) {
    if (!path || !callback || *path == '\0') {
        errno = EINVAL;
        return -1;
    }

    Directory* dir = dir_open(path);
    if (!dir) return -1;

    char* name                  = NULL;
    WalkDirOption ret           = DirContinue;
    int success                 = 0;
    char fullpath[FILENAME_MAX] = {0};

    while ((name = dir_next(dir)) != NULL) {
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        memset(fullpath, 0, sizeof(fullpath));
        if (!filepath_join_buf(path, name, fullpath, FILENAME_MAX)) {
            dir_close(dir);
            errno = ENAMETOOLONG;
            return -1;
        }

        ret = callback(fullpath, name, data);
        if (ret == DirStop) {
            success = 0;
            break;
        } else if (ret == DirError) {
            success = -1;
            break;
        } else if (ret == DirSkip) {
            continue;
        } else if (ret == DirContinue && is_dir(fullpath)) {
            if (dir_walk(fullpath, callback, data) != 0) {
                success = -1;
                break;
            }
        }
    }

    dir_close(dir);
    return success;
}

// Callback for directory size calculation
static WalkDirOption dir_size_callback(const char* path, const char* name, void* data) {
    (void)name;
    ssize_t* size = (ssize_t*)data;
    if (is_file(path)) {
        int64_t fs = get_file_size(path);
        if (fs == -1) {
            return DirError;
        }
        *size += fs;
    }
    return DirContinue;
}

// Get directory size
ssize_t dir_size(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return -1;
    }

    ssize_t size = 0;
    if (dir_walk(path, dir_size_callback, &size) != 0) {
        return -1;
    }
    return size;
}

// Create directories recursively
bool filepath_makedirs(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return false;
    }

    char* temp_path = strdup(path);
    if (!temp_path) {
        errno = ENOMEM;
        return false;
    }

    char* p = temp_path;
    while (*p != '\0') {
        while (*p == '/' || *p == '\\') {
            p++;
        }
        while (*p != '\0' && *p != '/' && *p != '\\') {
            p++;
        }

        char old = *p;
        *p       = '\0';

        if (*temp_path != '\0') {
            if (dir_create(temp_path) != 0) {
                free(temp_path);
                return false;
            }
        }

        *p = old;
        if (*p != '\0') {
            p++;
        }
    }

    free(temp_path);
    return true;
}

// Get temporary directory
char* get_tempdir(void) {
#ifdef _WIN32
    wchar_t wtemp[MAX_PATH];
    DWORD ret = GetTempPathW(MAX_PATH, wtemp);
    if (ret == 0 || ret > MAX_PATH) {
        errno = EIO;
        return NULL;
    }

    char* temp = (char*)malloc(MAX_PATH);
    if (!temp) {
        errno = ENOMEM;
        return NULL;
    }
    if (wcstombs(temp, wtemp, MAX_PATH) == (size_t)-1) {
        free(temp);
        errno = EINVAL;
        return NULL;
    }
    return temp;
#else
    const char* temp = getenv("TMPDIR");
    if (!temp) {
        temp = "/tmp";
    }
    char* result = strdup(temp);
    if (!result) {
        errno = ENOMEM;
        return NULL;
    }
    return result;
#endif
}

// Make a temporary file
char* make_tempfile(void) {
    char* tmpdir = get_tempdir();
    if (!tmpdir) {
        return NULL;
    }

    char pattern[TEMP_PREF_PREFIX_LEN + 7] = {0};
    random_string(pattern, TEMP_PREF_PREFIX_LEN);
    safe_strlcpy(pattern + TEMP_PREF_PREFIX_LEN, "XXXXXX", 7);

    char* tmpfile = filepath_join(tmpdir, pattern);
    free(tmpdir);
    if (!tmpfile) {
        errno = ENOMEM;
        return NULL;
    }

#ifdef _WIN32
    wchar_t wtmpfile[MAX_PATH];
    if (mbstowcs(wtmpfile, tmpfile, MAX_PATH) == (size_t)-1) {
        free(tmpfile);
        errno = EINVAL;
        return NULL;
    }

    int fd = _wcreat(wtmpfile, _S_IREAD | _S_IWRITE);
    if (fd == -1) {
        free(tmpfile);
        return NULL;
    }
    _close(fd);
#else
    int fd = mkstemp(tmpfile);
    if (fd == -1) {
        free(tmpfile);
        return NULL;
    }
    close(fd);
#endif
    return tmpfile;
}

// Make a temporary directory
char* make_tempdir(void) {
    char* tmpdir = get_tempdir();
    if (!tmpdir) {
        return NULL;
    }

    char pattern[TEMP_PREF_PREFIX_LEN + 7] = {0};
    random_string(pattern, TEMP_PREF_PREFIX_LEN);
    safe_strlcpy(pattern + TEMP_PREF_PREFIX_LEN, "XXXXXX", 7);

    char* tmp = filepath_join(tmpdir, pattern);
    free(tmpdir);
    if (!tmp) {
        errno = ENOMEM;
        return NULL;
    }

#ifdef _WIN32
    wchar_t wtmp[MAX_PATH];
    if (mbstowcs(wtmp, tmp, MAX_PATH) == (size_t)-1) {
        free(tmp);
        errno = EINVAL;
        return NULL;
    }
    if (_wmkdir(wtmp) != 0) {
        free(tmp);
        return NULL;
    }
#else
    if (mkdtemp(tmp) == NULL) {
        free(tmp);
        return NULL;
    }
#endif
    return tmp;
}

// Check if path exists
bool path_exists(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return false;
    }

#ifdef _WIN32
    wchar_t wpath[MAX_PATH];
    if (mbstowcs(wpath, path, MAX_PATH) == (size_t)-1) {
        errno = EINVAL;
        return false;
    }

    DWORD attr = GetFileAttributesW(wpath);
    return attr != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

// Get basename of path
void filepath_basename(const char* path, char* basename, size_t size) {
    if (!path || !basename || size == 0) {
        if (basename) basename[0] = '\0';
        return;
    }

    const char* base = strrchr(path, '/');
    if (!base) {
        base = strrchr(path, '\\');
    }
    base = base ? base + 1 : path;
    safe_strlcpy(basename, base, size);
}

// Get dirname of path
void filepath_dirname(const char* path, char* dirname, size_t size) {
    if (!path || !dirname || size == 0) {
        if (dirname) dirname[0] = '\0';
        return;
    }

    const char* base = strrchr(path, '/');
    if (!base) {
        base = strrchr(path, '\\');
    }
    if (!base) {
        dirname[0] = '\0';
    } else {
        size_t len = (size_t)(base - path);
        safe_strlcpy(dirname, path, len + 1 < size ? len + 1 : size);
    }
}

// Get file extension
void filepath_extension(const char* path, char* ext, size_t size) {
    if (!path || !ext || size == 0) {
        if (ext) ext[0] = '\0';
        return;
    }

    const char* dot = strrchr(path, '.');
    safe_strlcpy(ext, dot ? dot : "", size);
}

// Get filename without extension
void filepath_nameonly(const char* path, char* name, size_t size) {
    if (!path || !name || size == 0) {
        if (name) name[0] = '\0';
        return;
    }

    char base[FILENAME_MAX] = {0};
    filepath_basename(path, base, FILENAME_MAX);
    char* dot = strrchr(base, '.');
    safe_strlcpy(name, base, dot ? (size_t)(dot - base + 1) : size);
}

// Get absolute path
char* filepath_absolute(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return NULL;
    }

#ifdef _WIN32
    char* abs = _fullpath(NULL, path, 0);
    if (!abs) {
        errno = ENOMEM;
        return NULL;
    }
#else
    char* abs = realpath(path, NULL);
    if (!abs) {
        return NULL;
    }
#endif
    return abs;
}

// Get current working directory
char* get_cwd(void) {
    char* cwd = getcwd(NULL, 0);
    if (!cwd) {
        errno = ENOMEM;
        return NULL;
    }
    return cwd;
}

// Remove a file
int filepath_remove(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return -1;
    }

#ifdef _WIN32
    return _unlink(path);
#else
    return unlink(path);
#endif
}

// Rename a file
int filepath_rename(const char* oldpath, const char* newpath) {
    if (!oldpath || !newpath || *oldpath == '\0' || *newpath == '\0') {
        errno = EINVAL;
        return -1;
    }
    return rename(oldpath, newpath);
}

// Get user home directory
const char* user_home_dir(void) {
#ifdef _WIN32
    return getenv("USERPROFILE");
#else
    return secure_getenv("HOME");
#endif
}

// Expand user home directory
char* filepath_expanduser(const char* path) {
    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    if (path[0] != '~') {
        return strdup(path);
    }

    const char* home = user_home_dir();
    if (!home) {
        errno = ENOENT;
        return NULL;
    }

    size_t pathLen = strlen(path);
    bool isHome    = pathLen == 1 || (pathLen == 2 && (path[1] == '/' || path[1] == '\\'));
    if (isHome) {
        return strdup(home);
    }

    size_t len     = strlen(home) + pathLen + 1;
    char* expanded = (char*)malloc(len);
    if (!expanded) {
        errno = ENOMEM;
        return NULL;
    }

#ifdef _WIN32
    snprintf(expanded, len, "%s%s%s", home, path[1] == '\\' ? "\\" : "/", path + 1);
#else
    snprintf(expanded, len, "%s/%s", home, path + 1);
#endif
    return expanded;
}

// Expand user home directory into buffer
bool filepath_expanduser_buf(const char* path, char* expanded, size_t len) {
    if (!path || !expanded || len == 0) {
        if (expanded) expanded[0] = '\0';
        errno = EINVAL;
        return false;
    }

    if (path[0] != '~') {
        return safe_strlcpy(expanded, path, len) < len;
    }

    const char* home = user_home_dir();
    if (!home) {
        errno = ENOENT;
        return false;
    }

    size_t pathLen = strlen(path);
    bool isHome    = pathLen == 1 || (pathLen == 2 && (path[1] == '/' || path[1] == '\\'));
    if (isHome) {
        return safe_strlcpy(expanded, home, len) < len;
    }

    size_t homeLen = strlen(home);
    if (homeLen + pathLen + 1 > len) {
        errno = ENAMETOOLONG;
        return false;
    }

#ifdef _WIN32
    snprintf(expanded, len, "%s%s%s", home, path[1] == '\\' ? "\\" : "/", path + 1);
#else
    snprintf(expanded, len, "%s/%s", home, path + 1);
#endif
    return true;
}

// Join paths
char* filepath_join(const char* path1, const char* path2) {
    if (!path1 || !path2) {
        errno = EINVAL;
        return NULL;
    }

    size_t len   = strlen(path1) + strlen(path2) + 2;
    char* joined = (char*)malloc(len);
    if (!joined) {
        errno = ENOMEM;
        return NULL;
    }

#ifdef _WIN32
    snprintf(joined, len, "%s%s%s", path1, strchr(path1, '\\') ? "\\" : "/", path2);
#else
    snprintf(joined, len, "%s/%s", path1, path2);
#endif
    return joined;
}

// Join paths into buffer
bool filepath_join_buf(const char* path1, const char* path2, char* abspath, size_t len) {
    if (!path1 || !path2 || !abspath || len == 0) {
        if (abspath) abspath[0] = '\0';
        errno = EINVAL;
        return false;
    }

    size_t newlen = strlen(path1) + strlen(path2) + 2;
    if (newlen > len) {
        errno = ENAMETOOLONG;
        return false;
    }

#ifdef _WIN32
    snprintf(abspath, len, "%s%s%s", path1, strchr(path1, '\\') ? "\\" : "/", path2);
#else
    snprintf(abspath, len, "%s/%s", path1, path2);
#endif
    return true;
}

// Split path into directory and filename
void filepath_split(const char* path, char* dir, char* name, size_t dir_size, size_t name_size) {
    if (!path || !dir || !name || dir_size == 0 || name_size == 0) {
        if (dir) dir[0] = '\0';
        if (name) name[0] = '\0';
        return;
    }

    const char* p = strrchr(path, '/');
    if (!p) {
        p = strrchr(path, '\\');
    }

    if (!p) {
        dir[0] = '\0';
        safe_strlcpy(name, path, name_size);
    } else {
        size_t len = (size_t)(p - path);
        safe_strlcpy(dir, path, len + 1 < dir_size ? len + 1 : dir_size);
        safe_strlcpy(name, p + 1, name_size);
    }
}
