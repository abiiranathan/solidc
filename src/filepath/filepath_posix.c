/**
 * @file filepath_posix.c
 * @brief POSIX (Linux/macOS/BSD) implementation of the filepath backends.
 *
 * Contains all POSIX-specific filesystem primitives: opendir/readdir based
 * directory access, stat-based attribute mapping, and — on Linux — an
 * optimized getdents64 + openat + fstatat fast path for directory walks.
 */

#include "filepath_internal.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../include/lock.h"

#ifdef __linux__
#include <sys/syscall.h>
#include <sys/utsname.h>

// Linux getdents64 structures
#ifndef SYS_getdents64
#define SYS_getdents64 217
#endif

struct linux_dirent64 {
    ino64_t d_ino;
    off64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

// Kernel version check for fast path
static int kernel_version_ge(int want_major, int want_minor) {
    static int cached_result = -1;
    struct utsname u;
    int major = 0, minor = 0;

    if (cached_result != -1) {
        return cached_result;
    }

    if (uname(&u) != 0) {
        cached_result = 0;
        return 0;
    }

    sscanf(u.release, "%d.%d", &major, &minor);

    if (major > want_major) {
        cached_result = 1;
    } else if (major == want_major && minor >= want_minor) {
        cached_result = 1;
    } else {
        cached_result = 0;
    }
    return cached_result;
}

static inline int has_fast_dirent(void) {
    // getdents64 + openat + fstatat available since 2.6.16, d_type reliable since 2.6
    // statx available since 4.11
    return kernel_version_ge(2, 6);
}

static inline int has_statx(void) { return kernel_version_ge(4, 11); }
#endif /* __linux__ */

/* ---------------------------------------------------------------------------
 * Entropy
 * ------------------------------------------------------------------------- */

bool fp_random_bytes(unsigned char* buf, size_t n) {
#ifdef __linux__
    /* Kernel-provided, thread-safe, no descriptor churn. */
    ssize_t bytes_read = getrandom(buf, n, 0);
    if (bytes_read != (ssize_t)n) {
        return false;
    }
#else
    /*
     * Thread-safety: /dev/urandom needs per-call open/close; guard it with a
     * lock whose initialiser uses an atomic exchange so exactly one thread
     * ever runs lock_init.
     */
    static Lock rand_lock;
    static atomic_int initialized = 0;

    if (!atomic_exchange(&initialized, 1)) {
        lock_init(&rand_lock);
    }

    lock_acquire(&rand_lock);
    ssize_t bytes_read = -1;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd != -1) {
        bytes_read = read(fd, buf, n);
        close(fd);
    }
    if (bytes_read != (ssize_t)n) {
        lock_release(&rand_lock);
        return false;
    }
    lock_release(&rand_lock);
#endif
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

    dir->dir = opendir(path);
    if (!dir->dir) {
        int saved_errno = errno;
        free(dir->path);
        free(dir);
        errno = saved_errno;
        return NULL;
    }
    return dir;
}

