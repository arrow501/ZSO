#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

// Generic queue structure for storing pointers
typedef struct {
    void** items;          // Array of pointers
    int capacity;
    int size;
    int front;
    int rear;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} queue_t;

// Queue operations
void queue_init(queue_t* queue, int capacity);
void queue_push(queue_t* queue, void* item);
void* queue_pop(queue_t* queue);
bool queue_is_empty(queue_t* queue);
void queue_cleanup(queue_t* queue);

#endif // QUEUE_H