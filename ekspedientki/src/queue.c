#include "queue.h"

void queue_init(queue_t* queue, int capacity) {
    queue->items = (void**)malloc(capacity * sizeof(void*));
    queue->capacity = capacity;
    queue->size = 0;
    queue->front = 0;
    queue->rear = -1;
    
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
}

void queue_push(queue_t* queue, void* item) {
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->size < queue->capacity) {
        queue->rear = (queue->rear + 1) % queue->capacity;
        queue->items[queue->rear] = item;
        queue->size++;
        
        // Signal that the queue is not empty anymore
        pthread_cond_signal(&queue->not_empty);
    }
    
    pthread_mutex_unlock(&queue->mutex);
}

void* queue_pop(queue_t* queue) {
    pthread_mutex_lock(&queue->mutex);
    
    // Wait until the queue has something in it
    while (queue->size == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    void* item = queue->items[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    
    pthread_mutex_unlock(&queue->mutex);
    return item;
}

bool queue_is_empty(queue_t* queue) {
    pthread_mutex_lock(&queue->mutex);
    bool empty = (queue->size == 0);
    pthread_mutex_unlock(&queue->mutex);
    return empty;
}

void queue_cleanup(queue_t* queue) {
    // Free the queue array but not the items themselves
    free(queue->items);
    
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
}