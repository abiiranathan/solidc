#include "../include/arena.h"
#include "../include/macros.h"
#include "../include/thread.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_THREADS            16
#define STRESS_TEST_ITERATIONS 100000

// Helper function to print test results
void print_test_result(const char* test_name, int success) {
    printf("%-40s: %s\n", test_name, success ? "PASS" : "FAIL");
}

// Helper function to validate writing to a pointer
int validate_writing(void* ptr, size_t size) {
    if (!ptr) return 0;

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
    if (!str) return 0;

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

    const char s[] = "Thread initial allocation";
    char* str      = arena_strdup(arena, s);
    if (!str || !validate_writing(str, sizeof(s))) {
        printf("Thread %lu: arena_alloc_string failed\n", (unsigned long)thread_self());
        return NULL;
    }

    // Allocate another chunk
    int* numbers = arena_alloc(arena, sizeof(int) * 10);
    if (!numbers || !validate_writing(numbers, sizeof(int) * 10)) {
        printf("Thread %lu: Array allocation failed\n", (unsigned long)thread_self());
        return NULL;
    }

    for (int i = 0; i < 10; i++) {
        numbers[i] = i;
    }

    return NULL;
}

// Test basic allocations
void test_basic_allocations() {
    Arena* arena = arena_create(0);  // use default
    ASSERT(arena);

    // Allocate a small chunk
    char* str      = arena_strdup(arena, "Hello, Arena!");
    int test1_pass = validate_string_allocation(str, "Hello, Arena!");
    print_test_result("Basic allocation (small chunk)", test1_pass);

    // Test arena_strdupn
    char bytes[]     = {'S', 'O', 'L', 'I', 'D'};  // not-null terminated
    char* bytes_copy = arena_strdupn(arena, bytes, sizeof(bytes));
    ASSERT(bytes_copy);
    ASSERT_STR_EQ(bytes_copy, "SOLID");

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

// Test multithreaded allocations
void test_multithreaded_allocations() {
    Arena* arena = arena_create(1024UL * 1024);  // 1 MB arena
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
void stress_test_arena() {
    Arena* arena = arena_create(20 << 20);  // 20MB
    ASSERT(arena);

    for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        // Allocate a small chunk
        char* str = arena_strdup(arena, "Stress test");
        if (!str || !validate_writing(str, strlen("Stress test") + 1)) {
            printf("Stress test failed at iteration %d\n", i);
            ASSERT(str);
            arena_destroy(arena);
            return;
        }
    }

    print_test_result("Stress test (alloc/realloc)", 1);  // Pass if no crashes
    arena_destroy(arena);
}

void test_arena_allocbatch() {
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

void test_arena_alloc_array() {
    Arena* arena = arena_create(sizeof(int) * 10);
    ASSERT(arena);

    int* arr = ARENA_ALLOC_ARRAY(arena, int, 10);
    ASSERT(arr);

    print_test_result("Arena Alloc Array", 1);
    arena_destroy(arena);
}

int main() {
    // Run all tests
    test_basic_allocations();
    test_arena_allocbatch();
    test_arena_alloc_array();
    test_multithreaded_allocations();
    stress_test_arena();

    // Test some macros
    TIME_BLOCK_MS("for loop duration", {
        for (volatile int i = 0; i < 10000000; ++i) {}
    });

    int arr[] = {1, 2, 3, 4};
    FOR_EACH_ARRAY(n, arr) {
        printf("n=%d\n", *n);
    }

    printf("All tests completed.\n");

    return 0;
}
