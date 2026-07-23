#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/cache.h"

#define POOL_SIZE 100000

typedef struct {
    char key[64];
    uint8_t value[1024];
} kv_pair_t;

kv_pair_t* kv_pool = NULL;
cache_t* global_cache = NULL;
size_t read_ratio = 90;
size_t value_size = 128;
size_t ops_per_thread = 0;

typedef struct {
    unsigned int seed;
    long operations_completed;
} thread_arg_t;

void* thread_worker(void* arg) {
    thread_arg_t* t_arg = (thread_arg_t*)arg;
    unsigned int seed = t_arg->seed;
    uint8_t* read_buf = malloc(value_size + 128UL);

    for (size_t i = 0; i < ops_per_thread; i++) {
        int idx = rand_r(&seed) % POOL_SIZE;
        size_t op = (size_t)rand_r(&seed) % 100;

        if (op < read_ratio) {
            size_t out_len;
            cache_get(global_cache, kv_pool[idx].key, strlen(kv_pool[idx].key), read_buf, value_size + 128, &out_len);
        } else {
            cache_put(global_cache, kv_pool[idx].key, strlen(kv_pool[idx].key), kv_pool[idx].value, value_size, 300);
        }
        t_arg->operations_completed++;
    }

    free(read_buf);
    return NULL;
}

void run_benchmark(const char* name, size_t ratio, size_t val_sz, size_t num_threads, long total_ops) {
    read_ratio = ratio;
    value_size = val_sz;
    ops_per_thread = (size_t)total_ops / num_threads;

    cache_config_t config = {.capacity_per_shard = 2048,  // 2048 * 256 = ~524k slots total capacity
                             .slab_blocks_per_shard = 1024,
                             .default_ttl_sec = 300};

    global_cache = cache_create(&config);

    // Warm up
    for (int i = 0; i < (int)(POOL_SIZE * 0.7); i++) {
        cache_put(global_cache, kv_pool[i].key, strlen(kv_pool[i].key), kv_pool[i].value, value_size, 300);
    }

    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    thread_arg_t* args = malloc(num_threads * sizeof(thread_arg_t));

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t i = 0; i < num_threads; i++) {
        args[i].seed = (unsigned int)((size_t)time(NULL) ^ i);
        args[i].operations_completed = 0;
        pthread_create(&threads[i], NULL, thread_worker, &args[i]);
    }

    for (size_t i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double throughput = (double)total_ops / elapsed;

    printf("%-30s: Completed %ld ops in %.4f seconds (%.2f ops/sec)\n", name, total_ops, elapsed, throughput);

    free(threads);
    free(args);
    cache_destroy(global_cache);
}

int main() {
    printf("Initializing key-value pool...\n");
    kv_pool = malloc(POOL_SIZE * sizeof(kv_pair_t));
    for (int i = 0; i < POOL_SIZE; i++) {
        snprintf(kv_pool[i].key, sizeof(kv_pool[i].key), "key-string-padding-format-%010d", i);
        for (int j = 0; j < 1024; j++) {
            kv_pool[i].value[j] = (uint8_t)(rand() % 256);
        }
    }

    size_t threads = 8;
    long total_ops = 5000000;  // 5 Million ops

    printf("Starting C Cache Benchmarks (%zu threads, %ld total ops):\n\n", threads, total_ops);
    run_benchmark("ReadHeavy_90_10_InlineVal", 90, 128, threads, total_ops);
    run_benchmark("Balanced_50_50_InlineVal", 50, 128, threads, total_ops);
    run_benchmark("ReadHeavy_90_10_SlabVal", 90, 512, threads, total_ops);

    free(kv_pool);
    return 0;
}
