#ifndef DA20B33A_06DF_4AB0_8B0A_B8874A623312
#define DA20B33A_06DF_4AB0_8B0A_B8874A623312

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#include <dirent.h>
#include <windows.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400  // Required for syncapi
#define PATH_SEP     '\\'
#define PATH_SEP_STR "\\"
#endif
#else
#include <dirent.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>
#define PATH_SEP     '/'
#define PATH_SEP_STR "/"
#endif

#ifndef MAX_PATH
#define MAX_PATH 1024
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
    DIR* dir;  // directory pointer found in dirent.h
#endif
} Directory;

// Open a directory for reading
Directory* dir_open(const char* path) __attribute__((warn_unused_result));

// Close a directory
void dir_close(Directory* dir);

// Read the next entry in the directory.
char* dir_next(Directory* dir) __attribute__((warn_unused_result));

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
char** dir_list(const char* path, size_t* count) __attribute__((warn_unused_result));

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
char* get_tempdir(void) __attribute__((warn_unused_result));

// Create a temporary file.
char* make_tempfile(void) __attribute__((warn_unused_result));

// Create a temporary directory.
char* make_tempdir(void) __attribute__((warn_unused_result));

typedef enum WalkDirOption {
    DirContinue,  // Continue walking the directory recursively
    DirStop,      // Stop traversal and return from dir_walk
    DirSkip,      // Skip the current entry and continue traversal.
    DirError,     // An error occured.
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
char* get_cwd(void) __attribute__((warn_unused_result));

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
char* filepath_absolute(const char* path) __attribute__((warn_unused_result));

// Delete or unlink file or directory. Returns 0 if successful, -1 otherwise.
int filepath_remove(const char* path);

// Rename file or directory.
int filepath_rename(const char* oldpath, const char* newpath);

// Returns a pointer to the user's home directory.
// or NULL if the home directory could not be found.
const char* user_home_dir(void);

// Expand user home directory.
char* filepath_expanduser(const char* path) __attribute__((warn_unused_result));

// Expand user home directory and store in expanded buffer.
// Returns true if successful, false otherwise.
bool filepath_expanduser_buf(const char* path, char* expanded, size_t len);

// Join path1 and path2 using standard os specific separator.
// Returns a pointer to the joined path or NULL on error.
char* filepath_join(const char* path1, const char* path2) __attribute__((warn_unused_result));

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
