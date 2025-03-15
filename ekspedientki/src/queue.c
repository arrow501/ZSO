#include "../include/queue.h"
#include <stddef.h>
#include <stdio.h>


void* queue_pop(queue* q) {
    // critical section
    pthread_mutex_lock(&q->lock);
    while (q->size == 0) {
        pthread_cond_wait(&q->cond, &q->lock);
    }

    queue_node* node = q->head;
    q->head = node->prev;
    q->size--;
    pthread_mutex_unlock(&q->lock);
    // end critical section

    void* data = node->data;
    free(node);
    return data;
}

void queue_push(queue* q, void* data) {
    queue_node* node = (queue_node*)malloc(sizeof(queue_node));
    if (node == NULL) {
        printf("Error: malloc failed\n");
        exit(1);
    }
    
    node->data = data;

    // critical section
    pthread_mutex_lock(&q->lock);
    node->prev = q->tail;
    q->tail = node;
    if (q->size == 0) {
        q->head = node;
    }
    q->size++;
    pthread_mutex_unlock(&q->lock);
    // end critical section

}

queue* queue_create() {
    // allocate memory and initialize the queue
    queue* q = (queue*)malloc(sizeof(queue));
     if (q == NULL) {
        fprintf(stderr, "Error: malloc failed\n");
        exit(1);
    }

    q->head = NULL;
    q->tail = NULL;
    q->size = 0;

    // initialize the synchronization primitives
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);

    return q;
}

void queue_destroy(queue* q) {
    // TODO: fix this
    while (q->size > 0) {
        queue_pop(q); // this is probably not safe
    }
    free(q);
}