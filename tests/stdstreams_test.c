#include "../include/stdstreams.h"
#include "../include/filepath.h"
#include "../include/macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_iocopy_n(void) {
    FILE* current_file = fopen(__FILE__, "r");
    ASSERT(current_file);
    size_t size = 128;

    stream_t fstream = create_file_stream(current_file);
    stream_t sstream = create_string_stream(size);

    size_t n = io_copy_n(sstream, fstream, size);
    ASSERT_EQ(n, size);
    stream_destroy(sstream);
    stream_destroy(fstream);
}

void test_iocopy(void) {
    FILE* current_file = fopen(__FILE__, "r");
    ASSERT(current_file);
    stream_t fstream = create_file_stream(current_file);
    ASSERT(fstream);
    stream_t dstStream = create_string_stream(2048);
    ASSERT(dstStream);

    char buffer[4096];
    file_stream_read(fstream, buffer, 1, sizeof(buffer));

    size_t n = io_copy(dstStream, fstream);
    ASSERT_TRUE(n > 0);
    buffer[n] = '\0';
    ASSERT_STR_EQ(buffer, string_stream_data(dstStream));

    stream_destroy(dstStream);
    stream_destroy(fstream);
}

void test_readuntil(void) {
    char buf[256];

    stream_t stream  = {0};
    stream_t fstream = {0};
    ssize_t n        = 0;

    stream = create_string_stream(128);
    string_stream_write(stream, "Hello, World!\n");

    const char* data = string_stream_data(stream);
    ASSERT_STR_EQ(data, "Hello, World!\n");

    // Read from stream until a comma or EOF
    n = read_until(stream, ',', buf, sizeof(buf));
    ASSERT_EQ(n, 5);
    ASSERT_STR_EQ(buf, "Hello");
    stream_destroy(stream);

    // create a test file
    const char* file = make_tempfile();
    ASSERT(file);

    FILE* fp = fopen(file, "w+");
    ASSERT(fp);

    fwrite("Hello, World!\n", 1, 14, fp);
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    fstream = create_file_stream(fp);
    n       = read_until(fstream, '\n', buf, sizeof(buf));
    ASSERT(n == 13);
    ASSERT_STR_EQ(buf, "Hello, World!");

    stream_destroy(stream);
}

int main(void) {
    test_readuntil();
    test_iocopy_n();
    test_iocopy();
    return 0;
}
