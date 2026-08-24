/*
MIT License
Copyright (c) 2019 win32ports
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// Source: https://github.com/win32ports/dirent_h

#ifdef _WIN32
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <ctype.h>  // for isdigit
#include <errno.h>
#include <shlwapi.h>  // for PathIsRelativeW
#include <stdint.h>
#include <stdlib.h>  // for malloc, free
#include <string.h>  // for memcpy, memset
#include <sys/types.h>
#include <windows.h>   // for Windows API
#include <winioctl.h>  // for FSCTL_GET_REPARSE_POINT

#ifdef _MSC_VER
#pragma comment(lib, "Shlwapi.lib")
#endif

#include <wincrypt.h>                 // For HCRYPTPROV, CryptAcquireContextW, CryptGenRandom, etc.
#pragma comment(lib, "advapi32.lib")  // Link against the Crypto API library

/*
 * Constants and Macros
 */

#ifndef NAME_MAX
#define NAME_MAX 260
#endif /* NAME_MAX */

#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif /* DT_UNKNOWN */

#ifndef DT_FIFO
#define DT_FIFO 1
#endif /* DT_FIFO */

#ifndef DT_CHR
#define DT_CHR 2
#endif /* DT_CHR */

#ifndef DT_DIR
#define DT_DIR 4
#endif /* DT_DIR */

#ifndef DT_BLK
#define DT_BLK 6
#endif /* DT_BLK */

#ifndef DT_REG
#define DT_REG 8
#endif /* DT_REG */

#ifndef DT_LNK
#define DT_LNK 10
#endif /* DT_LNK */

#ifndef DT_SOCK
#define DT_SOCK 12
#endif /* DT_SOCK */

#ifndef DT_WHT
#define DT_WHT 14
#endif /* DT_WHT */

#ifndef NTFS_MAX_PATH
#define NTFS_MAX_PATH 32768
#endif /* NTFS_MAX_PATH */

#ifndef MAXIMUM_REPARSE_DATA_BUFFER_SIZE
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE 16384
#endif /* MAXIMUM_REPARSE_DATA_BUFFER_SIZE */

#ifndef IO_REPARSE_TAG_SYMLINK
#define IO_REPARSE_TAG_SYMLINK 0xA000000C
#endif /* IO_REPARSE_TAG_SYMLINK */

#ifndef FILE_NAME_NORMALIZED
#define FILE_NAME_NORMALIZED 0
#endif /* FILE_NAME_NORMALIZED */

typedef void* DIR;

/** Inode structure for Windows */
typedef struct ino_t {
    unsigned long long serial;
    unsigned char fileid[16];
} __ino_t;

/** Directory entry structure */
struct dirent {
    __ino_t d_ino;           /* File serial number */
    off_t d_off;             /* Offset to the next dirent */
    unsigned short d_reclen; /* Length of this record */
    unsigned char d_namelen; /* Length of the name */
    unsigned char d_type;    /* Type of file (DT_*) */
    char d_name[NAME_MAX];   /* Null-terminated filename */
};

/** Internal directory structure */
struct __dir {
    struct dirent* entries;
    intptr_t fd;
    long int count;
    long int index;
};

/**
 * @brief Closes a directory stream.
 *
 * @param dirp The directory stream to close.
 * @return int 0 on success, -1 on error (errno is set).
 */
int closedir(DIR* dirp) {
    struct __dir* data = NULL;
    if (!dirp) {
        errno = EBADF;
        return -1;
    }
    data = (struct __dir*)dirp;
    CloseHandle((HANDLE)data->fd);
    free(data->entries);
    free(data);
    return 0;
}

/** Sets errno in a compiler-agnostic way. */
static void __seterrno(int value) {
#ifdef _MSC_VER
    _set_errno(value);
#else  /* _MSC_VER */
    errno = value;
#endif /* _MSC_VER */
}

/** Checks if a path is a symbolic link. */
static int __islink(const wchar_t* name, char* buffer) {
    DWORD io_result = 0;
    DWORD bytes_returned = 0;
    HANDLE hFile =
        CreateFileW(name, 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    io_result = (DWORD)DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, NULL, 0, buffer,
                                       MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &bytes_returned, NULL);

    CloseHandle(hFile);

    if (io_result == 0) return 0;

    return ((REPARSE_GUID_DATA_BUFFER*)buffer)->ReparseTag == IO_REPARSE_TAG_SYMLINK;
}

#pragma pack(push, 1)

typedef struct dirent_FILE_ID_128 {
    BYTE Identifier[16];
} dirent_FILE_ID_128;

typedef struct _dirent_FILE_ID_INFO {
    ULONGLONG VolumeSerialNumber;
    dirent_FILE_ID_128 FileId;
} dirent_FILE_ID_INFO;

#pragma pack(pop)

