#include "../include/threadpool.h"
#include <stdio.h>

void task_fn(void* arg) {
    int* value = (int*)arg;
    printf("Task: %d\n", *value);
}

int main(void) {
    ThreadPool* pool = threadpool_create();
    if (!pool) {
        printf("Failed to create thread pool.\n");
        return 1;
    }

    int values[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    for (int i = 0; i < 8; ++i) {
        threadpool_add_task(pool, task_fn, &values[i]);
    }

    threadpool_wait(pool);
    threadpool_destroy(pool);
    return 0;
}