/**
 * @file filepath_win32.c
 * @brief Win32 implementation of the filepath backends.
 *
 * Contains all Windows-specific filesystem primitives: FindFirstFile-based
 * directory access, WIN32_FIND_DATAW attribute mapping, and UTF-16 <-> UTF-8
 * conversion at the API boundary.
 *
 * NOTE: This file is only compiled for _WIN32 targets and cannot be built on
 * POSIX machines; keep it in sync with filepath_internal.h.
 */

#include "filepath_internal.h"

#include "../../include/wintypes.h"

#include <errno.h>
#include <io.h>  // for _access, _wcreat
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <windows.h>

#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif

// Windows doesn't have R_OK, W_OK, X_OK
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 1
#endif

/* ---------------------------------------------------------------------------
 * Entropy
 * ------------------------------------------------------------------------- */

bool fp_random_bytes(unsigned char* buf, size_t n) {
    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContextW(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return false;
    }

    if (!CryptGenRandom(hCryptProv, (DWORD)n, buf)) {
        CryptReleaseContext(hCryptProv, 0);
        return false;
    }
    CryptReleaseContext(hCryptProv, 0);
    return true;
}

/* ---------------------------------------------------------------------------
 * Directory handles
 * ------------------------------------------------------------------------- */

// Open a directory
Directory* dir_open(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return NULL;
    }

    Directory* dir = (Directory*)calloc(1, sizeof(Directory));
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
    return dir;
}

// Close a directory
void dir_close(Directory* dir) {
    if (!dir) return;

    if (dir->handle != INVALID_HANDLE_VALUE) {
        FindClose(dir->handle);
    }
    free(dir->path);
    free(dir);
}

// Read the next entry in the directory
char* dir_next(Directory* dir) {
    if (!dir) {
        errno = EINVAL;
        return NULL;
    }

    if (dir->handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return NULL;
    }
    if (FindNextFileW(dir->handle, &dir->find_data)) {
        // Convert wide-char filename to UTF-8 directly into the struct buffer
        WideCharToMultiByte(CP_UTF8, 0, dir->find_data.cFileName, -1, dir->name_buf, MAX_PATH, NULL, NULL);
        return dir->name_buf;
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Attribute mapping
 * ------------------------------------------------------------------------- */

static void map_win32_attrs(const WIN32_FIND_DATAW* fd, FileAttributes* attr) {
    attr->attrs = FATTR_NONE;
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        attr->attrs |= FATTR_DIR;
    else
        attr->attrs |= FATTR_FILE;
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) attr->attrs |= FATTR_SYMLINK;
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) attr->attrs |= FATTR_HIDDEN;
    if (fd->cFileName[0] == '.') attr->attrs |= FATTR_HIDDEN;

    attr->size = ((size_t)fd->nFileSizeHigh << 32) | fd->nFileSizeLow;

    // Convert Windows FileTime to Unix mtime (simplified)
    ULARGE_INTEGER ull;
    ull.LowPart = fd->ftLastWriteTime.dwLowDateTime;
    ull.HighPart = fd->ftLastWriteTime.dwHighDateTime;
    attr->mtime = (time_t)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

const FileAttributes* lazy_get_attrs(LazyFileAttributes* lazy) {
    if (!lazy) {
        errno = EINVAL;
        return NULL;
    }
    // The Win32 traversal backends always populate cached attributes eagerly,
    // so has_stat is true whenever a LazyFileAttributes is handed out.
    if (!lazy->has_stat) {
        errno = EINVAL;
        return NULL;
    }
    return &lazy->cached;
}

/* ---------------------------------------------------------------------------
 * Path queries and simple operations
 * ------------------------------------------------------------------------- */

// Check if path is a directory
bool is_dir(const char* path) {
    if (!path || *path == '\0') {
        return false;
    }

    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Check if path is a file
bool is_file(const char* path) {
    if (!path || *path == '\0') {
        return false;
    }

    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// Check if path is a symbolic link
bool is_symlink(const char* path) {
    if (!path || *path == '\0') {
        return false;
    }

    // Windows supports symbolic links since Vista, but we keep original behavior
    return false;
}

// Check if path exists
bool path_exists(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return false;
    }

    wchar_t wpath[MAX_PATH];
    if (mbstowcs(wpath, path, MAX_PATH) == (size_t)-1) {
        errno = EINVAL;
        return false;
    }

    DWORD attr = GetFileAttributesW(wpath);
    return attr != INVALID_FILE_ATTRIBUTES;
}

// Create a directory
int dir_create(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return -1;
    }

    if (!CreateDirectoryA(path, NULL)) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            return 0;
        }
        errno = GetLastError() == ERROR_ACCESS_DENIED ? EACCES : EIO;
        return -1;
    }
    return 0;
}

