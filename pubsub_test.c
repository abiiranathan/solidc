#include "pubsub.h"
#include <stdlib.h>

// Initialize the reactive store
reactive_store_t* store;

// Subscriber thread function that waits for updates and prints the data
void* subscriber(void* arg) {
    subscriber_t* subscriber = (subscriber_t*)arg;
    char data[256];
    size_t size;
    while (1) {
        reactive_store_get(store, data, &size);
        subscriber->callback(subscriber->name, data, size);
    }
    return NULL;
}

// Publisher thread function that updates the reactive store with random data
void* publisher(void* arg) {
    char data[] = "Hello, world!";
    time_t start_time;
    time(&start_time);

    while (1) {
        reactive_store_set(store, data, strlen(data));
        reactive_store_notify(store);
        sleep(1);
        if (time(NULL) >= start_time + 10) {
            exit(0);
        }
    }
    return NULL;
}

void print_data(char* name, char* data, size_t size) {
    printf("Data received by subscriber %s: %d bytes: %s\n", name, (int)size, data);
}

int main() {
    // Initialize the reactive store
    store = reactive_store_init();
    if (!store) {
        return EXIT_FAILURE;
    }

    // Initialize the subscribers
    subscriber_t subscriber1 = {.name = "sub1", .callback = print_data};
    subscriber_t subscriber2 = {.name = "sub2", .callback = print_data};

    // Start the publisher thread
    printf("Start subscribers\n");
    // Subscribe the subscribers
    reactive_store_subscribe(store, &subscriber1);
    reactive_store_subscribe(store, &subscriber2);

    // Start the publisher thread
    printf("Start publisher thread\n");
    pthread_t publisher_thread;
    pthread_create(&publisher_thread, NULL, publisher, NULL);

    // Record the start time of the publisher
    printf("Record start time\n");
    time_t start_time;
    time(&start_time);
    printf("%lu\n", start_time);

    // Unsubscribe the first subscriber after 5 seconds
    sleep(5);
    printf("Unsubscribing subscriber1\n");
    reactive_store_unsubscribe(store, &subscriber1);

    // Wait for the publisher thread to finish
    pthread_join(publisher_thread, NULL);

    // Calculate the duration of the publisher
    time_t end_time;
    time(&end_time);

    double duration = difftime(end_time, start_time);
    printf("Publisher ran for %.2f seconds\n", duration);
    return 0;
}