typedef enum dirent_FILE_INFO_BY_HANDLE_CLASS { dirent_FileIdInfo = 18 } dirent_FILE_INFO_BY_HANDLE_CLASS;

/** Retrieves the inode information for a file. */
static __ino_t __inode(const wchar_t* name) {
    __ino_t value = {0};
    BOOL result;
    dirent_FILE_ID_INFO fileid;
    BY_HANDLE_FILE_INFORMATION info;
    typedef BOOL(__stdcall * pfnGetFileInformationByHandleEx)(HANDLE hFile,
                                                              dirent_FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
                                                              LPVOID lpFileInformation, DWORD dwBufferSize);

    HANDLE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) return value;

    pfnGetFileInformationByHandleEx fnGetFileInformationByHandleEx =
        (pfnGetFileInformationByHandleEx)(void*)GetProcAddress(hKernel32, "GetFileInformationByHandleEx");

    if (!fnGetFileInformationByHandleEx) return value;

    HANDLE hFile = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
    if (hFile == INVALID_HANDLE_VALUE) return value;

    result = fnGetFileInformationByHandleEx(hFile, dirent_FileIdInfo, &fileid, sizeof(fileid));
    if (result) {
        value.serial = fileid.VolumeSerialNumber;
        memcpy(value.fileid, fileid.FileId.Identifier, 16);
    } else {
        result = GetFileInformationByHandle(hFile, &info);
        if (result) {
            value.serial = info.dwVolumeSerialNumber;
            memcpy(value.fileid + 8, &info.nFileIndexHigh, 4);
            memcpy(value.fileid + 12, &info.nFileIndexLow, 4);
        }
    }
    CloseHandle(hFile);
    return value;
}