// Close a directory
void dir_close(Directory* dir) {
    if (!dir) return;

    if (dir->dir) {
        closedir(dir->dir);
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

    struct dirent* entry = readdir(dir->dir);
    if (entry) {
        return entry->d_name;
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Attribute mapping
 * ------------------------------------------------------------------------- */

static int map_dirent_attrs(const struct dirent* entry, const char* path, FileAttributes* attr) {
    struct stat st;
    if (lstat(path, &st) != 0) return -1;

    attr->size = (size_t)st.st_size;
    attr->mtime = st.st_mtime;
    attr->attrs = FATTR_NONE;

    // Check for hidden file based on name
    if (entry->d_name[0] == '.') {
        attr->attrs |= FATTR_HIDDEN;
    }

    // Use standard POSIX macros on st_mode instead of non-standard DT_ constants
    if (S_ISREG(st.st_mode)) {
        attr->attrs |= FATTR_FILE;
        // Optional: Check executable bits here if needed
        if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
            attr->attrs |= FATTR_EXECUTABLE;
        }
    } else if (S_ISDIR(st.st_mode)) {
        attr->attrs |= FATTR_DIR;
        attr->size = 0;  // Directory size is not meaningful
    } else if (S_ISLNK(st.st_mode)) {
        attr->attrs |= FATTR_SYMLINK;
    }
#ifdef S_ISCHR
    else if (S_ISCHR(st.st_mode)) {
        attr->attrs |= FATTR_CHARDEV;
    }
#endif
#ifdef S_ISBLK
    else if (S_ISBLK(st.st_mode)) {
        attr->attrs |= FATTR_BLOCKDEV;
    }
#endif
#ifdef S_ISFIFO
    else if (S_ISFIFO(st.st_mode)) {
        attr->attrs |= FATTR_FIFO;
    }
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(st.st_mode)) {
        attr->attrs |= FATTR_SOCKET;
    }
#endif
    return 0;
}

const FileAttributes* lazy_get_attrs(LazyFileAttributes* lazy) {
    if (!lazy) {
        errno = EINVAL;
        return NULL;
    }
    if (lazy->has_stat) {
        return &lazy->cached;
    }
#ifdef __linux__
    struct stat st;
    if (fstatat(lazy->dirfd, lazy->name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        return NULL;
    }
    lazy->cached.size = (size_t)st.st_size;
    lazy->cached.mtime = st.st_mtime;
    lazy->cached.attrs = FATTR_NONE;
    if (lazy->name[0] == '.') {
        lazy->cached.attrs |= FATTR_HIDDEN;
    }
    if (S_ISREG(st.st_mode)) {
        lazy->cached.attrs |= FATTR_FILE;
        if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
            lazy->cached.attrs |= FATTR_EXECUTABLE;
        }
    } else if (S_ISDIR(st.st_mode)) {
        lazy->cached.attrs |= FATTR_DIR;
        lazy->cached.size = 0;
    } else if (S_ISLNK(st.st_mode)) {
        lazy->cached.attrs |= FATTR_SYMLINK;
    }
#ifdef S_ISCHR
    else if (S_ISCHR(st.st_mode)) {
        lazy->cached.attrs |= FATTR_CHARDEV;
    }
#endif
#ifdef S_ISBLK
    else if (S_ISBLK(st.st_mode)) {
        lazy->cached.attrs |= FATTR_BLOCKDEV;
    }
#endif
#ifdef S_ISFIFO
    else if (S_ISFIFO(st.st_mode)) {
        lazy->cached.attrs |= FATTR_FIFO;
    }
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(st.st_mode)) {
        lazy->cached.attrs |= FATTR_SOCKET;
    }
#endif
#else
    // Fallback for non-Linux POSIX: use lstat on the full fd-relative path is
    // not portable without fstatat, so callers on other platforms always pass
    // has_stat = true. Guard anyway in case of direct use.
    struct stat st;
    if (fstatat(lazy->dirfd, lazy->name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        return NULL;
    }
    lazy->cached.size = (size_t)st.st_size;
    lazy->cached.mtime = st.st_mtime;
    lazy->cached.attrs = FATTR_NONE;
    if (lazy->name[0] == '.') lazy->cached.attrs |= FATTR_HIDDEN;
    if (S_ISREG(st.st_mode)) {
        lazy->cached.attrs |= FATTR_FILE;
        if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) lazy->cached.attrs |= FATTR_EXECUTABLE;
    } else if (S_ISDIR(st.st_mode)) {
        lazy->cached.attrs |= FATTR_DIR;
        lazy->cached.size = 0;
    } else if (S_ISLNK(st.st_mode)) {
        lazy->cached.attrs |= FATTR_SYMLINK;
    }
#endif
    lazy->has_stat = true;
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

    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

// Check if path is a file
bool is_file(const char* path) {
    if (!path || *path == '\0') {
        return false;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}

// Check if path is a symbolic link
bool is_symlink(const char* path) {
    if (!path || *path == '\0') {
        return false;
    }

    struct stat st;
    if (lstat(path, &st) != 0) {
        return false;
    }
    return S_ISLNK(st.st_mode);
}

// Check if path exists
bool path_exists(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return false;
    }

    struct stat st;
    return stat(path, &st) == 0;
}

// Create a directory
int dir_create(const char* path) {
    if (!path || *path == '\0') {
        errno = EINVAL;
        return -1;
    }

    if (mkdir(path, 0755) == -1) {
        if (errno == EEXIST) {
            return 0;
        }
        return -1;
    }
    return 0;
}

int fp_remove_single_directory(const char* path) {
    if (rmdir(path) == -1) {
        return -1;
    }
    return 0;
}

// Get temporary directory
char* get_tempdir(void) {
    const char* temp = GETENV("TMPDIR");
    if (!temp) {
        temp = "/tmp";
    }
    char* result = strdup(temp);
    if (!result) {
        errno = ENOMEM;
        return NULL;
    }
    return result;
}

char* fp_create_tempfile(char* candidate) {
    int fd = mkstemp(candidate);
    if (fd == -1) {
        free(candidate);
        return NULL;
    }
    close(fd);
    return candidate;
}

char* fp_create_tempdir(char* candidate) {
    if (mkdtemp(candidate) == NULL) {
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

    char* abs = realpath(path, NULL);
    if (!abs) {
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

    return unlink(path);
}

// Get user home directory
const char* user_home_dir(void) { return GETENV("HOME"); }

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
        if (rmdir(path) != 0) {
            return DirError;  // errno already set by rmdir
        }
    } else {
        if (unlink(path) != 0) {
            return DirError;  // errno already set by unlink
        }
    }
    return DirContinue;
}

/* ---------------------------------------------------------------------------
 * Linux fast-path traversal (getdents64 + fstatat + openat)
 *
 * This avoids path string construction for stat and uses the kernel's
 * d_type where possible.
 * ------------------------------------------------------------------------- */

#ifdef __linux__

// Fast attribute mapping using fstatat (fd-relative, avoids full path walk)
static int fast_map_attrs(int dirfd, const char* name, unsigned char d_type, FileAttributes* attr) {
    struct stat st;
    // For type determination, d_type is often sufficient and avoids stat
    // But we still need size/mtime for FileAttributes, so we need stat
    // However, for directories, size is always 0, so we could avoid stat
    // if we only need is_dir. For now, use fstatat which is faster than
    // lstat with full path.
    (void)d_type;

    if (fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        return -1;
    }

    attr->size = (size_t)st.st_size;
    attr->mtime = st.st_mtime;
    attr->attrs = FATTR_NONE;

    if (name[0] == '.') {
        attr->attrs |= FATTR_HIDDEN;
    }

    // Use stat result for definitive type (more reliable than d_type)
    if (S_ISREG(st.st_mode)) {
        attr->attrs |= FATTR_FILE;
        if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
            attr->attrs |= FATTR_EXECUTABLE;
        }
    } else if (S_ISDIR(st.st_mode)) {
        attr->attrs |= FATTR_DIR;
        attr->size = 0;  // Directory size is not meaningful
    } else if (S_ISLNK(st.st_mode)) {
        attr->attrs |= FATTR_SYMLINK;
    }
#ifdef S_ISCHR
    else if (S_ISCHR(st.st_mode)) {
        attr->attrs |= FATTR_CHARDEV;
    }
#endif
#ifdef S_ISBLK
    else if (S_ISBLK(st.st_mode)) {
        attr->attrs |= FATTR_BLOCKDEV;
    }
#endif
#ifdef S_ISFIFO
    else if (S_ISFIFO(st.st_mode)) {
        attr->attrs |= FATTR_FIFO;
    }
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(st.st_mode)) {
        attr->attrs |= FATTR_SOCKET;
    }
