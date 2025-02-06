#include <solidc/threadpool.h>
#include <stdio.h>
#include "include/threadpool.h"

void task(void* arg) {
    printf("Got: %d\n", *(int*)arg);
}

int main(void) {
    ThreadPool* pool = threadpool_create(8);

    int arr[1024];
    for (int i = 0; i < 1024; i++) {
        arr[i] = i;
        threadpool_submit(pool, task, &arr[i]);
    }

    threadpool_destroy(pool);
}
