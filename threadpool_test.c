/*
gcc -Wall -Werror -pedantic -fsanitize=address threadpool*.c -lpthread -lcurl
*/

#include "threadpool.h"
#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print_task(void* arg) {
    int num = *(int*)arg;
    printf("Task %d executed by thread %lu\n", num, pthread_self());
    sleep(1);
}

typedef struct {
    int* tasks;
    int num_tasks;
    int partial_sum;
} PartialSumData;

void sum_task(void* arg) {
    PartialSumData* data = (PartialSumData*)arg;

    int local_sum = 0;
    for (int i = 0; i < data->num_tasks; i++) {
        local_sum += data->tasks[i];
    }

    data->partial_sum = local_sum;
}

void concurrentSumExample() {
#define NUM_TASKS 10
#define NUM_THREADS 4

    ThreadPool* pool = threadpool_create(NUM_THREADS);
    int tasks[NUM_TASKS];

    for (int i = 0; i < NUM_TASKS; i++) {
        tasks[i] = i + 1;  // First 10 natural numbers
    }

    PartialSumData partial_sum_data[NUM_THREADS];
    int tasks_per_thread = NUM_TASKS / NUM_THREADS;
    int extra_tasks      = NUM_TASKS % NUM_THREADS;

    int task_index = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        int num_tasks = tasks_per_thread + (i < extra_tasks ? 1 : 0);

        partial_sum_data[i].tasks     = &tasks[task_index];
        partial_sum_data[i].num_tasks = num_tasks;

        task_index += num_tasks;

        threadpool_add_task(pool, sum_task, &partial_sum_data[i]);
    }

    threadpool_wait(pool);

    int total_sum = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        total_sum += partial_sum_data[i].partial_sum;
    }

    printf("Sum: %d\n", total_sum);

    threadpool_destroy(pool);
}

void simpleExample() {
    ThreadPool* pool = threadpool_create(4);
    int* tasks[10];

    for (int i = 0; i < 10; i++) {
        int* arg = (int*)malloc(sizeof(int));
        *arg     = i;
        tasks[i] = arg;
        threadpool_add_task(pool, print_task, arg);
    }

    threadpool_wait(pool);

    // Free all tasks
    for (int i = 0; i < 10; i++) {
        free(tasks[i]);
    }
    threadpool_destroy(pool);
}

typedef struct {
    char* url;
    char* content;
} PageData;

void fetch_page(void* arg) {
    PageData* data = (PageData*)arg;
    CURL* curl     = curl_easy_init();
    if (curl) {
        CURLcode res;

        curl_easy_setopt(curl, CURLOPT_URL, data->url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data->content);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "Failed to fetch %s: %s\n", data->url,
                    curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    } else {
        fprintf(stderr, "Failed to initialize libcurl\n");
    }
}

void webscraperExample() {
#define NUM_THREADS 4
#define NUM_URLS 5
    ThreadPool* pool = threadpool_create(NUM_THREADS);

    PageData page_data[NUM_URLS] = {{"https://example.com", NULL},
                                    {"https://www.google.com", NULL},
                                    {"https://www.github.com", NULL},
                                    {"https://www.openai.com", NULL},
                                    {"https://www.wikipedia.org", NULL}};

    for (int i = 0; i < NUM_URLS; i++) {
        threadpool_add_task(pool, fetch_page, &page_data[i]);
    }

    threadpool_wait(pool);

    for (int i = 0; i < NUM_URLS; i++) {
        if (page_data[i].content) {
            printf("URL: %s\nContent Length: %zu\n", page_data[i].url,
                   strlen(page_data[i].content));
            // Print or process the fetched content as desired
            free(page_data[i].content);
        }
    }
    threadpool_destroy(pool);
}

int main() {
    // simpleExample();
    concurrentSumExample();
    webscraperExample();
    return 0;
}