#endif
    return 0;
}

static inline bool fast_is_dot_entry(const char* name) {
    return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

// Optimized dir_walk helper using fd-based traversal and getdents64
static int dir_walk_fast_helper(const char* path, int dirfd, WalkDirCallback callback, void* data, int depth) {
    if (depth > FP_MAX_DIR_DEPTH) {
        errno = ELOOP;
        return -1;
    }

    // Use getdents64 directly with large buffer (64KB for fewer syscalls)
    char buf[64 * 1024];
    size_t path_len = strlen(path);

    int status = 0;
    char fullpath[FILENAME_MAX];

    while (1) {
        long nread = syscall(SYS_getdents64, dirfd, buf, sizeof(buf));
        if (nread == -1) {
            return -1;
        }
        if (nread == 0) break;

        for (long bpos = 0; bpos < nread;) {
            struct linux_dirent64* d = (struct linux_dirent64*)(buf + bpos);

            if (!fast_is_dot_entry(d->d_name)) {
                size_t name_len = strlen(d->d_name);
                if (path_len + 1 + name_len + 1 > sizeof(fullpath)) {
                    status = -1;
                    break;
                }
                memcpy(fullpath, path, path_len);
                fullpath[path_len] = '/';
                memcpy(fullpath + path_len + 1, d->d_name, name_len + 1);

                FileAttributes attr;
                if (fast_map_attrs(dirfd, d->d_name, d->d_type, &attr) != 0) {
                    // Skip unreadable entries
                    bpos += d->d_reclen;
                    continue;
                }

                WalkDirOption opt = callback(&attr, fullpath, d->d_name, data);
                if (opt == DirStop) {
                    return 0;
                }
                if (opt == DirError) {
                    return -1;
                }
                if (opt == DirSkip) {
                    bpos += d->d_reclen;
                    continue;
                }

                if (fattr_is_dir(&attr)) {
                    int child_fd = openat(dirfd, d->d_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                    if (child_fd >= 0) {
                        if (dir_walk_fast_helper(fullpath, child_fd, callback, data, depth + 1) != 0) {
                            close(child_fd);
                            return -1;
                        }
                        close(child_fd);
                    } else if (errno != EACCES && errno != ENOENT) {
                        // Only fail on unexpected errors
                        if (errno == ELOOP) return -1;
                    }
                }
            }
            bpos += d->d_reclen;
        }
        if (status != 0) break;
    }
    return status;
}

static int dir_walk_depth_first_fast_helper(const char* path, int dirfd, WalkDirCallback callback, void* data,
                                            int depth) {
    if (depth > FP_MAX_DIR_DEPTH) {
        errno = ELOOP;
        return -1;
    }

    char buf[64 * 1024];
    size_t path_len = strlen(path);

    int status = 0;
    char fullpath[FILENAME_MAX];

    while (1) {
        long nread = syscall(SYS_getdents64, dirfd, buf, sizeof(buf));
        if (nread == -1) return -1;
        if (nread == 0) break;

        for (long bpos = 0; bpos < nread;) {
            struct linux_dirent64* d = (struct linux_dirent64*)(buf + bpos);

            if (!fast_is_dot_entry(d->d_name)) {
                size_t name_len = strlen(d->d_name);
                if (path_len + 1 + name_len + 1 > sizeof(fullpath)) {
                    status = -1;
                    break;
                }
                memcpy(fullpath, path, path_len);
                fullpath[path_len] = '/';
                memcpy(fullpath + path_len + 1, d->d_name, name_len + 1);

                FileAttributes attr;
                if (fast_map_attrs(dirfd, d->d_name, d->d_type, &attr) != 0) {
                    bpos += d->d_reclen;
                    continue;
                }

                if (fattr_is_dir(&attr)) {
                    int child_fd = openat(dirfd, d->d_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                    if (child_fd >= 0) {
                        if (dir_walk_depth_first_fast_helper(fullpath, child_fd, callback, data, depth + 1) != 0) {
                            close(child_fd);
                            status = -1;
                            break;
                        }
                        close(child_fd);
                        // Re-stat after recursion as directory may have been modified
                        if (fast_map_attrs(dirfd, d->d_name, d->d_type, &attr) != 0) {
                            bpos += d->d_reclen;
                            continue;
                        }
                    }
                }

                WalkDirOption opt = callback(&attr, fullpath, d->d_name, data);
                if (opt == DirStop) return 0;
                if (opt == DirError) return -1;
            }
            bpos += d->d_reclen;
        }
        if (status != 0) break;
    }
    return status;
}

static int dir_walkx_fast_helper(const char* path, int dirfd, WalkDirCallbackX callback, void* data, int depth) {
    if (depth > FP_MAX_DIR_DEPTH) {
        errno = ELOOP;
        return -1;
    }

    char buf[64 * 1024];
    size_t path_len = strlen(path);
    char fullpath[FILENAME_MAX];

    while (1) {
        long nread = syscall(SYS_getdents64, dirfd, buf, sizeof(buf));
        if (nread == -1) return -1;
        if (nread == 0) break;

        for (long bpos = 0; bpos < nread;) {
            struct linux_dirent64* d = (struct linux_dirent64*)(buf + bpos);

            if (!fast_is_dot_entry(d->d_name)) {
                size_t name_len = strlen(d->d_name);
                if (path_len + 1 + name_len + 1 > sizeof(fullpath)) {
                    bpos += d->d_reclen;
                    continue;
                }
                memcpy(fullpath, path, path_len);
                fullpath[path_len] = '/';
                memcpy(fullpath + path_len + 1, d->d_name, name_len + 1);

                LazyFileAttributes lazy = {
                    .dirfd = dirfd,
                    .name = d->d_name,
                    .d_type = d->d_type,
                    .has_stat = false,
                    .is_dir_cached = false,
                };
                if (d->d_name[0] == '.') {
                    lazy.cached.attrs = FATTR_HIDDEN;
                }

                WalkDirOption opt = callback(&lazy, fullpath, d->d_name, data);
                if (opt == DirStop) return 0;
                if (opt == DirError) return -1;
                if (opt == DirSkip) {
                    bpos += d->d_reclen;
                    continue;
                }

                if (lazy_is_dir(&lazy)) {
                    int child_fd = openat(dirfd, d->d_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                    if (child_fd >= 0) {
                        if (dir_walkx_fast_helper(fullpath, child_fd, callback, data, depth + 1) != 0) {
                            close(child_fd);
                            return -1;
                        }
                        close(child_fd);
                    }
                }
            }
            bpos += d->d_reclen;
        }
    }
    return 0;
}

#endif /* __linux__ */

/* ---------------------------------------------------------------------------
 * Generic (readdir-based) traversal and dispatchers
 * ------------------------------------------------------------------------- */

/**
 * @brief Generic readdir-based walk used when the Linux fast path is
 *        unavailable or cannot open the directory.
 */
static int dir_walk_generic(const char* path, WalkDirCallback callback, void* data, int depth) {
    if (depth > FP_MAX_DIR_DEPTH) {
        errno = ELOOP;  // Symbolic link loop or too many levels of directories
        return -1;
    }

    Directory* dir = dir_open(path);
    if (!dir) return -1;

    char fullpath[FILENAME_MAX];
    int status = 0;

    struct dirent* entry;
    while ((entry = readdir(dir->dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        if (!filepath_join_buf(path, entry->d_name, fullpath, sizeof(fullpath))) {
            status = -1;
            break;
        }

        FileAttributes attr;
        if (map_dirent_attrs(entry, fullpath, &attr) != 0) {
            continue;
        }

        if (attr.attrs == FATTR_NONE) {
            if (populate_file_attrs(fullpath, &attr) != 0) continue;
        }

        WalkDirOption opt = callback(&attr, fullpath, entry->d_name, data);
        if (opt == DirStop) break;
        if (opt == DirError) {
            status = -1;
            break;
        }
        if (opt == DirSkip) continue;

        if (fattr_is_dir(&attr)) {
            if (fp_dir_walk_impl(fullpath, callback, data) != 0) {
                status = -1;
                break;
            }
        }
    }

    dir_close(dir);
    return status;
}

/**
 * @brief Generic readdir-based depth-first (post-order) walk.
 */
static int dir_walk_depth_first_generic(const char* path, WalkDirCallback callback, void* data, int depth) {
    if (depth > FP_MAX_DIR_DEPTH) {
        errno = ELOOP;
        return -1;
    }

    Directory* dir = dir_open(path);
    if (!dir) return -1;

    char fullpath[FILENAME_MAX];
    int status = 0;

    struct dirent* entry;
    while ((entry = readdir(dir->dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        filepath_join_buf(path, entry->d_name, fullpath, sizeof(fullpath));
        FileAttributes attr;
        if (map_dirent_attrs(entry, fullpath, &attr) != 0) continue;

        if (attr.attrs == FATTR_NONE) {
            populate_file_attrs(fullpath, &attr);
        }

        if (fattr_is_dir(&attr)) {
            if (fp_dir_walk_depth_first_impl(fullpath, callback, data) != 0) {
                status = -1;
                break;
            }
        }

        WalkDirOption opt = callback(&attr, fullpath, entry->d_name, data);
        if (opt == DirStop) break;
        if (opt == DirError) {
            status = -1;
            break;
        }
    }

    dir_close(dir);
    return status;
}

/**
 * @brief Generic readdir-based lazy walk fallback for non-Linux POSIX systems
 *        or when the fast path is unavailable.
 */
static int dir_walkx_generic(const char* path, WalkDirCallbackX callback, void* data) {
    Directory* dir = dir_open(path);
    if (!dir) return -1;

    char fullpath[FILENAME_MAX];
    int status = 0;

    struct dirent* entry;
    while ((entry = readdir(dir->dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (!filepath_join_buf(path, entry->d_name, fullpath, sizeof(fullpath))) {
            status = -1;
            break;
        }
        FileAttributes attr;
        if (map_dirent_attrs(entry, fullpath, &attr) != 0) continue;
        if (attr.attrs == FATTR_NONE) {
            if (populate_file_attrs(fullpath, &attr) != 0) continue;
        }
        LazyFileAttributes lazy = {
            .dirfd = dirfd(dir->dir),
            .name = entry->d_name,
            .d_type = entry->d_type,
            .cached = attr,
            .has_stat = true,
            .is_dir_cached = true,
            .is_dir_value = fattr_is_dir(&attr),
        };
        WalkDirOption opt = callback(&lazy, fullpath, entry->d_name, data);
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
    }

    dir_close(dir);
    return status;
}

int fp_dir_walk_impl(const char* path, WalkDirCallback callback, void* data) {
#ifdef __linux__
    // Fast path: use getdents64 + openat + fstatat for 2-3x speedup
    // Kernel 2.6+ supports all required syscalls; check once and cache
    if (has_fast_dirent()) {
        int dirfd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dirfd >= 0) {
            int ret = dir_walk_fast_helper(path, dirfd, callback, data, 0);
            close(dirfd);
            return ret;
        }
        // Fall through to legacy path if open fails (e.g., permission)
    }
#endif
    return dir_walk_generic(path, callback, data, 0);
}

int fp_dir_walk_depth_first_impl(const char* path, WalkDirCallback callback, void* data) {
#ifdef __linux__
    if (has_fast_dirent()) {
        int dirfd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dirfd >= 0) {
            int ret = dir_walk_depth_first_fast_helper(path, dirfd, callback, data, 0);
            close(dirfd);
            return ret;
        }
    }
#endif
    return dir_walk_depth_first_generic(path, callback, data, 0);
}

int fp_dir_walkx_impl(const char* path, WalkDirCallbackX callback, void* data) {
#ifdef __linux__
    if (has_fast_dirent()) {
        int dirfd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dirfd >= 0) {
            int ret = dir_walkx_fast_helper(path, dirfd, callback, data, 0);
            close(dirfd);
            return ret;
        }
    }
#endif
    return dir_walkx_generic(path, callback, data);
}
