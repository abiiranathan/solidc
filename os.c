#include "os.h"

#include <string.h>

typedef struct File {
    int fd;
    FILE* fp;
    char* name;
    struct stat stats;
#ifdef OS_SUPPORT_LOCKING
    int is_locked;
#endif
} File;

File* file_open(const char* filename, const char* mode) {
    File* file = (File*)malloc(sizeof(File));
    char* msg  = NULL;

    if (!file) {
        perror("Error creating file");
        return NULL;
    }

    file->fp = fopen(filename, mode);
    if (file->fp == NULL) {
        perror("Error opening file");
        return NULL;
    }

    file->name = (char*)filename;
    file->fd   = fileno(file->fp);

    if (fstat(file->fd, &file->stats) == -1) {
        perror("Error getting file stats");
        fclose(file->fp);
        return NULL;
    }

#ifdef OS_SUPPORT_LOCKING
    file->is_locked = 0;
#endif

error:

    return file;
}

// Get pointer to the underlying standard FILE object.
FILE* file_get_ptr(File* file) {
    return file->fp;
}

// Get underlying file descriptor.
int file_get_fd(File* file) {
    return file->fd;
}

int file_close(File* file) {
    return fclose(file->fp);
}

// Free memory pointed to by file.
// If file is null, this function does nothing.
// If the underlying file descriptor is open, it will be closed.
void file_free(File* file) {
    if (file == NULL) {
        return;
    }

    close(file->fd);
    free(file);
    file = NULL;
}

long file_copy(File* src, File* dst) {
    char buffer[BUFSIZ];
    long bytes_written = 0;
    size_t n;
    while ((n = read(src->fd, buffer, sizeof(char) * BUFSIZ)) != 0) {
        long n = write(dst->fd, buffer, sizeof(char) * n);
        if (n > 0) {
            bytes_written += n;
        }
    }
    return bytes_written;
}

int file_move(File* file, const char* newdir) {
    // create intermediate directories if they don't exist
    int status = mkdir(newdir, S_IRWXU | S_IRWXG | S_IRWXO);
    if (status != 0 && errno != EEXIST) {
        perror("Error creating directory");
        return 1;
    }

    // move file to the new destination
    char new_path[BUFSIZ] = {0};
    sprintf(new_path, "%s/%s", newdir, file->name);

    int dest_fd = open(new_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (dest_fd < 0) {
        perror("error creating destination file");
        return 1;
    }

    char buf[4096];
    ssize_t nread;

    while ((nread = fread(buf, sizeof(buf), sizeof(char), file->fp)) > 0) {
        if (write(dest_fd, buf, nread) != nread) {
            perror("error writing to destination file");
            close(dest_fd);
            return 1;
        }
    }

    if (nread < 0) {
        perror("error reading from source file");
        return 1;
    }

    remove(file->name);
    return 0;
}

int file_truncate(File* file, off_t offset) {
    int result = ftruncate(file->fd, offset);
    if (result != 0) {
        perror("Error truncating file");
        return -1;
    }

    if (fstat(file->fd, &file->stats) == -1) {
        perror("Error getting file stats");
        return -1;
    }
    return 0;
}

int file_read(File* file, char* buffer, long bufsize) {
    char buf[BUFSIZ];
    ssize_t nread;
    while ((nread = fread(buf, sizeof(buf), sizeof(char), file->fp)) > 0)
        ;

    if (nread < 0) {
        perror("error reading from file");
        return 1;
    }

    memcpy(buffer, buf, bufsize);
    return 0;
}

int file_write(File* file, char* data, long nbytes) {
    int written = fwrite(data, sizeof(char), nbytes, file->fp);
    if (written < 0) {
        perror("unable to write to file");
        return -1;
    }
    fflush(file->fp);
    return 0;
}

ssize_t file_size(File* file) {
    fseek(file->fp, 0L, SEEK_END);
    ssize_t size = ftell(file->fp);
    // seek back to the beginning of the file
    fseek(file->fp, 0L, SEEK_SET);
    return size;
}

__off64_t file_size_large(File* file) {
    if (fseeko64(file->fp, 0L, SEEK_END) != 0) {
        perror("fseeko64");
        return -1;
    }

    __off64_t pos = ftello64(file->fp);
    if (pos == -1) {
        perror("ftello64");
        return -1;
    }

    if (fseeko64(file->fp, 0L, SEEK_SET) != 0) {
        perror("fseeko64");
        return -1;
    }
    return pos;
}

int file_stats(File* file, struct stat* stats) {
    *stats = file->stats;
    return 0;
}

#ifdef OS_SUPPORT_LOCKING
int file_lock(File* file, off_t offset, off_t length) {
    if (file->is_locked) {
        return -1;  // File is already locked
    }
    struct flock lock;
    lock.l_type   = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start  = offset;
    lock.l_len    = length;

    if (fcntl(file->fd, F_SETLK, &lock) == -1) {
        return -1;  // Failed to lock file
    }
    file->is_locked = 1;
    return 0;
}

int file_unlock(File* file, off_t offset, off_t length) {
    if (!file->is_locked) {
        return -1;  // File is not locked
    }
    struct flock lock;
    lock.l_type   = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start  = offset;
    lock.l_len    = length;
    if (fcntl(file->fd, F_SETLK, &lock) == -1) {
        return -1;  // Failed to unlock file
    }
    file->is_locked = 0;
    return 0;
}
#endif