int fp_remove_single_directory(const char* path) {
    if (!RemoveDirectoryA(path)) {
        DWORD err = GetLastError();
        errno = (err == ERROR_DIR_NOT_EMPTY)                                   ? ENOTEMPTY
                : (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND) ? ENOENT
                                                                               : EACCES;
        return -1;
    }
    return 0;
}

// Get temporary directory
char* get_tempdir(void) {
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
}

char* fp_create_tempfile(char* candidate) {
    wchar_t wtmpfile[MAX_PATH];
    if (mbstowcs(wtmpfile, candidate, MAX_PATH) == (size_t)-1) {
        free(candidate);
        errno = EINVAL;
        return NULL;
    }

    int fd = _wcreat(wtmpfile, _S_IREAD | _S_IWRITE);
    if (fd == -1) {
        free(candidate);
        return NULL;
    }
    _close(fd);
    return candidate;
}

char* fp_create_tempdir(char* candidate) {
    wchar_t wtmp[MAX_PATH];
    if (mbstowcs(wtmp, candidate, MAX_PATH) == (size_t)-1) {
        free(candidate);
        errno = EINVAL;
        return NULL;
    }
    if (_wmkdir(wtmp) != 0) {
        free(candidate);
        return NULL;
    }
    return candidate;
}

// Get absolute path
char* filepath_absolute(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return NULL;
    }

    char* abs = _fullpath(NULL, path, 0);
    if (!abs) {
        errno = ENOMEM;
        return NULL;
    }
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

    return _unlink(path);
}

// Get user home directory
const char* user_home_dir(void) { return GETENV("USERPROFILE"); }

/**
 * Callback function for removing files and directories during traversal.
 * Should be used with fp_dir_walk_depth_first_impl for proper deletion order.
 */
WalkDirOption fp_dir_remove_entry(const FileAttributes* attr, const char* path, const char* name, void* data) {
    (void)data;
    (void)name;

    if (!attr || !path) {
        errno = EINVAL;
        return DirError;
    }

    if (fattr_is_dir(attr)) {
        if (!RemoveDirectoryA(path)) {
            DWORD err = GetLastError();
            errno = (err == ERROR_DIR_NOT_EMPTY)                                   ? ENOTEMPTY
                    : (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND) ? ENOENT
                                                                                   : EACCES;
            return DirError;
        }
    } else {
        if (!DeleteFileA(path)) {
            DWORD err = GetLastError();
            errno = (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND) ? ENOENT
                    : (err == ERROR_ACCESS_DENIED)                               ? EACCES
                                                                                 : EIO;
            return DirError;
        }
    }
    return DirContinue;
}

/* ---------------------------------------------------------------------------
 * Traversal
 * ------------------------------------------------------------------------- */

