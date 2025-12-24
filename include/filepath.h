/**
 * @file filepath.h
 * @brief Cross-platform file path manipulation and directory traversal utilities.
 */

#ifndef DA20B33A_06DF_4AB0_8B0A_B8874A623312
#define DA20B33A_06DF_4AB0_8B0A_B8874A623312

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "macros.h"
#include "platform.h"

#ifdef _WIN32
#include <windows.h>
#include "win32_dirent.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400  // Required for syncapi
#endif
#define PATH_SEP     '\\'
#define PATH_SEP_STR "\\"
#else
#include <dirent.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

#define PATH_SEP     '/'
#define PATH_SEP_STR "/"

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

//  ===== Directory related function declarations =====
// Directory handle
typedef struct {
    char* path;  // directory path
#ifdef _WIN32
    HANDLE handle;
    WIN32_FIND_DATAW find_data;  // found in windows.h
#else
    DIR* dir;
#endif
} Directory;

/** File attribute flags bitmask */
typedef enum FileAttrFlags {
    FATTR_NONE       = 0,       /**< No attributes set */
    FATTR_FILE       = 1 << 0,  /**< Regular file */
    FATTR_DIR        = 1 << 1,  /**< Directory */
    FATTR_SYMLINK    = 1 << 2,  /**< Symbolic link */
    FATTR_CHARDEV    = 1 << 3,  /**< Character device */
    FATTR_BLOCKDEV   = 1 << 4,  /**< Block device */
    FATTR_FIFO       = 1 << 5,  /**< Named pipe (FIFO) */
    FATTR_SOCKET     = 1 << 6,  /**< Socket */
    FATTR_READABLE   = 1 << 7,  /**< File is readable by caller */
    FATTR_WRITABLE   = 1 << 8,  /**< File is writable by caller */
    FATTR_EXECUTABLE = 1 << 9,  /**< File is executable by caller */
    FATTR_HIDDEN     = 1 << 10, /**< Hidden file (starts with '.') */
} FileAttrFlags;

/** Represents file attributes for directory traversal */
typedef struct FileAttributes {
    /** Full path to the file/directory */
    const char* path;

    /** File/directory name (basename) */
    const char* name;

    /** Bitmask of FileAttrFlags */
    uint32_t attrs;

    /** File size in bytes (0 for directories and special files) */
    size_t size;

    /** Last modification time (Unix timestamp) */
    time_t mtime;
} FileAttributes;

/**
 * Checks if a file has a specific attribute flag.
 * @param attr File attributes structure.
 * @param flag The flag to check (from FileAttrFlags).
 * @return true if the flag is set, false otherwise.
 */
static inline bool fattr_has(const FileAttributes* attr, FileAttrFlags flag) {
    return (attr->attrs & flag) != 0;
}

/**
 * Checks if the file is a regular file.
 * @param attr File attributes structure.
 * @return true if file is a regular file, false otherwise.
 */
static inline bool fattr_is_file(const FileAttributes* attr) {
    return (attr->attrs & FATTR_FILE) != 0;
}

/**
 * Checks if the file is a directory.
 * @param attr File attributes structure.
 * @return true if file is a directory, false otherwise.
 */
static inline bool fattr_is_dir(const FileAttributes* attr) {
    return (attr->attrs & FATTR_DIR) != 0;
}

/**
 * Checks if the file is a symbolic link.
 * @param attr File attributes structure.
 * @return true if file is a symbolic link, false otherwise.
 */
static inline bool fattr_is_symlink(const FileAttributes* attr) {
    return (attr->attrs & FATTR_SYMLINK) != 0;
}

/**
 * Checks if the file is an I/O device (character or block device).
 * @param attr File attributes structure.
 * @return true if file is a device, false otherwise.
 */
static inline bool fattr_is_device(const FileAttributes* attr) {
    return (attr->attrs & (FATTR_CHARDEV | FATTR_BLOCKDEV)) != 0;
}

// Open a directory for reading
WARN_UNUSED_RESULT Directory* dir_open(const char* path);

// Close a directory
void dir_close(Directory* dir);

// Read the next entry in the directory.
WARN_UNUSED_RESULT char* dir_next(Directory* dir);

// Create a directory. Returns 0 if successful, -1 otherwise
int dir_create(const char* path);

// Remove a directory. Returns 0 if successful, -1 otherwise
int dir_remove(const char* path, bool recursive);

