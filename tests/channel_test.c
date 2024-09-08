#include "../include/channel.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DEFINE_CHANNEL(channel_str, char*);
DEFINE_CHANNEL(channel_int, int);

// Receive numbers from the channel in a separate thread
// because sending beyond the available slots will block
// the main thread.
// This should be called before sending numbers to the channel.
void* receive_numbers(void* arg) {
    channel_int_t* ch = (channel_int_t*)arg;
    for (int i = 0; i < 1000; i++) {
        int data = channel_int_receive(ch);
        assert(data == i);
    }

    return NULL;
}

void test_sync_send_receive(void) {
    channel_str_t* ch = channel_str_create();

    // Send data to the channel
    channel_str_send(ch, "Hello");
    channel_str_send(ch, "World");

    // Receive data from the channel
    assert(strcmp((char*)channel_str_receive(ch), "Hello") == 0);
    assert(strcmp((char*)channel_str_receive(ch), "World") == 0);

    // Close the channel
    channel_str_close(ch);

    // Destroy the channel
    channel_str_destroy(ch);
}

void test_async_send_receive(void) {
    channel_int_t* ch = channel_int_create();

    // create a thread to receive numbers
    pthread_t thread;
    pthread_create(&thread, NULL, receive_numbers, ch);

    for (int i = 0; i < 1000; i++) {
        channel_int_send(ch, i);
    }

    // Wait for the thread to finish
    pthread_join(thread, NULL);

    // Close the channel
    channel_int_close(ch);

    // Destroy the channel
    channel_int_destroy(ch);
}

int main() {
    test_sync_send_receive();
    test_async_send_receive();
    return 0;
}
