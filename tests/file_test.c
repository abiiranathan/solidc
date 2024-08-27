
#include "../include/file.h"
#include "../include/filepath.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_buffer(const char* buffer, size_t bytes_read) {
    for (size_t i = 0; i < bytes_read; ++i) {
        printf("%02x ", ((unsigned char*)buffer)[i]);
    }
    printf("\n");
}

void test_file_open_read_write() {
    // create a temporary file
    char* tmpfile = make_tempfile();
    assert(tmpfile != NULL);
    puts(tmpfile);

    // open the file
    file_t* file = file_open(tmpfile, "w");
    assert(file != NULL);

    // write to the file
    const char* str = "Hello, World!";
    size_t n = file_write_string(file, str);
    assert(n == strlen(str));

    // flush the file before attempting to write asynchronously
    // this is necessary to ensure that the file is written to disk
    // before we attempt to read from it. Especially on Windows.
    fflush(file_fp(file));

    // write asynchronously at the end of the file
    n = file_awrite(file, str, strlen(str), (off_t)strlen(str));
    assert(n == strlen(str));

    // close the file
    file_close(file);

    file = file_open(tmpfile, "r");
    assert(file != NULL);

    // read from the file
    char buffer[1024] = {0};
    size_t bytes_read;
    bytes_read = file_read(file, buffer, 1, sizeof(buffer));
    file_close(file);

    // terminate the buffer
    buffer[bytes_read] = '\0';
    print_buffer(buffer, bytes_read);
    printf("bytes_read: %zu\n", bytes_read);
    assert(bytes_read == strlen(str) * 2);

    // filesize_tostring
    char buf[16];
    filesize_tostring(file_size_char(tmpfile), buf, sizeof(buf));
    puts(buf);

    assert(strcmp(buf, "26 B") == 0);

    free(tmpfile);
}

void test_fileseek() {
    // file_seek
    char* tmpfile = make_tempfile();
    assert(tmpfile != NULL);
    puts(tmpfile);

    file_t* file = file_open(tmpfile, "w+");
    assert(file != NULL);
    const char* sonnet18 = "Shall I compare thee to a summer's day?\n";
    size_t len = strlen(sonnet18);

    size_t n = file_write_string(file, sonnet18);
    assert(n == len);

    // flush the file
    int ok = fflush(file_fp(file));
    assert(ok == 0);

    // seek to the beginning of the file
    file_seek(file, 0, SEEK_SET);

    char buffer[1024];
    size_t bytes_read = file_read(file, buffer, 1, sizeof(buffer));
    file_close(file);
    printf("bytes_read: %zu\n", bytes_read);
    assert(bytes_read == len);

    // terminate the buffer
    buffer[bytes_read] = '\0';
    puts(buffer);
    assert(strcmp(buffer, sonnet18) == 0);

    // seek to the end of the file
    file = file_open(tmpfile, "r");
    assert(file != NULL);
    file_seek(file, 0, SEEK_END);

    bytes_read = file_read(file, buffer, 1, sizeof(buffer));
    file_close(file);
    buffer[bytes_read] = '\0';

    // asset that the buffer is empty
    assert(bytes_read == 0);
    assert(buffer[0] == '\0');

    // seek to the middle of the file
    file = file_open(tmpfile, "r");
    assert(file != NULL);
    file_seek(file, (long)len / 2, SEEK_SET);

    bytes_read = file_read(file, buffer, 1, sizeof(buffer));
    file_close(file);

    // terminate the buffer
    buffer[bytes_read] = '\0';

    puts(buffer);

    assert(bytes_read == len / 2);
    assert(strcmp(buffer, sonnet18 + len / 2) == 0);

    free(tmpfile);
}

void test_file_fileno_fp() {
    char* tmpfile = make_tempfile();
    assert(tmpfile != NULL);
    puts(tmpfile);

    file_t* file = file_open(tmpfile, "w");
    assert(file != NULL);

    int fd = file_fileno(file);
    assert(fd != -1);

    FILE* fp = file_fp(file);
    assert(fp != NULL);

    file_close(file);
    free(tmpfile);
}

void test_getfile_size() {
    char* tmpfile = make_tempfile();
    assert(tmpfile != NULL);
    puts(tmpfile);

    file_t* file = file_open(tmpfile, "w");
    assert(file != NULL);

    // If this ends in \n, the file size will be 1 byte larger than strlen(sonnet18)
    // because of the newline character on windows being \r\n
    const char* sonnet18 = "Shall I compare thee to a summer's day?";
    size_t len = strlen(sonnet18);

    size_t n = file_write_string(file, sonnet18);
    assert(n == len);
    file_close(file);

    ssize_t size = file_size_char(tmpfile);

    printf("computed size: %zd, strlen: %zu\n", size, len);
    assert(size == (ssize_t)len);

    free(tmpfile);
}

void test_async_io() {
    char* tmpfile = make_tempfile();
    assert(tmpfile != NULL);
    puts(tmpfile);

    file_t* file = file_open(tmpfile, "w+");
    assert(file != NULL);

    const char* sonnet18 = "Shall I compare thee to a summer's day?\n";
    size_t len = strlen(sonnet18);

    // write asynchronously at the end of the file
    size_t n = file_awrite(file, sonnet18, len, 0);
    assert(n == len);
    // file_flush(file);

    // read asynchronously from the end of the file
    char buffer[1024];
    ssize_t bytes_read = file_aread(file, buffer, len, 0);
    printf("bytes_read: %zd\n", bytes_read);
    assert(bytes_read == (ssize_t)len);

    // terminate the buffer
    buffer[bytes_read] = '\0';
    puts(buffer);

    file_close(file);
    free(tmpfile);
}

void test_maketempdir() {
    char* tmpdir = make_tempdir();
    assert(tmpdir != NULL);
    puts(tmpdir);
    free(tmpdir);
}

int main(void) {
    test_file_open_read_write();
    test_fileseek();
    test_file_fileno_fp();
    test_getfile_size();
    test_async_io();
    test_maketempdir();
    return 0;
}
