#include "../include/file.h"
#include "../include/filepath.h"
#include "../include/macros.h"

void test_file_open_read_write() {
    // create a temporary file
    char* tmpfile = make_tempfile();
    ASSERT(tmpfile != NULL);
    file_t file;
    file_result_t res;

    res = file_open(&file, tmpfile, "w");
    ASSERT(res == FILE_SUCCESS);

    // write to the file
    const char* str = "Hello, World!";
    size_t length   = strlen(str);
    size_t n        = file_write_string(&file, str);
    ASSERT(n == strlen(str));

    file_flush(&file);

    // write asynchronously at the end of the file
    ssize_t written = file_pwrite(&file, str, length, (off_t)length);
    ASSERT(written != -1);
    ASSERT((size_t)written == length);

    // close the file
    file_close(&file);

    res = file_open(&file, tmpfile, "r");
    ASSERT(res == FILE_SUCCESS);

    // read from the file
    char buffer[1024] = {0};
    size_t bytes_read = file_read(&file, buffer, 1, sizeof(buffer));

    // terminate the buffer
    buffer[bytes_read] = '\0';
    ASSERT(bytes_read == strlen(str) * 2);

    // filesize_tostring
    int64_t size = file_get_size(&file);
    ASSERT(size > 0);

    char buf[16];
    filesize_tostring((uint64_t)size, buf, sizeof(buf));
    ASSERT(strcmp(buf, "26 B") == 0);

    free(tmpfile);
    file_close(&file);
}

void test_fileseek() {
    char* tmpfile = make_tempfile();
    ASSERT(tmpfile != NULL);

    file_t file;
    file_result_t res;

    res = file_open(&file, tmpfile, "w+");
    ASSERT(res == FILE_SUCCESS);

    const char* sonnet18 = "Shall I compare thee to a summer's day?\n";
    size_t len           = strlen(sonnet18);

    size_t n = file_write_string(&file, sonnet18);
    ASSERT(n == len);

    ASSERT(file_flush(&file) == FILE_SUCCESS);

    // seek to the beginning of the file
    ASSERT(file_seek(&file, 0, SEEK_SET) == FILE_SUCCESS);

    char buffer[1024];
    size_t bytes_read = file_read(&file, buffer, 1, sizeof(buffer));
    ASSERT(bytes_read == len);
    buffer[bytes_read] = '\0';
    ASSERT(strcmp(buffer, sonnet18) == FILE_SUCCESS);

    file_close(&file);

    // seek to end
    res = file_open(&file, tmpfile, "r");
    ASSERT(res == FILE_SUCCESS);
    ASSERT(file_seek(&file, 0, SEEK_END) == FILE_SUCCESS);

    bytes_read = file_read(&file, buffer, 1, sizeof(buffer));
    ASSERT(bytes_read == 0);
    buffer[0] = '\0';
    ASSERT(buffer[0] == '\0');

    file_close(&file);

    // seek to middle
    res = file_open(&file, tmpfile, "r");
    ASSERT(res == FILE_SUCCESS);
    ASSERT(file_seek(&file, (long)len / 2, SEEK_SET) == FILE_SUCCESS);

    bytes_read         = file_read(&file, buffer, 1, sizeof(buffer));
    buffer[bytes_read] = '\0';
    ASSERT(bytes_read == len / 2);
    ASSERT(strcmp(buffer, sonnet18 + len / 2) == 0);

    free(tmpfile);
    file_close(&file);
}

void test_getfile_size() {
    char* tmpfile = make_tempfile();
    ASSERT(tmpfile != NULL);

    file_t file;
    file_result_t res = file_open(&file, tmpfile, "w");
    ASSERT(res == FILE_SUCCESS);

    const char* sonnet18 = "Shall I compare thee to a summer's day?";
    size_t len           = strlen(sonnet18);

    size_t n = file_write_string(&file, sonnet18);
    ASSERT(n == len);

    file_close(&file);

    int64_t size = get_file_size(tmpfile);
    ASSERT(size == (int64_t)len);

    free(tmpfile);
}

void test_async_io() {
    char* tmpfile = make_tempfile();
    ASSERT(tmpfile != NULL);

    file_t file;
    file_result_t res = file_open(&file, tmpfile, "w+");
    ASSERT(res == FILE_SUCCESS);

    const char* sonnet18 = "Shall I compare thee to a summer's day?\n";
    size_t len           = strlen(sonnet18);

    // write asynchronously
    ssize_t n = file_pwrite(&file, sonnet18, len, 0);
    ASSERT(n == (ssize_t)len);

    char buffer[1024];
    ssize_t bytes_read = file_pread(&file, buffer, len, 0);
    ASSERT(bytes_read == (ssize_t)len);

    buffer[bytes_read] = '\0';

    file_close(&file);
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
    puts("☑️ All file tests passed\n");
    return 0;
}