// Rename a directory. Returns 0 if successful, -1 otherwise
int dir_rename(const char* oldpath, const char* newpath);

// Change the current working directory
int dir_chdir(const char* path);

// List files in a directory, returns a pointer to a list of file names or
// NULL on error The caller is responsible for freeing the memory. The number
// of files is stored in the count parameter. Note: This algorithm walks the
// directory tree recursively and may be slow for large directories.
WARN_UNUSED_RESULT char** dir_list(const char* path, size_t* count);

// List all contents of the directory and passes the name of file/dir in a
// callback. Skips over "." and ".." to avoid infinite loops.
void dir_list_with_callback(const char* path, void (*callback)(const char* name));

// Returns true if the path is a directory
bool is_dir(const char* path);

// Returns true if a path is a regular file.
bool is_file(const char* path);

// Returns true if path is a symbolic link.
// This function does nothing on Windows.
bool is_symlink(const char* path);

// Create a directory recursively
bool filepath_makedirs(const char* path);

// Get path to platform's TEMP directory.
// On Windows, the TEMP environment variable tried first,
// then TMP, and finally C:\Windows\Temp
// On Unix, /tmp is returned.
// The caller is responsible for freeing the memory
WARN_UNUSED_RESULT char* get_tempdir(void);

// Create a temporary file.
WARN_UNUSED_RESULT char* make_tempfile(void);

// Create a temporary directory.
WARN_UNUSED_RESULT char* make_tempdir(void);

typedef enum WalkDirOption {
    DirContinue,  // Continue walking the directory recursively
    DirStop,      // Stop traversal and return from dir_walk
    DirSkip,      // Skip the current entry and continue traversal.
    DirError,     // An error occured.
} WalkDirOption;

/**
 * Callback function type for directory walking.
 * @param attr File attributes of the current entry.
 * @param data User-provided data pointer.
 * @return WalkDirOption to control traversal behavior.
 */
typedef WalkDirOption (*WalkDirCallback)(const FileAttributes* attr, void* data);

// Walk the directory path, for each entry call the callback
// with path, name and user data pointer.
// Return from the callback 0 to continue or non-zero to stop the walk.
// Returns 0 if successful, -1 otherwise
int dir_walk(const char* path, WalkDirCallback callback, void* data);

/*
 * Depth-first *post-order* walk.
 * Callback is called AFTER a directory's children are processed.
 * Suitable for recursive deletion.
 * Returns 0 if successful, -1 otherwise
 */
int dir_walk_depth_first(const char* path, WalkDirCallback callback, void* data);

// Find the size of the directory.
// This is slow on large directories since it walks the directory.
ssize_t dir_size(const char* path);

bool path_exists(const char* path);

// Returns the path to the current working directory.
WARN_UNUSED_RESULT char* get_cwd(void);

/**
get the file's basename.
Example:
char basename[FILENAME_MAX];
filepath_basename(path, basename, FILENAME_MAX);
*/
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
WARN_UNUSED_RESULT char* filepath_absolute(const char* path);

// Delete or unlink file or directory. Returns 0 if successful, -1 otherwise.
int filepath_remove(const char* path);

// Rename file or directory.
int filepath_rename(const char* oldpath, const char* newpath);

// Returns a pointer to the user's home directory.
// or NULL if the home directory could not be found.
const char* user_home_dir(void);

// Expand user home directory.
WARN_UNUSED_RESULT char* filepath_expanduser(const char* path);

// Expand user home directory and store in expanded buffer.
// Returns true if successful, false otherwise.
bool filepath_expanduser_buf(const char* path, char* expanded, size_t len);

// Join path1 and path2 using standard os specific separator.
// Returns a pointer to the joined path or NULL on error.
WARN_UNUSED_RESULT char* filepath_join(const char* path1, const char* path2);

// Join path1 and path2 using standard os specific separator and store in
// abspath buffer. Returns true if successful, false otherwise.
bool filepath_join_buf(const char* path1, const char* path2, char* abspath, size_t len);

// Split a file path into directory and basename.
// The dir and name parameters must be pre-allocated buffers or
// pointers to pre-allocated buffers.
// dir_size and name_size are the size of the dir and name
// buffers.
void filepath_split(const char* path, char* dir, char* name, size_t dir_size, size_t name_size);

#ifdef __cplusplus
}
#endif

#endif /* DA20B33A_06DF_4AB0_8B0A_B8874A623312 */
