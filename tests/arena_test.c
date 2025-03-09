#include "../include/arena.h"
#include "../include/thread.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_THREADS 4

#define ASSERT(cond, msg)                                                                          \
    if (!(cond)) {                                                                                 \
        printf("%s\n", msg);                                                                       \
        exit(1);                                                                                   \
    }

static void* thread_func(void* arg) {
    Arena* arena = (Arena*)arg;
    char* buffer = arena_alloc_string(arena, "Hello from thread!");
    if (buffer) {
        printf("Thread-ID %zu: %s\n", thread_self(), buffer);
    }
    return NULL;
}

void multithreaded_example(void) {
    // Create an arena of with default chunk size and alignment
    Arena* arena = arena_create(0);
    ASSERT(arena, "Failed to create arena.");

    Thread threads[NUM_THREADS] = {0};

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_create(&threads[i], thread_func, arena);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], NULL);
    }

    // Destroy the arena
    arena_destroy(arena);
}

void print_string(const char* str) {
    printf("%s\n", str);
}

static void reusing_arena_chunks_example(void) {
    Arena* arena = arena_create(124);
    ASSERT(arena, "Failed to create arena.")

    int num_strings = 100;

    // Allocate memory
    for (int i = 0; i < num_strings; i++) {
        char* str = arena_alloc_string(arena, "Hello from Arena Allocator!");
        ASSERT(str, "arena_alloc_string: Failed to allocate memory.")
        print_string(str);
    }

    // Allocate again to reuse the chunks
    for (int i = 0; i < num_strings; i++) {
        char* str = arena_alloc_string(arena, "Reusing the chunks!");
        ASSERT(str, "arena_alloc_string: Failed to allocate memory.")
        (void)str;
    }

    // Destroy the arena
    arena_destroy(arena);
}

// Simple function to print test results
void print_test_result(const char* test_name, int success) {
    printf("%-30s: %s\n", test_name, success ? "PASS" : "FAIL");
}

int test_arena_realloc(void) {
    // Create an arena with a small chunk size for testing
    Arena* arena = arena_create(4096);  // 4 KB chunks
    if (arena == NULL) {
        printf("Failed to create arena\n");
        return 1;
    }

    // Test 1: Allocate and realloc to a larger size
    char* str1 = arena_alloc_string(arena, "Hello");
    if (str1 == NULL) {
        printf("Initial allocation failed\n");
        arena_destroy(arena);
        return 1;
    }
    strcpy(str1, "Hello");  // Ensure data is written
    char* str1_realloc = arena_realloc(arena, str1, 20);
    int test1_pass     = (str1_realloc != NULL && strcmp(str1, "Hello") == 0);
    print_test_result("Realloc to larger size", test1_pass);
    if (test1_pass) {
        strcpy(str1_realloc, "Hello, World!");  // Test writing to new size
        test1_pass = (strcmp(str1_realloc, "Hello, World!") == 0);
        print_test_result("Write to larger realloc", test1_pass);
    }

    // Test 2: Realloc with NULL pointer (should act like alloc)
    char* str2     = arena_realloc(arena, NULL, 10);
    int test2_pass = (str2 != NULL);
    print_test_result("Realloc with NULL", test2_pass);
    if (test2_pass) {
        strcpy(str2, "Test");
        test2_pass = (strcmp(str2, "Test") == 0);
        print_test_result("Write to NULL realloc", test2_pass);
    }

    // Test 3: Multiple allocations followed by realloc
    int* num1 = arena_alloc(arena, sizeof(int));
    int* num2 = arena_alloc(arena, sizeof(int));

    if (num1 == NULL || num2 == NULL) {
        printf("Multiple allocations failed\n");
        arena_destroy(arena);
        return 1;
    }
    *num1             = 42;
    *num2             = 99;
    int* num1_realloc = arena_realloc(arena, num1, sizeof(int) * 2);
    int test3_pass    = (num1_realloc != NULL && *num1_realloc == 42);
    print_test_result("Realloc after multiple allocs", test3_pass);
    if (test3_pass) {
        num1_realloc[1] = 100;  // Test writing to new space
        test3_pass      = (num1_realloc[1] == 100);
        print_test_result("Write to extended array", test3_pass);
    }

    // Test 4: Realloc to a smaller size
    char* str3 = arena_alloc_string(arena, "LongStringHere");
    if (str3 == NULL) {
        printf("Allocation for shrink test failed\n");
        arena_destroy(arena);
        return 1;
    }
    char* str3_realloc = arena_realloc(arena, str3, 5);
    int test4_pass     = (str3_realloc != NULL && strncmp(str3_realloc, "LongStringHere", 4) == 0);
    print_test_result("Realloc to smaller size", test4_pass);

    // Clean up
    arena_destroy(arena);
    printf("All tests completed.\n");

    // Arena with small blocks
    arena = arena_create(16);
    if (arena == NULL) {
        printf("Failed to create arena\n");
        return 1;
    }

    char* s1 = arena_alloc_string(arena, "Hello World my dear friends!");
    if (s1 == NULL) {
        printf("Failed to allocate string\n");
        return 1;
    }

    s1 = arena_realloc(arena, s1, 32);
    if (s1 == NULL) {
        printf("Failed to re-allocate string\n");
        return 1;
    }

    if (strcmp(s1, "Hello World my dear friends!") != 0) {
        printf("Strings do not match after re-allocation\n");
        return 1;
    }

    s1 = arena_realloc(arena, s1, 16);
    if (s1 == NULL) {
        printf("Failed to re-allocate(shrink) string\n");
        return 1;
    }

    // allocate something after shrinking to overwrite shrunk memory.
    char* s2 = arena_alloc_string(arena, "Allocate new string");
    if (s2 == NULL) {
        printf("Failed to allocate string\n");
        return 1;
    }

    if (strlen(s1) != 15) {
        printf("Expected shrunk string to be of len 15, got %ld\n", strlen(s1));
        return 1;
    }

    return 0;
}

