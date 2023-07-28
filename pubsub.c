#include "pubsub.h"
#include <stdlib.h>

// Data structure to represent a reactive store
typedef struct reactive_store_t {
    char* data;
    size_t size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    subscriber_t* subscribers;  // linked list of subscribers
} reactive_store_t;

// Allocate and initialize a new reactive store
reactive_store_t* reactive_store_init() {
    reactive_store_t* store = malloc(sizeof(reactive_store_t));
    if (store) {
        store->data = NULL;
        store->size = 0;
        pthread_mutex_init(&store->mutex, NULL);
        pthread_cond_init(&store->cond, NULL);
    }
    return store;
}

// Set the data in the reactive store and notify all subscribers
void reactive_store_set(reactive_store_t* store, const char* data, size_t size) {
    pthread_mutex_lock(&store->mutex);
    if (store->data) {
        free(store->data);
    }

    store->data = malloc(size);
    if (store->data == NULL) {
        printf("memory allocation failed\n");
        pthread_cond_broadcast(&store->cond);
        pthread_mutex_unlock(&store->mutex);
        return;
    }

    memcpy(store->data, data, size);
    store->size = size;
    pthread_cond_broadcast(&store->cond);
    pthread_mutex_unlock(&store->mutex);
}

// Get the data from the reactive store.
// Blocks on pthread_cond_wait until some data is available.
void reactive_store_get(reactive_store_t* store, char* data, size_t* size) {
    pthread_mutex_lock(&store->mutex);
    while (!store->data) {
        pthread_cond_wait(&store->cond, &store->mutex);
    }
    memcpy(data, store->data, store->size);
    *size = store->size;
    pthread_mutex_unlock(&store->mutex);
}

// Add a new subscriber to the reactive store
// Adds subscriber to the linked list
void reactive_store_subscribe(reactive_store_t* store, subscriber_t* subscriber) {
    pthread_mutex_lock(&store->mutex);
    subscriber->next = store->subscribers;
    store->subscribers = subscriber;
    pthread_mutex_unlock(&store->mutex);
}

// Remove a subscriber from the reactive store
void reactive_store_unsubscribe(reactive_store_t* store, subscriber_t* subscriber) {
    pthread_mutex_lock(&store->mutex);
    subscriber_t* prev = NULL;
    subscriber_t* curr = store->subscribers;
    while (curr) {
        if (curr == subscriber) {
            if (prev) {
                prev->next = curr->next;
            } else {
                store->subscribers = curr->next;
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&store->mutex);
}

// Notify all subscribers with the current data in the store
void reactive_store_notify(reactive_store_t* store) {
    pthread_mutex_lock(&store->mutex);
    subscriber_t* curr = store->subscribers;
    while (curr) {
        curr->callback(curr->name, store->data, store->size);
        curr = curr->next;
    }
    pthread_mutex_unlock(&store->mutex);
}
