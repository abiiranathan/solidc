// gcc -Wall -Werror -pedantic -fsanitize=address threadpool_scraper.c threadpool.c -lpthread -lcurl

#include <curl/curl.h>
#include <curl/easy.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threadpool.h"

#define NUM_THREADS 4
#define NUM_URLS 5
#define MAX_CONTENT_LENGTH 1024  // Maximum content length per page

typedef struct {
    char* url;
    char content[MAX_CONTENT_LENGTH];
    size_t content_length;
} PageData;

size_t write_callback(void* contents, size_t size, size_t nmemb,
                      void* user_data) {
    PageData* data    = (PageData*)user_data;
    size_t total_size = size * nmemb;

    // Copy the fetched content to the PageData buffer
    size_t copy_size = total_size;
    if (copy_size > MAX_CONTENT_LENGTH - 1) {
        copy_size = MAX_CONTENT_LENGTH - 1;
    }

    memcpy(data->content, contents, copy_size);
    data->content[copy_size] = '\0';  // Null-terminate the content string

    data->content_length = copy_size;
    return total_size;
}

void fetch_page(void* arg) {
    PageData* data = (PageData*)arg;
    CURL* curl     = curl_easy_init();
    if (curl) {
        CURLcode res;

        curl_easy_setopt(curl, CURLOPT_URL, data->url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

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

int main() {
    ThreadPool* pool             = threadpool_create(NUM_THREADS);
    PageData page_data[NUM_URLS] = {{"https://example.com", "", 0},
                                    {"https://www.google.com", "", 0},
                                    {"https://www.github.com", "", 0},
                                    {"https://www.openai.com", "", 0},
                                    {"https://www.wikipedia.org", "", 0}};

    for (int i = 0; i < NUM_URLS; i++) {
        threadpool_add_task(pool, fetch_page, &page_data[i]);
    }

    threadpool_wait(pool);

    for (int i = 0; i < NUM_URLS; i++) {
        printf("URL: %s\nContent Length: %zu\n\nContent: %s\n\n\n",
               page_data[i].url, page_data[i].content_length,
               page_data[i].content);
    }

    threadpool_destroy(pool);
    return 0;
}
