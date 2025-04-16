#include "../include/arena.h"
#include "../include/macros.h"
#include "../include/thread.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_THREADS 4
#define STRESS_TEST_ITERATIONS 100000

// Helper function to print test results
void print_test_result(const char* test_name, int success) {
    printf("%-40s: %s\n", test_name, success ? "PASS" : "FAIL");
}

// Helper function to validate writing to a pointer
int validate_writing(void* ptr, size_t size) {
    if (!ptr)
        return 0;

    // Write a pattern to the memory
    memset(ptr, 0xAA, size);

    // Verify the pattern
    for (size_t i = 0; i < size; i++) {
        if (((char*)ptr)[i] != (char)0xAA) {
            return 0;  // Validation failed
        }
    }

    return 1;  // Validation passed
}

// Helper function to validate string allocation
int validate_string_allocation(char* str, const char* expected) {
    if (!str)
        return 0;

    // Verify the string content and length
    size_t length = strlen(expected);
    if (strlen(str) != length || strcmp(str, expected) != 0) {
        return 0;  // Validation failed
    }

    return 1;  // Validation passed
}

// Thread function for multithreaded testing
static void* thread_func(void* arg) {
    Arena* arena = (Arena*)arg;

    // Allocate and reallocate memory in the thread
    char* str = arena_alloc_string(arena, "Thread initial allocation");
    if (!str || !validate_writing(str, strlen("Thread initial allocation") + 1)) {
        printf("Thread %zu: Initial allocation failed\n", thread_self());
        return NULL;
    }

    // Reallocate to a larger size
    str = arena_realloc(arena, str, 50);
    if (!str || !validate_writing(str, 50)) {
        printf("Thread %zu: Reallocation failed\n", thread_self());
        return NULL;
    }
    strcpy(str, "Thread reallocated string");

    // Allocate another chunk
    int* numbers = arena_alloc(arena, sizeof(int) * 10);
    if (!numbers || !validate_writing(numbers, sizeof(int) * 10)) {
        printf("Thread %zu: Array allocation failed\n", thread_self());
        return NULL;
    }
    for (int i = 0; i < 10; i++) {
        numbers[i] = i;
    }

    // Reallocate the array to a smaller size
    numbers = arena_realloc(arena, numbers, sizeof(int) * 5);
    if (!numbers || !validate_writing(numbers, sizeof(int) * 5)) {
        printf("Thread %zu: Array reallocation failed\n", thread_self());
        return NULL;
    }

    return NULL;
}

// Test basic allocations
void test_basic_allocations(void) {
    Arena* arena = arena_create(1024);  // 1 KB arena
    ASSERT(arena);

    // Allocate a small chunk
    char* str      = arena_alloc_string(arena, "Hello, Arena!");
    int test1_pass = validate_string_allocation(str, "Hello, Arena!");
    print_test_result("Basic allocation (small chunk)", test1_pass);

    // Allocate a larger chunk
    int* numbers   = arena_alloc(arena, sizeof(int) * 100);
    int test2_pass = (numbers != NULL && validate_writing(numbers, sizeof(int) * 100));
    print_test_result("Basic allocation (large chunk)", test2_pass);

    // Allocate zero bytes (should return NULL)
    void* zero_alloc = arena_alloc(arena, 0);
    int test3_pass   = (zero_alloc == NULL);
    print_test_result("Allocation of zero bytes", test3_pass);

    arena_destroy(arena);
}

// Test reallocations
void test_reallocations(void) {
    Arena* arena = arena_create(1024);  // 1 KB arena
    ASSERT(arena);

    // Allocate a string and reallocate to a larger size
    char* str      = arena_alloc_string(arena, "Hello");
    str            = arena_realloc(arena, str, 20);
    int test1_pass = validate_string_allocation(str, "Hello");
    print_test_result("Reallocation to larger size", test1_pass);

    // Reallocate to a smaller size
    str            = arena_realloc(arena, str, 5);
    int test2_pass = (str != NULL && strncmp(str, "Hello", 4) == 0);
    print_test_result("Reallocation to smaller size", test2_pass);

    // Reallocate NULL (should act like alloc)
    char* new_str  = arena_realloc(arena, NULL, 10);
    int test3_pass = (new_str != NULL && validate_writing(new_str, 10));
    print_test_result("Reallocation of NULL pointer", test3_pass);

    // Reallocate to zero bytes (should free the memory)
    new_str        = arena_realloc(arena, new_str, 0);
    int test4_pass = (new_str == NULL);
    print_test_result("Reallocation to zero bytes", test4_pass);

    arena_destroy(arena);
}

// Test multithreaded allocations
void test_multithreaded_allocations(void) {
    Arena* arena = arena_create(1024 * 1024);  // 1 MB arena
    ASSERT(arena);

    Thread threads[NUM_THREADS] = {0};

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_create(&threads[i], thread_func, arena);
    }

    // Join threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_join(threads[i], NULL);
    }

    print_test_result("Multithreaded allocations", 1);  // Pass if no crashes
    arena_destroy(arena);
}

// Stress test the arena allocator
void stress_test_arena(void) {
    Arena* arena = arena_create(20 << 20);  // 20MB
    ASSERT(arena);

    for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        // Allocate a small chunk
        char* str = arena_alloc_string(arena, "Stress test");
        if (!str || !validate_writing(str, strlen("Stress test") + 1)) {
            printf("Stress test failed at iteration %d\n", i);
            ASSERT(str);
            arena_destroy(arena);
            return;
        }

        // Reallocate to a larger size
        str = arena_realloc(arena, str, 100);
        if (!str || !validate_writing(str, 100)) {
            printf("Stress test failed at iteration %d\n", i);
            ASSERT(str);
            arena_destroy(arena);
            return;
        }

        // Reallocate to a smaller size
        str = arena_realloc(arena, str, 10);
        if (!str || !validate_writing(str, 10)) {
            printf("Stress test failed at iteration %d\n", i);
            ASSERT(str);
            arena_destroy(arena);
            return;
        }
    }

    print_test_result("Stress test (alloc/realloc)", 1);  // Pass if no crashes
    arena_destroy(arena);
}

void test_arena_allocbatch(void) {
    Arena* arena = arena_create(1 << 20);
    ASSERT(arena);

    size_t sizes[] = {32, 64, 128};
    void* ptrs[3];
    size_t count = sizeof(sizes) / sizeof(sizes[0]);
    bool ret     = arena_alloc_batch(arena, sizes, count, ptrs);
    ASSERT(ret);

    strncpy(ptrs[0], "Hello World\n", 31);
    strncpy(ptrs[1], "Hello World\n", 63);
    strncpy(ptrs[2], "Hello World\n", 127);

    // ptrs[0], ptrs[1], and ptrs[2] now contain valid allocations.
    print_test_result("Arena Alloc Batch", 1);
    arena_destroy(arena);
}

int main(void) {
    // Run all tests
    test_basic_allocations();
    test_reallocations();
    test_arena_allocbatch();
    test_multithreaded_allocations();
    stress_test_arena();

    printf("All tests completed.\n");
    return 0;
}
