#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct queue_node
{
    struct queue_node* next;
    void* data;
} queue_node;

typedef struct
{
    queue_node* head;
    queue_node* tail;
    int size;

    pthread_mutex_t lock;
    pthread_cond_t cond;
} queue;

/**
 * Remove and return the first item from the queue.
 * Blocks if queue is empty until an item is available.
 * 
 * @param q Pointer to queue structure
 * @return Pointer to the dequeued data, NULL if queue is invalid
 */
void* queue_pop(queue* q);

/**
 * Add an item to the end of the queue.
 * 
 * @param q Pointer to queue structure
 * @param data Pointer to data to enqueue
 */
void queue_push(queue* q, void* data);

/**
 * Create a new empty queue.
 * 
 * @return Pointer to newly allocated queue structure
 */
queue* queue_create();

/**
 * Destroy queue and free all associated memory.
 * Does not free data stored in queue nodes.
 * 
 * @param q Pointer to queue structure to destroy
 */
void queue_destroy(queue* q);

#endif