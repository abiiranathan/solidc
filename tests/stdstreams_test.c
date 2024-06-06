#include "../include/stdstreams.h"
#include "../include/filepath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT(cond, msg)                                                                          \
    if (!(cond)) {                                                                                 \
        fprintf(stderr, "Assertion failed: %s\n", msg);                                            \
        exit(1);                                                                                   \
    }

void test_iocopy_n(void) {
    FILE* current_file = fopen(__FILE__, "r");
    ASSERT(current_file, "Error opening file")
    int size = 128;

    stream_t fstream = create_file_stream(current_file);
    stream_t sstream = create_string_stream(size);

    int n = io_copy_n(sstream, fstream, size);
    ASSERT(n == size, "Error copying streams");
    stream_destroy(sstream);
}

void test_stdinputstreams(void) {
    // test copying from sstream to stdout
    stream_t sstream = create_string_stream(128);
    string_stream_write(sstream, "Hello, World!\n");

    stream_t stdout_stream = create_file_stream(stdout);
    int n = io_copy(stdout_stream, sstream);
    if (n == -1) {
        fprintf(stderr, "Error copying streams\n");
        return;
    }
    stream_destroy(sstream);

#ifdef TEST_STDIN
    // This test is not possible to run in an automated  environment
    // test copying from stdin to stdout
    printf("Enter some text and press Ctrl-D when you are done\n");
    stream_t istream = create_file_stream(stdin);
    n = io_copy_n(stdout_stream, istream, 10);
    if (n == -1) {
        fprintf(stderr, "Error copying streams\n");
        return;
    }
    stream_destroy(istream);
#endif
    stream_destroy(stdout_stream);
}

void test_readuntil(void) {
    stream_t stream;
    stream = create_string_stream(128);

    string_stream_write(stream, "Hello, World!\n");
    const char* data = string_stream_data(stream);
    ASSERT(strcmp(data, "Hello, World!\n") == 0, "Error writing to stream");

    // Read from stream until a comma or EOF
    char buf[256];
    ssize_t n = read_until(stream, ',', buf, sizeof(buf));

    ASSERT(n == 5, "n != 5: Error reading stream");
    ASSERT(strcmp(buf, "Hello") == 0, "Hello expected: Error reading stream");
    stream_destroy(stream);

    // create a test file
    const char* file = make_tempfile();
    ASSERT(file, "Error creating file");

    FILE* fp = fopen(file, "w+");
    ASSERT(fp, "Error opening file");

    fwrite("Hello, World!\n", 1, 14, fp);
    fflush(fp);
    fseek(fp, 0, SEEK_SET);

    stream_t fstream = create_file_stream(fp);
    n = read_until(fstream, '\n', buf, sizeof(buf));
    ASSERT(n == 13, "n != 13: Error reading stream");
    ASSERT(strcmp(buf, "Hello, World!") == 0, "string mismatch: Error reading stream");

    stream_destroy(stream);
}

int main(void) {
    test_readuntil();
    test_iocopy_n();
    test_stdinputstreams();
    return 0;
}

// gcc stdinput_test.c stdinput.c cstr.c arena.c lock.c filepath.c file.c -ggdb