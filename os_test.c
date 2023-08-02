#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os.h"

void test_file_open() {
    // Test file_open with a non-existing file
    File* file = file_open("non_existing_file.txt", "r");
    assert(file == NULL);  // file should be NULL as it couldn't be opened

    // Test file_open with an existing file
    file = file_open("os.h", "r");
    assert(file != NULL);  // file should be opened successfully

    // Test file_read after file_open
    char buffer[100];
    int result = file_read(file, buffer, sizeof(buffer));
    assert(result == 100);  // file_read should return 0 indicating success

    file_free(file);  // Free the file after testing
}

void test_file_write() {
    // Test file_write with a new file
    File* file = file_open("test_write.txt", "w");
    assert(file != NULL);  // file should be opened successfully

    // Write data to the file
    char data[] = "Hello, this is a test write.";
    int result = file_write(file, data);

    // file_write should return number of bytes writen
    // do not include the null-terminator.
    assert(result == 28);

    file_remove(file);
    file_free(file);  // Free the file after testing
}

void test_file_read() {
    File* f = file_open("os_test.c", "r");

    char buf[100];
    int n = file_read(f, buf, 100);
    assert(n == 100);

    file_close(f);
    file_free(f);
}

void test_file_readall() {
    File* f = file_open("os_test.c", "r");
    char* buffer = file_readall(f);
    assert(buffer != NULL);
    free(buffer);

    file_close(f);
    file_free(f);
}

void test_file_copy() {
    File* tmp = file_open("source_file.txt", "w");
    assert(tmp != NULL);

    char data[] = "Hello world!\n";
    size_t n = file_write(tmp, data);
    assert(n == 13);
    file_close(tmp);

    // Test file_copy with an existing source file
    File* src = file_open("source_file.txt", "r");
    assert(src != NULL);  // source file should be opened successfully

    // Test file_copy with a new destination file
    File* dst = file_open("destination_file.txt", "w+");
    assert(dst != NULL);  // destination file should be opened successfully

    long bytes_copied = file_copy(src, dst);
    assert(bytes_copied > 0);

    char buf[13];
    int bytes_read = file_read(dst, buf, sizeof(buf));
    assert(bytes_read == sizeof(buf));

    file_remove(tmp);
    file_remove(src);
    file_remove(dst);

    file_free(tmp);
    file_free(src);  // Free the source file after testing
    file_free(dst);  // Free the destination file after testing
}

void test_file_locking() {
    // Test file_lock and file_unlock with an existing file
    File* file = file_open("lock_test.txt", "w+");
    assert(file != NULL);  // file should be opened successfully

    // Write some data to the file
    char data[] = "This is a test file for locking.";
    file_write(file, data);
    file_rewind(file);

    // Acquire a lock on the entire file
    int lock_result = file_lock(file, 0, 29);
    assert(lock_result == 0);  // Locking should succeed

    // Release the lock on the file
    int unlock_result = file_unlock(file, 0, 10);
    assert(unlock_result == 0);  // Unlocking should succeed

    // Close the file after testing
    file_close(file);
    file_remove(file);
}

int main() {
    test_file_open();
    test_file_write();
    test_file_copy();
    test_file_read();
    test_file_readall();
    test_file_locking();

    printf("All tests passed successfully!\n");
    return 0;
}
