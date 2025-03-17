#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Queue Module
 * 
 * This module implements a thread-safe queue data structure
 * used throughout the shop simulation for managing work items
 * and coordinating between different threads.
 */

/**
 * Represents a node in the queue linked list.
 */
typedef struct queue_node
{
    struct queue_node* next; // Pointer to next node
    void* data;              // Pointer to stored data
} queue_node;

/**
 * Thread-safe queue structure with mutex and condition variable.
 */
typedef struct
{
    queue_node* head;        // Pointer to first node
    queue_node* tail;        // Pointer to last node
    int size;               // Number of items in queue

    pthread_mutex_t lock;    // Mutex for thread safety
    pthread_cond_t cond;     // Condition variable for signaling
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

/**
 * Get the number of items in the queue.
 * 
 * @param q Pointer to queue structure
 * @return Number of items in the queue
 */
int queue_size(queue* q);

#endif