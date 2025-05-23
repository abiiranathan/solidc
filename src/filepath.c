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

// Helper function for safer string copy
static inline size_t safe_strlcpy(char* dst, const char* src, size_t size) {
    if (!dst || !src || size == 0)
        return 0;  // Input validation

#ifdef _WIN32
    size_t srclen = strnlen_s(src, size);  // safer string length check
#else
    size_t srclen = strnlen(src, size);
#endif
    size_t n = srclen < size - 1 ? srclen : size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
    return srclen;  // Returns original length
}

static void random_string(char* str, size_t len) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (size_t i = 0; i < len; i++) {
        size_t index = (unsigned int)rand() % (sizeof(charset) - 1);
        str[i]       = charset[index];
    }
    str[len] = '\0';
}

// Open a directory
Directory* dir_open(const char* path) {
    // Input validation
    if (!path || *path == '\0') {
        errno = EINVAL;
        return NULL;
    }

    // Allocate directory structure
    Directory* dir = (Directory*)malloc(sizeof(Directory));
    if (!dir) {
        perror("malloc");
        return NULL;
    }

    // Store path
    dir->path = strdup(path);
    if (!dir->path) {
        perror("strdup");
        free(dir);
        return NULL;
    }

#ifdef _WIN32
    // Convert to wide char
    wchar_t wpath[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH)) {
        free(dir->path);
        free(dir);
        return NULL;
    }

    // Open directory
    wchar_t search_path[MAX_PATH + 2];
    swprintf(search_path, MAX_PATH + 2, L"%s\\*", wpath);

    dir->handle = FindFirstFileW(search_path, &dir->find_data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir->path);
        free(dir);
        return NULL;
    }
