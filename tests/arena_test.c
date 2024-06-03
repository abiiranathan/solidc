#include "../include/arena.h"
#include "../include/thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARENA_DEFAULT_SIZE (1024 * 1024)
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
    // Create an arena of 1MB
    Arena* arena = arena_create(ARENA_DEFAULT_SIZE, ARENA_DEFAULT_ALIGNMENT);
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
    // Create an arena of 1MB
    Arena* arena = arena_create(124, ARENA_DEFAULT_SIZE);
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

int main(void) {
    // Create an arena of 1MB
    Arena* arena = arena_create(ARENA_DEFAULT_SIZE, ARENA_DEFAULT_ALIGNMENT);
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
    char* buffer3 = arena_alloc(arena, 1024 * 1024 * 10);  // 10MB
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
    Arena* arena2 = arena_create(8, ARENA_DEFAULT_ALIGNMENT);
    ASSERT(arena2, "Failed to create arena.")

    char* buffer4 = arena_alloc(arena2, 12);
    ASSERT(buffer4, "arena_alloc: Failed to allocate memory.")

    strcpy(buffer4, "Allocator!");
    printf("Allocated buffer 4: %s at %p\n", buffer4, (void*)buffer4);

    // Create another arena and benchmark 10m allocations
    time_t start, end;
    time(&start);
    Arena* arena4 = arena_create(1024 * 1024 * 10, ARENA_DEFAULT_ALIGNMENT);  // 10MB
    ASSERT(arena4, "Failed to create arena.")

    for (int i = 0; i < 10000000; i++) {
        char* str = arena_alloc(arena4, 1024 * 1024);
        ASSERT(str, "arena_alloc: Failed to allocate memory.")
        // No need to free the memory, as it will be returned to the free list
        (void)str;
    }

    // Allocate a very big chunk to test arena expansion
    char* buffer6 = arena_alloc(arena4, 1024 * 1024 * 10);  // 10MB
    ASSERT(buffer6, "arena_alloc: Failed to allocate memory.")

    strcpy(buffer6, "Big buffer here.");
    printf("Buffer6: %p\n", (void*)buffer6);

    arena_destroy(arena4);
    time(&end);

    printf("Time taken: %f seconds\n", difftime(end, start));
    return 0;
}
