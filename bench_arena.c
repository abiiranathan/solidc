#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "include/arena.h"

// Structure to pass data to threads
typedef struct {
    Arena* arena;
    size_t size;
    size_t n;
    double* elapsed_time;
} ThreadData;

// Function to run in each thread for arena_alloc
void* thread_arena_alloc(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t i = 0; i < data->n; i++) {
        void* ptr = arena_alloc(data->arena, data->size);
        if (ptr == NULL) {
            fprintf(stderr, "arena_alloc failed in thread\n");
            pthread_exit((void*)-1);  // Indicate failure
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    *(data->elapsed_time) = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    return NULL;
}

// Function to run in each thread for malloc
void* thread_malloc(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t i = 0; i < data->n; i++) {
        void* ptr = malloc(data->size);
        if (ptr == NULL) {
            fprintf(stderr, "malloc failed in thread, errno: %d\n", errno);
            pthread_exit((void*)-1);  // Indicate failure
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    *(data->elapsed_time) = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <size> <n> <threads>\n", argv[0]);
        return 1;
    }

    size_t size = atoi(argv[1]);
    size_t n = atoi(argv[2]);
    int num_threads = atoi(argv[3]);

    if (num_threads <= 0) {
        fprintf(stderr, "Number of threads must be positive.\n");
        return 1;
    }

    pthread_t threads[num_threads];
    ThreadData thread_data[num_threads];
    double elapsed_times[num_threads];

    // Benchmark arena_alloc
    Arena* arena = arena_create(ARENA_DEFAULT_CHUNKSIZE * 100);
    if (arena == NULL) {
        fprintf(stderr, "arena_create failed\n");
        return 1;
    }

    for (int i = 0; i < num_threads; i++) {
        thread_data[i].arena = arena;
        thread_data[i].size = size;
        thread_data[i].n = n / num_threads;  // Distribute allocations among threads
        thread_data[i].elapsed_time = &elapsed_times[i];
        if (pthread_create(&threads[i], NULL, thread_arena_alloc, &thread_data[i]) != 0) {
            perror("pthread_create failed");
            return 1;
        }
    }

    for (int i = 0; i < num_threads; i++) {
        void* result;
        if (pthread_join(threads[i], &result) != 0) {
            perror("pthread_join failed");
            return 1;
        }
        if ((long)result == -1) {
            fprintf(stderr, "Thread %d failed.\n", i);
            return 1;
        }
    }

    double total_elapsed_arena = 0;
    for (int i = 0; i < num_threads; i++) {
        total_elapsed_arena += elapsed_times[i];
    }
    printf("arena_alloc (multithreaded): %.6f ms\n", total_elapsed_arena * 1e3);
    arena_destroy(arena);

    // Benchmark malloc
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].arena = NULL;  //Not used for malloc
        thread_data[i].size = size;
        thread_data[i].n = n / num_threads;
        thread_data[i].elapsed_time = &elapsed_times[i];
        if (pthread_create(&threads[i], NULL, thread_malloc, &thread_data[i]) != 0) {
            perror("pthread_create failed");
            return 1;
        }
    }
    for (int i = 0; i < num_threads; i++) {
        void* result;
        if (pthread_join(threads[i], &result) != 0) {
            perror("pthread_join failed");
            return 1;
        }
        if ((long)result == -1) {
            fprintf(stderr, "Thread %d failed.\n", i);
            return 1;
        }
    }

    double total_elapsed_malloc = 0;
    for (int i = 0; i < num_threads; i++) {
        total_elapsed_malloc += elapsed_times[i];
    }
    printf("malloc (multithreaded): %.6f ms\n", total_elapsed_malloc * 1e3);

    return 0;
}