#else
    dir->dir = opendir(path);
    if (!dir->dir) {
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
    if (FindNextFileW(dir->handle, &dir->find_data)) {
        return (char*)dir->find_data.cFileName;
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

// List files in a directory, returns a pointer to a list of file names or NULL
// on error The caller is responsible for freeing the memory. The number of
// files is stored in the count parameter. Note: This algorithm walks the
// directory tree recursively and may be slow for large directories.
char** dir_list(const char* path, size_t* count) {
    // Uses the dir_open and dir_next functions which are defined above
    Directory* dir = dir_open(path);
    if (!dir) {
        return NULL;
    }

    char** list     = NULL;
    size_t size     = 0;   // Number of files in the list so far
    size_t capacity = 10;  // Initial capacity to minimize reallocations
    list            = (char**)malloc(capacity * sizeof(char*));
    if (!list) {
        perror("malloc");
        dir_close(dir);
        return NULL;
    }

    char* name;  // Tracks the current file name
    while ((name = dir_next(dir)) != NULL) {
        if (size >= capacity) {
            capacity *= 2;
            char** tmp = (char**)realloc(list, capacity * sizeof(char*));
            if (!tmp) {
                perror("realloc");
                goto cleanup;
            }
            list = tmp;
        }

        list[size] = strdup(name);
        if (!list[size]) {
            perror("strdup");
            goto cleanup;
        }
        size++;
    }

    dir_close(dir);
    *count = size;
    return list;

// Cleanup on error
cleanup:
    // Free already allocated memory if an error occurs
    if (list) {
        for (size_t i = 0; i < size; i++) {
            free(list[i]);
        }
        free(list);
    }
    dir_close(dir);
    return NULL;
}

// List all files in directory, recursively and passes the name in a callback.
void dir_list_with_callback(const char* path, void (*callback)(const char* name)) {
    Directory* dir = dir_open(path);
    if (!dir) {
        return;
    }

    char* name = NULL;  // Tracks the current file name
    while ((name = dir_next(dir)) != NULL) {
        callback(name);
    }

    dir_close(dir);
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
    (void)path;
#else
    struct stat st;
    if (lstat(path, &st) != 0) {
        return false;
    }
    return S_ISLNK(st.st_mode);
#endif
}

// Walk a directory tree recursively and call the callback function for each
// file The callback function should return 0 to continue or non-zero to stop
// the walk. dir_walk skips the "." and ".." directories.
//
// @param path: The directory to walk
// @param callback: The callback function to call for each file
// @param data: User data pointer to pass to the callback function
// Warning: The path is allocated on the stack and there for should not be
// stored beyound the scope of the callback function. Returns 0 if successful,
// -1 otherwise
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

        char fullpath[FILENAME_MAX] = {0};
        if (!filepath_join_buf(path, name, fullpath, FILENAME_MAX)) {
            perror("filepath_join_buf() failed");
            break;
        };

        ret = callback(fullpath, name, data);

        if (ret == DirStop) {
            break;  // Stop the walk
        } else if (ret == DirError) {
            dir_close(dir);
            return -1;  // An error occured.
        } else if (ret == DirSkip) {
            continue;  // Skip the current entry
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

// dir_size_callback is a callback function for dir_walk to sum the size of all
// files in a directory. The data parameter is a pointer to a ssize_t variable
// that stores the size of the directory.
static WalkDirOption dir_size_callback(const char* path, const char* name, void* data) {
    (void)name;  // Suppress unused variable warning

    ssize_t* size = (ssize_t*)data;
    if (is_file(path)) {
        int64_t fs = get_file_size(path);
        if (fs == -1) {
            perror("get_file_size");
            return DirError;
        }
        *size += fs;
    }

    // Continue walking the directory
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
bool filepath_makedirs(const char* path) {
    char* temp_path = strdup(path);
    if (temp_path == NULL) {
        return false;
    }

    char* p = temp_path;
    while (*p != '\0') {
        // Skip the initial separators
        while (*p == '/' || *p == '\\') {
            p++;
        }

        // Find the next separator
        while (*p != '\0' && *p != '/' && *p != '\\') {
            p++;
        }

        // Temporarily terminate the string here
        char old = *p;
        *p       = '\0';

        // Create the directory
        if (strlen(temp_path) > 0) {
            if (MKDIR(temp_path) != 0) {
#ifdef _WIN32
                if (GetLastError() != ERROR_ALREADY_EXISTS) {
                    free(temp_path);
                    return false;
                }
#else
                if (errno != EEXIST) {
                    free(temp_path);
                    return false;
                }
#endif
            }
        }

        // Restore the character
        *p = old;

        // Move to the next part of the path
        if (*p != '\0') {
            p++;
        }
    }

    free(temp_path);
    return true;
}

char* get_tempdir(void) {
#ifdef _WIN32
    wchar_t wtemp[FILENAME_MAX];
    DWORD ret = GetTempPathW(FILENAME_MAX, wtemp);
    if (ret > FILENAME_MAX || ret == 0) {
        fprintf(stderr, "get_tempdir: GetTempPathW failed.\n");
        return NULL;
    }

    // Convert to char
    char* temp = (char*)malloc(FILENAME_MAX);
    if (!temp) {
        perror("get_tempdir: malloc");
        return NULL;
    }
    wcstombs(temp, wtemp, FILENAME_MAX);
    return temp;
#else
    const char* temp = getenv("TMPDIR");
    if (!temp) {
        temp = "/tmp";
    }
    return strdup(temp);
#endif
}

// Make a temporary file
// Returns a pointer to the temporary file or NULL on error
// The caller is responsible for freeing the memory
char* make_tempfile(void) {
    char* tmpdir = get_tempdir();
    if (!tmpdir) {
        return NULL;
    }

    // Mutex for random string.
    static Lock rand_lock;  // lock not destroyed.
    static atomic_int initialized = 0;

    if (!atomic_load(&initialized)) {
        lock_init(&rand_lock);
        srand((unsigned int)time(NULL));
        atomic_store(&initialized, 1);
    }

    // Generate a random pattern for the temporary file
    char pattern[TEMP_PREF_PREFIX_LEN + 7] = {0};  // +7 for XXXXXX and null terminator

    lock_acquire(&rand_lock);
    random_string(pattern, sizeof(pattern) - 1);
    lock_release(&rand_lock);

    // overwrite the last 6 characters with XXXXXX
    safe_strlcpy(pattern + TEMP_PREF_PREFIX_LEN, "XXXXXX", 7);

    // join the path
    char* tmpfile = filepath_join(tmpdir, pattern);
    if (!tmpfile) {
        free(tmpdir);
        return NULL;
    }
#ifdef _WIN32
    wchar_t wtmpfile[FILENAME_MAX];
    mbstowcs(wtmpfile, tmpfile, FILENAME_MAX);
    if (errno == EILSEQ) {
        fprintf(stderr, "make_tempfile: Invalid multibyte sequence.\n");
        free(tmpfile);
        free(tmpdir);
        return NULL;
    }

    // Create temporary file, returns handle
    int fd = _wcreat(wtmpfile, S_IREAD | S_IWRITE);
    if (fd == -1) {
        perror("make_tempfile: _wcreat");
        free(tmpfile);
        free(tmpdir);
        return NULL;
    }
    _close(fd);  // close the file after created.

#else
    int fd = mkstemp(tmpfile);
    if (fd == -1) {
        perror("make_tempfile: mkstemp");
        free(tmpfile);
        free(tmpdir);
        return NULL;
    }
    close(fd);
#endif
    free(tmpdir);
    return tmpfile;
}

// Make a temporary directory
// Returns a pointer to the temporary directory or NULL on error
// The caller is responsible for freeing the memory
char* make_tempdir(void) {
    char* tmpdir = get_tempdir();
    if (!tmpdir) {
        return NULL;
    }

    // Generate a random pattern for the temporary file
    char pattern[TEMP_PREF_PREFIX_LEN + 7] = {0};  // +7 for XXXXXX and null terminator

    random_string(pattern, sizeof(pattern) - 1);

    // overwrite the last 6 characters with XXXXXX
    safe_strlcpy(pattern + TEMP_PREF_PREFIX_LEN, "XXXXXX", 7);

    char* tmp = filepath_join(tmpdir, pattern);
    if (!tmp) {
        free(tmpdir);
        return NULL;
    }

#ifdef _WIN32
    wchar_t wtmp[FILENAME_MAX];
    mbstowcs(wtmp, tmp, FILENAME_MAX);
    if (errno == EILSEQ) {
        fprintf(stderr, "make_tempdir: Invalid multibyte sequence.\n");
        free(tmp);
        free(tmpdir);
        return NULL;
    }
    if (_wmkdir(wtmp) != 0) {
        perror("make_tempdir: _wmkdir");
        free(tmp);
        free(tmpdir);
        return NULL;
    }
#else
    if (mkdtemp(tmp) == NULL) {
        perror("make_tempdir: mkdtemp");
        free(tmp);
        free(tmpdir);
        return NULL;
    }
#endif
    free(tmpdir);
    return tmp;
}

bool path_exists(const char* path) {
    if (!path) {
        fprintf(stderr, "path_exists: Path cannot be NULL.\n");
        return false;
    }

#ifdef _WIN32
    wchar_t wpath[FILENAME_MAX];
    mbstowcs(wpath, path, FILENAME_MAX);
    if (errno == EILSEQ) {
        fprintf(stderr, "path_exists: Invalid multibyte sequence.\n");
        return false;
    }

    DWORD attr = GetFileAttributesW(wpath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return true;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return true;
#endif
}

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
        size_t len = (size_t)(base - path);
        if (len >= size) {
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
    char base[FILENAME_MAX] = {0};
    filepath_basename(path, base, FILENAME_MAX);
    char* dot = strrchr(base, '.');
    if (!dot) {
        strncpy(name, base, size);
    } else {
        size_t len = (size_t)(dot - base);
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
char* get_cwd(void) {
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

int filepath_remove(const char* path) {
    int ret = -1;
#ifdef _WIN
    ret = _unlink(path);
#else
    ret = unlink(path);
#endif
    return ret;
}

// Rename oldpath to newpath. Returns 0 if successful, -1 otherwise
int filepath_rename(const char* oldpath, const char* newpath) {
    return rename(oldpath, newpath);
}

const char* user_home_dir(void) {
#ifdef _WIN32
    return getenv("USERPROFILE");
#else
    return secure_getenv("HOME");
#endif
}

// expand user home directory
char* filepath_expanduser(const char* path) {
    // If the path is not a home directory path, return it as is
    if (path[0] != '~') {
        return strdup(path);
    }

#ifdef _WIN32
    char* home = getenv("USERPROFILE");
#else
    char* home = (char*)secure_getenv("HOME");
#endif
    if (!home) {
        fprintf(stderr, "USERPROFILE/HOME environment variable not set\n");
        return NULL;
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
    // if there is a backslash in the path1, then use backslash as separator
    // otherwise use forward slash
    if (strchr(path, '\\')) {
        _snprintf(expanded, len, "%s\\%s", home, path);
    } else {
        _snprintf(expanded, len, "%s/%s", home, path);
    }
#else
    snprintf(expanded, len, "%s/%s", home, path);
#endif
    return expanded;
}

bool filepath_expanduser_buf(const char* path, char* expanded, size_t len) {
#ifdef _WIN32
    char* home = getenv("USERPROFILE");
#else
    char* home = (char*)secure_getenv("HOME");
#endif
    if (!home) {
        fprintf(stderr, "USERPROFILE/HOME environment variable not set\n");
        return false;
    }

    // If the path is not a home directory path, return it as is
    if (path[0] != '~') {
        strncpy(expanded, path, len);
        expanded[len - 1] = '\0';
        return true;
    }

    // If path is just "~" or ~/, return the home directory
    size_t pathLen = strlen(path);
    bool isHome    = pathLen == 1 || (pathLen == 2 && path[1] == '/');
    if (isHome) {
        strncpy(expanded, home, len);
        expanded[len - 1] = '\0';
        return true;
    }

    // 1 for the separator and 1 for the null terminator
    size_t homelen = strlen(home);
    if (homelen + pathLen + 2 > len) {
        fprintf(stderr, "Buffer is too small to store the expanded path\n");
        return false;
    }
#ifdef _WIN32
    // if there is a backslash in the path1, then use backslash as separator
    // otherwise use forward slash
    if (strchr(path, '\\')) {
        _snprintf(expanded, len, "%s\\%s", home, path);
    } else {
        _snprintf(expanded, len, "%s/%s", home, path);
    }
#else
    snprintf(expanded, len, "%s/%s", home, path);
#endif
    return true;
}

// join path
char* filepath_join(const char* path1, const char* path2) {
    size_t len   = strlen(path1) + strlen(path2) + 2;  // 1 for the separator and 1 for the null terminator
    char* joined = (char*)malloc(len);
    if (!joined) {
        perror("malloc");
        return NULL;
    }
#ifdef _WIN32
    // if there is a backslash in the path1, then use backslash as separator
    // otherwise use forward slash
    if (strchr(path1, '\\')) {
        _snprintf(joined, len, "%s\\%s", path1, path2);
    } else {
        _snprintf(joined, len, "%s/%s", path1, path2);
    }
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
        fprintf(stderr, "Buffer is too small to store the joined path\n");
        return false;
    }
#ifdef _WIN32
    // if there is a backslash in the path1, then use backslash as separator
    // otherwise use forward slash
    if (strchr(path1, '\\')) {
        _snprintf(abspath, len, "%s\\%s", path1, path2);
    } else {
        _snprintf(abspath, len, "%s/%s", path1, path2);
    }
#else
    snprintf(abspath, len, "%s/%s", path1, path2);
#endif
    return true;
}

/*
 * Split a file path into directory and file name.
 * The dir and name parameters must be pre-allocated buffers or pointers to
 * pre-allocated buffers. dir_size and name_size are the size of the dir and
 * name buffers.
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
        size_t len = (size_t)(p - path);
        if (len >= dir_size) {
            len = dir_size - 1;
        }
        strncpy(dir, path, len);
        dir[len] = '\0';
        strncpy(name, p + 1, name_size);
        name[name_size - 1] = '\0';  // Ensure null-termination
    }
}
