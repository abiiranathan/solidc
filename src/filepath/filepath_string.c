/**
 * @file filepath_string.c
 * @brief Pure string manipulation for file paths: separators, basename,
 *        dirname, extension, joining and splitting.
 *
 * Everything in this file is platform-independent: both '/' and '\\' are
 * accepted as separators on every platform so that foreign paths can be
 * reasoned about anywhere.
 */

#include "filepath_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

size_t fp_strlcpy(char* dst, const char* src, size_t size) {
    if (!dst || !src || size == 0) {
        return 0;
    }
    size_t n = strnlen(src, size - 1);
    memcpy(dst, src, n);
    dst[n] = '\0';
    return n;
}

/** Finds the last path separator ('/' or '\\') in a path, or NULL. */
static const char* last_separator(const char* path) {
    const char* sep = strrchr(path, '/');
    if (!sep) {
        sep = strrchr(path, '\\');
    }
    return sep;
}

// Get basename of path
void filepath_basename(const char* path, char* basename, size_t size) {
    if (!path || !basename || size == 0) {
        if (basename) basename[0] = '\0';
        return;
    }

    const char* base = last_separator(path);
    base = base ? base + 1 : path;
    fp_strlcpy(basename, base, size);
}

// Get dirname of path
void filepath_dirname(const char* path, char* dirname, size_t size) {
    if (!path || !dirname || size == 0) {
        if (dirname) dirname[0] = '\0';
        return;
    }

    const char* base = last_separator(path);
    if (!base) {
        dirname[0] = '\0';
    } else {
        size_t len = (size_t)(base - path);
        fp_strlcpy(dirname, path, len + 1 < size ? len + 1 : size);
    }
}

/*
 * Returns the extension of the BASENAME, including the dot, or "" if the
 * basename has none.  Two subtleties handled here:
 *   - The search must start AFTER the last separator, otherwise
 *     "/path/to.dir/file" would report ".dir/file" (Bug #8).
 *   - A leading dot in the basename marks a hidden file ("~/.bashrc"),
 *     not an extension, so ".bashrc" yields "".
 * Multi-part names behave as expected: "archive.tar.gz" -> ".gz".
 */
void filepath_extension(const char* path, char* ext, size_t size) {
    if (!path || !ext || size == 0) {
        if (ext) ext[0] = '\0';
        return;
    }

    const char* base = last_separator(path);
    base = base ? base + 1 : path;

    const char* dot = strrchr(base, '.');
    if (!dot || dot == base) {
        ext[0] = '\0';
        return;
    }
    fp_strlcpy(ext, dot, size);
}

#define BASENAME_MAX 512

// Get filename without extension
void filepath_nameonly(const char* path, char* name, size_t size) {
    if (!path || !name || size == 0) {
        if (name) name[0] = '\0';
        return;
    }

    char base[BASENAME_MAX] = {0};
    filepath_basename(path, base, BASENAME_MAX);
    char* dot = strrchr(base, '.');

    size_t base_len = strnlen(base, BASENAME_MAX);
    size_t source_len = dot ? (size_t)(dot - base) : base_len;

    // Don't exceed destination size
    size_t to_copy = source_len < size - 1 ? source_len : size - 1;
    memcpy(name, base, to_copy);
    name[to_copy] = '\0';
}

// Join paths
char* filepath_join(const char* path1, const char* path2) {
    if (!path1 || !path2) {
        errno = EINVAL;
        return NULL;
    }

    size_t len = strlen(path1) + strlen(path2) + 2;
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

bool filepath_join_buf(const char* path1, const char* path2, char* abspath, size_t len) {
    if (!path1 || !path2 || !abspath || len == 0) {
        if (abspath && len > 0) abspath[0] = '\0';
        errno = EINVAL;
        return false;
    }

#ifdef _WIN32
    // On Windows, decide which separator to use based on what path1 already uses
    const char* sep = strchr(path1, '\\') ? "\\" : "/";
    int result = snprintf(abspath, len, "%s%s%s", path1, sep, path2);
#else
    int result = snprintf(abspath, len, "%s/%s", path1, path2);
#endif

    // Check for encoding error or truncation
    if (result < 0 || (size_t)result >= len) {
        // Ensure null-termination if truncated (snprintf does this, but good practice to be sure)
        abspath[len - 1] = '\0';
        errno = ENAMETOOLONG;
        return false;
    }

    return true;
}

// Split path into directory and filename
void filepath_split(const char* path, char* dir, char* name, size_t dir_size, size_t name_size) {
    if (!path || !dir || !name || dir_size == 0 || name_size == 0) {
        if (dir) dir[0] = '\0';
        if (name) name[0] = '\0';
        return;
    }

    const char* p = last_separator(path);

    if (!p) {
        dir[0] = '\0';
        fp_strlcpy(name, path, name_size);
    } else {
        size_t len = (size_t)(p - path);
        fp_strlcpy(dir, path, len + 1 < dir_size ? len + 1 : dir_size);
        fp_strlcpy(name, p + 1, name_size);
    }
}
