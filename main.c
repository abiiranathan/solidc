#include <stdio.h>
#include "include/threadpool.h"

#define TASKS 10000

void task(void* arg) {
    printf("Got: %d\n", *(int*)arg);
}

int main(void) {
    ThreadPool* pool = threadpool_create(8);

    int arr[TASKS];
    for (int i = 0; i < TASKS; i++) {
        arr[i] = i;
        if (threadpool_submit(pool, task, &arr[i]) != 0) {
            printf("Threadpool full\n");
        }
    }

    threadpool_destroy(pool);
}
