#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define CHANNEL_BUFFER_SIZE 8

// Define the type-safe macros with custom name and type
#define DEFINE_CHANNEL(name, type)                                                                 \
    typedef struct {                                                                               \
        type buffer[CHANNEL_BUFFER_SIZE];                                                          \
        int read_index;                                                                            \
        int write_index;                                                                           \
        int count;                                                                                 \
        pthread_mutex_t mutex;                                                                     \
        sem_t empty;                                                                               \
        sem_t full;                                                                                \
        bool is_closed;                                                                            \
    } name##_t;                                                                                    \
                                                                                                   \
    name##_t* name##_create(void) {                                                                \
        name##_t* ch = (name##_t*)malloc(sizeof(name##_t));                                        \
        if (ch == NULL)                                                                            \
            return NULL;                                                                           \
        ch->read_index = 0;                                                                        \
        ch->write_index = 0;                                                                       \
        ch->count = 0;                                                                             \
        ch->is_closed = false;                                                                     \
        pthread_mutex_init(&ch->mutex, NULL);                                                      \
        sem_init(&ch->empty, 0, CHANNEL_BUFFER_SIZE);                                              \
        sem_init(&ch->full, 0, 0);                                                                 \
        return ch;                                                                                 \
    }                                                                                              \
                                                                                                   \
    bool name##_send(name##_t* ch, type data) {                                                    \
        if (ch->is_closed)                                                                         \
            return false;                                                                          \
        sem_wait(&ch->empty);                                                                      \
        pthread_mutex_lock(&ch->mutex);                                                            \
        if (ch->is_closed) {                                                                       \
            pthread_mutex_unlock(&ch->mutex);                                                      \
            sem_post(&ch->empty);                                                                  \
            return false;                                                                          \
        }                                                                                          \
        ch->buffer[ch->write_index] = data;                                                        \
        ch->write_index = (ch->write_index + 1) % CHANNEL_BUFFER_SIZE;                             \
        ch->count++;                                                                               \
        pthread_mutex_unlock(&ch->mutex);                                                          \
        sem_post(&ch->full);                                                                       \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    type name##_receive(name##_t* ch) {                                                            \
        sem_wait(&ch->full);                                                                       \
        pthread_mutex_lock(&ch->mutex);                                                            \
        if (ch->count == 0 && ch->is_closed) {                                                     \
            pthread_mutex_unlock(&ch->mutex);                                                      \
            sem_post(&ch->full);                                                                   \
            return (type){0}; /* Return default value of the type */                               \
        }                                                                                          \
        type data = ch->buffer[ch->read_index];                                                    \
        ch->read_index = (ch->read_index + 1) % CHANNEL_BUFFER_SIZE;                               \
        ch->count--;                                                                               \
        pthread_mutex_unlock(&ch->mutex);                                                          \
        sem_post(&ch->empty);                                                                      \
        return data;                                                                               \
    }                                                                                              \
                                                                                                   \
    void name##_close(name##_t* ch) {                                                              \
        pthread_mutex_lock(&ch->mutex);                                                            \
        ch->is_closed = true;                                                                      \
        pthread_mutex_unlock(&ch->mutex);                                                          \
    }                                                                                              \
                                                                                                   \
    void name##_destroy(name##_t* ch) {                                                            \
        pthread_mutex_destroy(&ch->mutex);                                                         \
        sem_destroy(&ch->empty);                                                                   \
        sem_destroy(&ch->full);                                                                    \
        free(ch);                                                                                  \
    }
