/**
 * @file test_allocator.c
 * @brief Test program for the thread-safe memory allocator
 */

#include "my_malloc.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NUM_THREADS       4
#define ALLOCS_PER_THREAD 1000
#define MAX_ALLOC_SIZE    8192

// Structure to track allocations
typedef struct allocation {
    void* ptr;
    size_t size;
    int value;
} allocation;

// Thread function state
typedef struct thread_state {
    int thread_id;
    allocation allocs[ALLOCS_PER_THREAD];
    int success_count;
    int failure_count;
} thread_state;

// Fill memory with a pattern based on the value
void fill_memory(void* ptr, size_t size, int value) {
    unsigned char* bytes = (unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        bytes[i] = (value + i) & 0xFF;
    }
}

// Verify memory contains the expected pattern
bool verify_memory(void* ptr, size_t size, int value) {
    unsigned char* bytes = (unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != ((value + i) & 0xFF)) {
            return false;
        }
    }
    return true;
}

// Thread function for concurrent allocations and frees
void* thread_func(void* arg) {
    thread_state* state = (thread_state*)arg;
    int id              = state->thread_id;

    // Seed the random number generator differently for each thread
    srand((unsigned int)time(NULL) + id);

    printf("Thread %d starting\n", id);

    // Phase 1: Perform allocations
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        // Random size between 1 and MAX_ALLOC_SIZE bytes
        size_t size = rand() % MAX_ALLOC_SIZE + 1;

        void* ptr = my_malloc(size);
        if (ptr) {
            state->allocs[i].ptr   = ptr;
            state->allocs[i].size  = size;
            state->allocs[i].value = rand();
            fill_memory(ptr, size, state->allocs[i].value);
            state->success_count++;
        } else {
            state->allocs[i].ptr = NULL;
            state->failure_count++;
        }

        // Occasionally do some frees to prevent running out of memory
        if (rand() % 4 == 0) {
            int idx = rand() % i;
            if (state->allocs[idx].ptr) {
                my_free(state->allocs[idx].ptr);
                state->allocs[idx].ptr = NULL;
            }
        }

        // Occasionally realloc
        if (rand() % 8 == 0 && i > 0) {
            int idx = rand() % i;
            if (state->allocs[idx].ptr) {
                size_t new_size = rand() % MAX_ALLOC_SIZE + 1;
                void* new_ptr   = my_realloc(state->allocs[idx].ptr, new_size);
                if (new_ptr) {
                    state->allocs[idx].ptr  = new_ptr;
                    state->allocs[idx].size = new_size;
                    // Only fill the new portion
                    fill_memory(new_ptr, new_size, state->allocs[idx].value);
                }
            }
        }
    }

    // Phase 2: Verify memory contents
    int verify_errors = 0;
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        if (state->allocs[i].ptr) {
            if (!verify_memory(state->allocs[i].ptr, state->allocs[i].size, state->allocs[i].value)) {
                verify_errors++;
            }
        }
    }

    // Phase 3: Free all allocations
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        if (state->allocs[i].ptr) {
            my_free(state->allocs[i].ptr);
            state->allocs[i].ptr = NULL;
        }
    }

    printf("Thread %d completed: %d successes, %d failures, %d verify errors\n", id, state->success_count,
           state->failure_count, verify_errors);

    return NULL;
}

void basic_test() {
    printf("=== Basic Functionality Test ===\n");

    // Test malloc
    int* numbers = (int*)my_malloc(10 * sizeof(int));
    assert(numbers != NULL);
    for (int i = 0; i < 10; i++) {
        numbers[i] = i;
    }

    // Verify values
    for (int i = 0; i < 10; i++) {
        assert(numbers[i] == i);
    }

    // Test realloc
    numbers = (int*)my_realloc(numbers, 20 * sizeof(int));
    assert(numbers != NULL);
    for (int i = 10; i < 20; i++) {
        numbers[i] = i;
    }

    // Verify all values after realloc
    for (int i = 0; i < 20; i++) {
        assert(numbers[i] == i);
    }

    // Test calloc
    char* buffer = (char*)my_calloc(100, sizeof(char));
    assert(buffer != NULL);
    for (int i = 0; i < 100; i++) {
        assert(buffer[i] == 0);  // Should be zero-initialized
    }

    // Test writing to calloc'd memory
    snprintf(buffer, 100, "Hello, thread-safe allocator!");
    assert(strcmp(buffer, "Hello, thread-safe allocator!") == 0);

    // Free the memory
    my_free(numbers);
    my_free(buffer);

    printf("Basic test passed!\n\n");
}

void edge_case_test() {
    printf("=== Edge Case Test ===\n");

    // Test malloc with size 0
    void* p1 = my_malloc(0);
    assert(p1 == NULL);

    // Test realloc with NULL pointer
    void* p2 = my_realloc(NULL, 100);
    assert(p2 != NULL);

    // Test realloc with size 0
    void* p3 = my_realloc(p2, 0);
    assert(p3 == NULL);  // Should free p2 and return NULL

    // Test large allocation
    void* p4 = my_malloc(1024 * 1024 * 10);  // 10MB
    assert(p4 != NULL);
    my_free(p4);

    // Test calloc overflow protection
    void* p5 = my_calloc(SIZE_MAX, 2);
    assert(p5 == NULL);

    printf("Edge case test passed!\n\n");
}

int main() {
    // Run basic functionality tests
    basic_test();

    // Run edge case tests
    edge_case_test();

    // Show initial memory state
    print_memory_state();

    // Multi-threaded test
    printf("=== Multi-threaded Test ===\n");

    pthread_t threads[NUM_THREADS];
    thread_state states[NUM_THREADS];

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        states[i].thread_id     = i;
        states[i].success_count = 0;
        states[i].failure_count = 0;
        pthread_create(&threads[i], NULL, thread_func, &states[i]);
    }

    // Wait for threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Display final memory state
    print_memory_state();

    // Calculate statistics
    int total_success = 0;
    int total_failure = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        total_success += states[i].success_count;
        total_failure += states[i].failure_count;
    }

    printf("Multi-threaded test complete\n");
    printf("Total successful allocations: %d\n", total_success);
    printf("Total failed allocations: %d\n", total_failure);

    return 0;
}