/** Internal implementation of opendir with wide-character path. */
static DIR* __internal_opendir(wchar_t* wname, int size) {
    struct __dir* data = NULL;
    struct dirent* tmp_entries = NULL;
    static char default_char = '?';
    static wchar_t* suffix = L"\\*.*";
    static int extra_prefix = 4; /* use prefix "\\?\" to handle long file names */
    static int extra_suffix = 4; /* use suffix "\*.*" to find everything */
    WIN32_FIND_DATAW w32fd = {0};
    HANDLE hFindFile = INVALID_HANDLE_VALUE;
    static size_t grow_factor = 2;
    char* buffer = NULL;

    BOOL relative = PathIsRelativeW(wname + extra_prefix);

    memcpy(wname + size - 1, suffix, sizeof(wchar_t) * (size_t)extra_suffix);
    wname[size + extra_suffix - 1] = 0;

    if (relative) {
        wname += extra_prefix;
        size -= extra_prefix;
    }
    hFindFile = FindFirstFileW(wname, &w32fd);
    if (INVALID_HANDLE_VALUE == hFindFile) {
        __seterrno(ENOENT);
        return NULL;
    }

    data = (struct __dir*)malloc(sizeof(struct __dir));
    if (!data) goto out_of_memory;
    wname[size - 1] = 0;
    data->fd = (intptr_t)CreateFileW(wname, 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    wname[size - 1] = L'\\';
    data->count = 16;
    data->index = 0;
    data->entries = (struct dirent*)malloc(sizeof(struct dirent) * (size_t)data->count);
    if (!data->entries) goto out_of_memory;
    buffer = malloc(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    if (!buffer) goto out_of_memory;
    do {
        WideCharToMultiByte(CP_UTF8, 0, w32fd.cFileName, -1, data->entries[data->index].d_name, NAME_MAX, &default_char,
                            NULL);

        memcpy(wname + size, w32fd.cFileName, sizeof(wchar_t) * NAME_MAX);

        if (((w32fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT) &&
            __islink(wname, buffer))
            data->entries[data->index].d_type = DT_LNK;
        else if ((w32fd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) == FILE_ATTRIBUTE_DEVICE)
            data->entries[data->index].d_type = DT_CHR;
        else if ((w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
            data->entries[data->index].d_type = DT_DIR;
        else
            data->entries[data->index].d_type = DT_REG;

        data->entries[data->index].d_ino = __inode(wname);
        data->entries[data->index].d_reclen = sizeof(struct dirent);
        data->entries[data->index].d_namelen = (unsigned char)wcslen(w32fd.cFileName);
        data->entries[data->index].d_off = data->index + 1;  // POSIX fix: offset to next entry

        if (++data->index == data->count) {
            tmp_entries =
                (struct dirent*)realloc(data->entries, sizeof(struct dirent) * (size_t)data->count * grow_factor);
            if (!tmp_entries) goto out_of_memory;
            data->entries = tmp_entries;
            data->count *= grow_factor;
        }
    } while (FindNextFileW(hFindFile, &w32fd) != 0);

    free(buffer);
    FindClose(hFindFile);

    data->count = data->index;
    data->index = 0;
    return (DIR*)data;
out_of_memory:
    if (data) {
        if (INVALID_HANDLE_VALUE != (HANDLE)data->fd) CloseHandle((HANDLE)data->fd);
        free(data->entries);
    }
    free(buffer);
    free(data);
    if (INVALID_HANDLE_VALUE != hFindFile) FindClose(hFindFile);
    __seterrno(ENOMEM);
    return NULL;
}

/** Allocates a buffer for the wide-character path with long path prefix. */
static wchar_t* __get_buffer(void) {
    wchar_t* name = malloc(sizeof(wchar_t) * (NTFS_MAX_PATH + NAME_MAX + 8));
    if (name) memcpy(name, L"\\\\?\\", sizeof(wchar_t) * 4);
    return name;
}

/**
 * @brief Opens a directory stream corresponding to the directory name.
 *
 * @param name The path to the directory to open (UTF-8).
 * @return DIR* A pointer to the directory stream, or NULL on error (errno is set).
 */
DIR* opendir(const char* name) {
    DIR* dirp = NULL;
    wchar_t* wname = __get_buffer();
    int size = 0;
    if (!wname) {
        errno = ENOMEM;
        return NULL;
    }
    size = MultiByteToWideChar(CP_UTF8, 0, name, -1, wname + 4, NTFS_MAX_PATH);
    if (0 == size) {
        free(wname);
        return NULL;
    }
    dirp = __internal_opendir(wname, size + 4);
    free(wname);
    return dirp;
}

/**
 * @brief Opens a directory stream using a wide-character string (Windows extension).
 *
 * @param name The path to the directory to open (wchar_t).
 * @return DIR* A pointer to the directory stream, or NULL on error.
 */
DIR* _wopendir(const wchar_t* name) {
    DIR* dirp = NULL;
    wchar_t* wname = __get_buffer();
    int size = 0;
    if (!wname) {
        errno = ENOMEM;
        return NULL;
    }
    size = wcslen(name);
    if (size > NTFS_MAX_PATH) {
        free(wname);
        return NULL;
    }
    memcpy(wname + 4, name, sizeof(wchar_t) * ((size_t)size + 1));
    dirp = __internal_opendir(wname, size + 5);
    free(wname);
    return dirp;
}

/**
 * @brief Opens a directory stream from an existing file descriptor.
 *
 * @param fd The file descriptor/handle.
 * @return DIR* A pointer to the directory stream, or NULL on error.
 */
DIR* fdopendir(intptr_t fd) {
    DIR* dirp = NULL;
    wchar_t* wname = __get_buffer();
    typedef DWORD(__stdcall * pfnGetFinalPathNameByHandleW)(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath,
                                                            DWORD dwFlags);

    HANDLE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) {
        errno = EINVAL;
        return NULL;
    }

    pfnGetFinalPathNameByHandleW fnGetFinalPathNameByHandleW =
        (pfnGetFinalPathNameByHandleW)(void*)GetProcAddress(hKernel32, "GetFinalPathNameByHandleW");
    if (!fnGetFinalPathNameByHandleW) {
        errno = EINVAL;
        return NULL;
    }

    size_t size = 0;
    if (!wname) {
        errno = ENOMEM;
        return NULL;
    }
    size = fnGetFinalPathNameByHandleW((HANDLE)fd, wname + 4, NTFS_MAX_PATH, FILE_NAME_NORMALIZED);
    if (0 == size) {
        free(wname);
        errno = ENOTDIR;
        return NULL;
    }
    dirp = __internal_opendir(wname, size + 5);
    free(wname);
    return dirp;
}

/**
 * @brief Reads the next directory entry from the stream.
 *
 * @param dirp The directory stream.
 * @return struct dirent* Pointer to the directory entry structure, or NULL if end of stream or error.
 */
struct dirent* readdir(DIR* dirp) {
    struct __dir* data = (struct __dir*)dirp;
    if (!data) {
        errno = EBADF;
        return NULL;
    }
    if (data->index < data->count) {
        return &data->entries[data->index++];
    }
    return NULL;
}

/**
 * @brief Thread-safe version of readdir.
 *
 * @param dirp The directory stream.
 * @param entry The caller-allocated buffer to store the entry.
 * @param result Pointer to the resulting entry pointer (set to NULL on end of stream).
 * @return int 0 on success, or an error number.
 */
int readdir_r(DIR* dirp, struct dirent* entry, struct dirent** result) {
    struct __dir* data = (struct __dir*)dirp;
    if (!data || !entry || !result) {
        return EINVAL;
    }
    if (data->index < data->count) {
        memcpy(entry, &data->entries[data->index++], sizeof(struct dirent));
        *result = entry;
    } else {
        *result = NULL;
    }
    return 0;
}

/**
 * @brief Sets the position of the directory stream.
 *
 * @param dirp The directory stream.
 * @param offset The offset to seek to.
 */
void seekdir(DIR* dirp, long int offset) {
    if (dirp) {
        struct __dir* data = (struct __dir*)dirp;
        if (offset >= 0 && offset <= data->count) {
            data->index = offset;
        }
    }
}

/**
 * @brief Resets the position of the directory stream to the beginning.
 *
 * @param dirp The directory stream.
 */
void rewinddir(DIR* dirp) { seekdir(dirp, 0); }

/**
 * @brief Returns the current position of the directory stream.
 *
 * @param dirp The directory stream.
 * @return long int The current position (index), or -1 on error.
 */
long int telldir(DIR* dirp) {
    if (!dirp) {
        errno = EBADF;
        return -1;
    }
    return ((struct __dir*)dirp)->index;
}

/**
 * @brief Returns the underlying file descriptor/handle of the directory stream.
 *
 * @param dirp The directory stream.
 * @return intptr_t The handle, or -1 on error.
 */
intptr_t dirfd(DIR* dirp) {
    if (!dirp) {
        errno = EINVAL;
        return -1;
    }
    return ((struct __dir*)dirp)->fd;
}

/**
 * @brief Scans a directory for matching entries.
 *
 * @param dirp The path of the directory to scan.
 * @param namelist Returns a pointer to an array of pointers to directory entries.
 * @param filter Function to filter entries (return non-zero to include).
 * @param compar Function to compare entries for sorting (passed to qsort).
 * @return int The number of entries selected, or -1 on error.
 */
int scandir(const char* dirp, struct dirent*** namelist, int (*filter)(const struct dirent*),
            int (*compar)(const struct dirent**, const struct dirent**)) {
    struct dirent **entries = NULL, **tmp_entries = NULL;
    unsigned long int i = 0, index = 0, count = 16;
    DIR* d = opendir(dirp);
    struct __dir* data = (struct __dir*)d;
    if (!data) {
        closedir(d);
        __seterrno(ENOENT);
        return -1;
    }
    entries = (struct dirent**)malloc(sizeof(struct dirent*) * count);
    if (!entries) {
        closedir(d);
        __seterrno(ENOMEM);
        return -1;
    }

    for (i = 0; i < (size_t)data->count; ++i) {
        if (!filter || filter(&data->entries[i])) {
            entries[index] = (struct dirent*)malloc(sizeof(struct dirent));
            if (!entries[index]) {
                closedir(d);
                for (i = 0; i < index; ++i) free(entries[i]);
                free(entries);
                __seterrno(ENOMEM);
                return -1;
            }
            memcpy(entries[index], &data->entries[i], sizeof(struct dirent));
            if (++index == count) {
                tmp_entries = (struct dirent**)realloc(entries, sizeof(struct dirent*) * count * 2);
                if (!tmp_entries) {
                    closedir(d);
                    for (i = 0; i < index; ++i) free(entries[i]);
                    free(entries);
                    __seterrno(ENOMEM);
                    return -1;
                }
                entries = tmp_entries;
                count *= 2;
            }
        }
    }
    qsort(entries, index, sizeof(struct dirent*), (int (*)(const void*, const void*))compar);
    entries[index] = NULL;
    if (namelist) *namelist = entries;
    closedir(d);
    return (int)index;
}

/**
 * @brief Compare two directory entries alphabetically.
 * Use as the 'compar' argument for scandir.
 */
int alphasort(const struct dirent** a, const struct dirent** b) {
    if (!a || !b || !*a || !*b) return 0;
    return strcoll((*a)->d_name, (*b)->d_name);
}

/**
 * @brief Minimal implementation of version/natural sort comparison.
 * Compares numeric parts as numbers, text parts lexicographically.
 */
static int __strverscmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return 0;

    while (*s1 && *s2) {
        if (isdigit((unsigned char)*s1) && isdigit((unsigned char)*s2)) {
            // Compare numeric parts
            unsigned long n1 = strtoul(s1, (char**)&s1, 10);
            unsigned long n2 = strtoul(s2, (char**)&s2, 10);
            if (n1 != n2) return (n1 < n2) ? -1 : 1;
        } else {
            if (*s1 != *s2) return ((unsigned char)*s1 < (unsigned char)*s2) ? -1 : 1;
            s1++;
            s2++;
        }
    }
    return *s1 ? 1 : (*s2 ? -1 : 0);
}

/**
 * @brief Compare two directory entries by version (natural sort).
 * Use as the 'compar' argument for scandir.
 */
int versionsort(const struct dirent** a, const struct dirent** b) {
    if (!a || !b || !*a || !*b) return 0;
    return __strverscmp((*a)->d_name, (*b)->d_name);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _WIN32 */