int main(void) {
    // Create an arena of 1MB
    Arena* arena = arena_create(0);
    ASSERT(arena, "Failed to create arena.")

    // Allocate memory
    char* buffer1 = arena_alloc(arena, 1024);
    ASSERT(buffer1, "arena_alloc: Failed to allocate memory.")
    char* buffer2 = arena_alloc(arena, 512);
    ASSERT(buffer2, "arena_alloc: Failed to allocate memory.")

    strcpy(buffer1, "Hello from Arena Allocator!");
    printf("Allocated buffer 1: %s\n", buffer1);

    strcpy(buffer2, "Another buffer here.");
    printf("Allocated buffer 2: %s\n", buffer2);

    // print the address of the buffer 1 and buffer 2
    printf("Buffer 1: %p\n", (void*)buffer1);
    printf("Buffer 2: %p\n", (void*)buffer2);

    // Allocate a very big chunk to test arena expansion
    char* buffer3 = arena_alloc(arena, (size_t)(1024 * 1024 * 10));  // 10MB
    ASSERT(buffer3, "arena_alloc: Failed to allocate memory.")

    strcpy(buffer3, "Big buffer here.");

    // 16777216
    // 1048576
    printf("Buffer3: %p\n", (void*)buffer3);

    // Destroy the arena
    arena_destroy(arena);

    // Multithreaded example
    multithreaded_example();

    // Reusing arena chunks example
    reusing_arena_chunks_example();

    // test arena realloc
    Arena* arena2 = arena_create(8);
    ASSERT(arena2, "Failed to create arena.")

    char* buffer4 = arena_alloc(arena2, 12);
    ASSERT(buffer4, "arena_alloc: Failed to allocate memory.")

    strcpy(buffer4, "Allocator!");
    printf("Allocated buffer 4: %s at %p\n", buffer4, (void*)buffer4);

    // Create another arena and benchmark 10m allocations
    time_t start, end;
    time(&start);
    Arena* arena4 = arena_create((size_t)(1024 * 1024 * 10));  // 10MB
    ASSERT(arena4, "Failed to create arena.")

    for (int i = 0; i < 1000; i++) {
        char* str = arena_alloc(arena4, (size_t)(1024 * 1024));
        ASSERT(str, "arena_alloc: Failed to allocate memory.")
        // No need to free the memory, as it will be returned to the free list
        (void)str;
    }

    // Allocate a very big chunk to test arena expansion
    char* buffer6 = arena_alloc(arena4, (size_t)(1024 * 1024 * 10));  // 10MB
    ASSERT(buffer6, "arena_alloc: Failed to allocate memory.")

    strcpy(buffer6, "Big buffer here.");
    printf("Buffer6: %p\n", (void*)buffer6);

    arena_destroy(arena4);

    int status = test_arena_realloc();

    time(&end);
    printf("Time taken: %f seconds\n", difftime(end, start));
    return status;
}
