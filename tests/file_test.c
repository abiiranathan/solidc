
#include "../include/file.h"
#include "../include/filepath.h"
#include "../include/macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_file_open_read_write() {
    // create a temporary file
    char* tmpfile = make_tempfile();
    ASSERT(tmpfile != NULL);

    // open the file
    file_t* file = file_open(tmpfile, "w");
    ASSERT(file != NULL);

    // write to the file
    const char* str = "Hello, World!";
    size_t n        = file_write_string(file, str);
    ASSERT(n == strlen(str));

    file_flush(file);

    // write asynchronously at the end of the file
    n = file_pwrite(file, str, strlen(str), (off_t)strlen(str));
    ASSERT(n == strlen(str));

    // close the file
    file_close(file);

    file = file_open(tmpfile, "r");
    ASSERT(file != NULL);

    // read from the file
    char buffer[1024] = {0};
    size_t bytes_read;
    bytes_read = file_read(file, buffer, 1, sizeof(buffer));

    // terminate the buffer
    buffer[bytes_read] = '\0';
    ASSERT(bytes_read == strlen(str) * 2);

    // filesize_tostring
    int64_t size = file_get_size(file);
    ASSERT(size > 0);

    char buf[16];
    filesize_tostring(size, buf, sizeof(buf));
    ASSERT(strcmp(buf, "26 B") == 0);

    free(tmpfile);
    file_close(file);
}

void test_fileseek() {
    // file_seek
    char* tmpfile = make_tempfile();
    ASSERT(tmpfile != NULL);

    file_t* file = file_open(tmpfile, "w+");
    ASSERT(file != NULL);
    const char* sonnet18 = "Shall I compare thee to a summer's day?\n";
    size_t len           = strlen(sonnet18);

    size_t n = file_write_string(file, sonnet18);
    ASSERT(n == len);

    // flush the file
    int ok = file_flush(file);
    ASSERT(ok == 0);

    // seek to the beginning of the file
    file_seek(file, 0, SEEK_SET);

    char buffer[1024];
    size_t bytes_read = file_read(file, buffer, 1, sizeof(buffer));
    file_close(file);
    ASSERT(bytes_read == len);
    // terminate the buffer
    buffer[bytes_read] = '\0';
    ASSERT(strcmp(buffer, sonnet18) == 0);

    file = file_open(tmpfile, "r");
    ASSERT(file != NULL);
    file_seek(file, 0, SEEK_END);

    bytes_read = file_read(file, buffer, 1, sizeof(buffer));
    file_close(file);
    buffer[bytes_read] = '\0';

    // asset that the buffer is empty
    ASSERT(bytes_read == 0);
    ASSERT(buffer[0] == '\0');

    // seek to the middle of the file
    file = file_open(tmpfile, "r");
    ASSERT(file != NULL);
    file_seek(file, (long)len / 2, SEEK_SET);

    bytes_read = file_read(file, buffer, 1, sizeof(buffer));
    file_close(file);

    // terminate the buffer
    buffer[bytes_read] = '\0';

    ASSERT(bytes_read == len / 2);
    ASSERT(strcmp(buffer, sonnet18 + len / 2) == 0);

    free(tmpfile);
}

void test_getfile_size() {
    char* tmpfile = make_tempfile();
    ASSERT(tmpfile != NULL);

    file_t* file = file_open(tmpfile, "w");
    ASSERT(file != NULL);

    // If this ends in \n, the file size will be 1 byte larger than
    // strlen(sonnet18) because of the newline character on windows being \r\n
    const char* sonnet18 = "Shall I compare thee to a summer's day?";
    size_t len           = strlen(sonnet18);

    size_t n = file_write_string(file, sonnet18);
    file_close(file);
    ASSERT(n == len);

    int64_t size = get_file_size(tmpfile);
    ASSERT(size == (int64_t)len);
    free(tmpfile);
}

void test_async_io() {
    char* tmpfile = make_tempfile();
    ASSERT(tmpfile != NULL);

    file_t* file = file_open(tmpfile, "w+");
    ASSERT(file != NULL);

    const char* sonnet18 = "Shall I compare thee to a summer's day?\n";
    size_t len           = strlen(sonnet18);

    // write asynchronously at the end of the file
    size_t n = file_pwrite(file, sonnet18, len, 0);
    ASSERT(n == len);
    // file_flush(file);

    // read asynchronously from the end of the file
    char buffer[1024];
    ssize_t bytes_read = file_pread(file, buffer, len, 0);
    ASSERT(bytes_read == (ssize_t)len);

    // terminate the buffer
    buffer[bytes_read] = '\0';

    file_close(file);
    free(tmpfile);
}

void test_maketempdir() {
    char* tmpdir = make_tempdir();
    ASSERT(tmpdir != NULL);
    free(tmpdir);
}

int main(void) {
    test_file_open_read_write();
    test_fileseek();
    test_getfile_size();
    test_async_io();
    test_maketempdir();
    return 0;
}
