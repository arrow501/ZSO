/* queue.c - Thread-safe queue implementation */
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

int queue_init(queue_t* queue) {
    if (!queue) {
        return -1;
    }
    
    /* Initialize queue structure */
    queue->head = NULL;
    queue->tail = NULL;
    queue->length = 0;
    
    /* Initialize synchronization primitives */
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        return -1;
    }
    
    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        return -1;
    }
    
    return 0;
}

void queue_cleanup(queue_t* queue, void (*free_data_func)(void*)) {
    if (!queue) {
        return;
    }
    
    /* Acquire mutex to ensure safe cleanup */
    pthread_mutex_lock(&queue->mutex);
    
    /* Free all nodes in the queue */
    queue_node_t* current = queue->head;
    while (current) {
        queue_node_t* next = current->next;
        
        /* Free the node data if function provided */
        if (free_data_func && current->data) {
            free_data_func(current->data);
        }
        
        /* Free the node */
        free(current);
        current = next;
    }
    
    /* Reset queue state */
    queue->head = NULL;
    queue->tail = NULL;
    queue->length = 0;
    
    /* Release mutex before destroying it */
    pthread_mutex_unlock(&queue->mutex);
    
    /* Destroy synchronization primitives */
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);
}

int queue_enqueue(queue_t* queue, void* data) {
    if (!queue) {
        return -1;
    }
    
    /* Create a new node for the data */
    queue_node_t* node = (queue_node_t*)malloc(sizeof(queue_node_t));
    if (!node) {
        return -1;
    }
    
    node->data = data;
    node->next = NULL;
    
    /* Acquire lock to safely modify queue */
    pthread_mutex_lock(&queue->mutex);
    
    /* Add node to queue */
    if (queue->tail) {
        /* Queue not empty, append to tail */
        queue->tail->next = node;
    } else {
        /* Queue empty, set as head */
        queue->head = node;
    }
    
    /* Update tail and length */
    queue->tail = node;
    queue->length++;
    
    /* Signal that queue is not empty */
    pthread_cond_signal(&queue->not_empty);
    
    #ifdef DEBUG
    printf("Queue: Item enqueued, length now %d\n", queue->length);
    #endif
    
    /* Release lock */
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

void* queue_dequeue(queue_t* queue, bool wait) {
    if (!queue) {
        return NULL;
    }
    
    void* data = NULL;
    
    /* Acquire lock to safely modify queue */
    pthread_mutex_lock(&queue->mutex);
    
    /* Wait if requested and queue is empty */
    while (queue->head == NULL && wait) {
        #ifdef DEBUG
        printf("Queue: Waiting for item\n");
        #endif
        
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    /* Get the first node if available */
    if (queue->head) {
        queue_node_t* node = queue->head;
        
        /* Update head */
        queue->head = node->next;
        
        /* If queue is now empty, update tail */
        if (!queue->head) {
            queue->tail = NULL;
        }
        
        /* Decrement length */
        queue->length--;
        
        /* Extract data */
        data = node->data;
        
        /* Free the node (not the data) */
        free(node);
        
        #ifdef DEBUG
        printf("Queue: Item dequeued, length now %d\n", queue->length);
        #endif
    }
    
    /* Release lock */
    pthread_mutex_unlock(&queue->mutex);
    
    return data;
}

bool queue_is_empty(queue_t* queue) {
    if (!queue) {
        return true;
    }
    
    bool is_empty;
    
    /* Acquire lock to check queue */
    pthread_mutex_lock(&queue->mutex);
    
    is_empty = (queue->head == NULL);
    
    /* Release lock */
    pthread_mutex_unlock(&queue->mutex);
    
    return is_empty;
}

int queue_length(queue_t* queue) {
    if (!queue) {
        return 0;
    }
    
    int length;
    
    /* Acquire lock to check queue */
    pthread_mutex_lock(&queue->mutex);
    
    length = queue->length;
    
    /* Release lock */
    pthread_mutex_unlock(&queue->mutex);
    
    return length;
}