static int dir_walk_win32(const char* path, WalkDirCallback callback, void* data, int depth) {
    if (depth > FP_MAX_DIR_DEPTH) {
        errno = ELOOP;
        return -1;
    }

    Directory* dir = dir_open(path);
    if (!dir) return -1;

    char fullpath[FILENAME_MAX];
    int status = 0;

    do {
        const wchar_t* wname = dir->find_data.cFileName;
        if (wcscmp(wname, L".") == 0 || wcscmp(wname, L"..") == 0) continue;

        char name[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, MAX_PATH, NULL, NULL);

        if (!filepath_join_buf(path, name, fullpath, sizeof(fullpath))) {
            status = -1;
            break;
        }

        FileAttributes attr;
        map_win32_attrs(&dir->find_data, &attr);

        WalkDirOption opt = callback(&attr, fullpath, name, data);
        if (opt == DirStop) break;
        if (opt == DirError) {
            status = -1;
            break;
        }
        if (opt == DirSkip) continue;

        if (fattr_is_dir(&attr)) {
            if (dir_walk_win32(fullpath, callback, data, depth + 1) != 0) {
                status = -1;
                break;
            }
        }
    } while (FindNextFileW(dir->handle, &dir->find_data));

    dir_close(dir);
    return status;
}

/** Depth-checked helper for depth-first (post-order) walking. */
static int dir_walk_depth_first_win32(const char* path, WalkDirCallback callback, void* data, int depth) {
    if (depth > FP_MAX_DIR_DEPTH) {
        errno = ELOOP;
        return -1;
    }

    Directory* dir = dir_open(path);
    if (!dir) return -1;

    char fullpath[FILENAME_MAX];
    int status = 0;

    do {
        const wchar_t* wname = dir->find_data.cFileName;
        if (wcscmp(wname, L".") == 0 || wcscmp(wname, L"..") == 0) continue;

        char name[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, MAX_PATH, NULL, NULL);
        filepath_join_buf(path, name, fullpath, sizeof(fullpath));

        FileAttributes attr;
        map_win32_attrs(&dir->find_data, &attr);

        if (fattr_is_dir(&attr)) {
            if (dir_walk_depth_first_win32(fullpath, callback, data, depth + 1) != 0) {
                status = -1;
                break;
            }
        }

        WalkDirOption opt = callback(&attr, fullpath, name, data);
        if (opt == DirStop) break;
        if (opt == DirError) {
            status = -1;
            break;
        }
    } while (FindNextFileW(dir->handle, &dir->find_data));

    dir_close(dir);
    return status;
}

/** Lazy-walk fallback that eagerly populates LazyFileAttributes from find data. */
static int dir_walkx_win32(const char* path, WalkDirCallbackX callback, void* data) {
    Directory* dir = dir_open(path);
    if (!dir) return -1;

    char fullpath[FILENAME_MAX];
    int status = 0;

    do {
        const wchar_t* wname = dir->find_data.cFileName;
        if (wcscmp(wname, L".") == 0 || wcscmp(wname, L"..") == 0) continue;
        char name[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, MAX_PATH, NULL, NULL);
        if (!filepath_join_buf(path, name, fullpath, sizeof(fullpath))) {
            status = -1;
            break;
        }
        // Create a fake lazy struct with has_stat = true since Win32 find data
        // already carries all attributes.
        FileAttributes attr;
        map_win32_attrs(&dir->find_data, &attr);
        LazyFileAttributes lazy = {
            .dirfd = -1,
            .name = name,
            .d_type = DT_UNKNOWN,
            .cached = attr,
            .has_stat = true,
            .is_dir_cached = true,
            .is_dir_value = fattr_is_dir(&attr),
        };
        WalkDirOption opt = callback(&lazy, fullpath, name, data);
        if (opt == DirStop) break;
        if (opt == DirError) {
            status = -1;
            break;
        }
        if (opt == DirSkip) continue;
        if (fattr_is_dir(&attr)) {
            if (fp_dir_walkx_impl(fullpath, callback, data) != 0) {
                status = -1;
                break;
            }
        }
    } while (FindNextFileW(dir->handle, &dir->find_data));

    dir_close(dir);
    return status;
}

int fp_dir_walk_impl(const char* path, WalkDirCallback callback, void* data) {
    return dir_walk_win32(path, callback, data, 0);
}

int fp_dir_walk_depth_first_impl(const char* path, WalkDirCallback callback, void* data) {
    return dir_walk_depth_first_win32(path, callback, data, 0);
}

int fp_dir_walkx_impl(const char* path, WalkDirCallbackX callback, void* data) {
    return dir_walkx_win32(path, callback, data);
}
