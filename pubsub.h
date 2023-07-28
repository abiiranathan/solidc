#ifndef PUBSUB_H
#define PUBSUB_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>  // for the sleep() function

// Data structure to represent a reactive store
typedef struct reactive_store_t reactive_store_t;

// Data structure to represent a subscriber
typedef struct subscriber_t {
    char* name;
    void (*callback)(char*, char*, size_t);
    struct subscriber_t* next;
} subscriber_t;

// Allocate and initialize a new reactive store
reactive_store_t* reactive_store_init();

// Set the data in the reactive store and notify all subscribers
void reactive_store_set(reactive_store_t* store, const char* data, size_t size);

// Get the data from the reactive store.
// Blocks on pthread_cond_wait until some data is available.
void reactive_store_get(reactive_store_t* store, char* data, size_t* size);

// Add a new subscriber to the reactive store
void reactive_store_subscribe(reactive_store_t* store, subscriber_t* subscriber);

// Remove a subscriber from the reactive store
void reactive_store_unsubscribe(reactive_store_t* store, subscriber_t* subscriber);

// Notify all subscribers with the current data in the store
void reactive_store_notify(reactive_store_t* store);

#endif  // PUBSUB_H
