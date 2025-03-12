/* queue.h - Thread-safe queue implementation */
#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdbool.h>

/* Generic queue node */
typedef struct queue_node {
    void* data;               /* Pointer to node data */
    struct queue_node* next;  /* Next node in queue */
} queue_node_t;

/* Thread-safe queue structure */
typedef struct {
    queue_node_t* head;       /* First node in queue */
    queue_node_t* tail;       /* Last node in queue */
    int length;               /* Current queue length */
    pthread_mutex_t mutex;    /* Mutex to protect queue access */
    pthread_cond_t not_empty; /* CV to signal when queue has items */
} queue_t;

/**
 * Initialize a new queue
 * 
 * @param queue Pointer to queue structure to initialize
 * @return 0 on success, non-zero on failure
 */
int queue_init(queue_t* queue);

/**
 * Clean up queue resources
 * 
 * @param queue Pointer to queue structure to clean up
 * @param free_data_func Function to free data (NULL to skip freeing data)
 */
void queue_cleanup(queue_t* queue, void (*free_data_func)(void*));

/**
 * Enqueue an item
 * 
 * @param queue Pointer to queue structure
 * @param data Pointer to data to enqueue (ownership transfers to queue)
 * @return 0 on success, non-zero on failure
 */
int queue_enqueue(queue_t* queue, void* data);

/**
 * Dequeue an item
 * 
 * @param queue Pointer to queue structure
 * @param wait Whether to wait if queue is empty
 * @return Pointer to dequeued data (ownership transfers to caller), or NULL if empty
 */
void* queue_dequeue(queue_t* queue, bool wait);

/**
 * Check if queue is empty
 * 
 * @param queue Pointer to queue structure
 * @return true if empty, false otherwise
 */
bool queue_is_empty(queue_t* queue);

/**
 * Get current queue length
 * 
 * @param queue Pointer to queue structure
 * @return Current number of items in queue
 */
int queue_length(queue_t* queue);

#endif /* QUEUE_H */