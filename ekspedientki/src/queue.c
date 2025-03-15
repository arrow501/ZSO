#include "queue.h"

void* queue_pop(queue* q) {
    if (q == NULL) return NULL;

    pthread_mutex_lock(&q->lock);
    while (q->size == 0) {
        pthread_cond_wait(&q->cond, &q->lock);
    }

    queue_node* node = q->head;
    void* data = node->data;
    
    q->head = node->next;  // Fix: move to next node
    if (--q->size == 0) {
        q->tail = NULL;
    }

    pthread_mutex_unlock(&q->lock);
    free(node);
    return data;
}

void queue_push(queue* q, void* data) {
    queue_node* node = malloc(sizeof(queue_node));
    if (node == NULL) { // Fix: check if malloc failed
        fprintf(stderr, "Error: malloc failed\n");
        exit(1);
    }
    
    node->data = data;
    node->next = NULL;

    pthread_mutex_lock(&q->lock);
    if (q->tail != NULL) {
        q->tail->next = node;  // Fix: current tail points to new node
    }
    q->tail = node;
    if (q->size == 0) {
        q->head = node;
    }
    q->size++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

queue* queue_create() {
    queue* q = malloc(sizeof(queue));
    if (q == NULL) {
        fprintf(stderr, "Error: malloc failed\n");
        exit(1);
    }
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    return q;
}

void queue_destroy(queue* q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
    while (q->head != NULL) {
        queue_node* node = q->head;
        q->head = node->next;
        free(node->data);
        free(node);
    }
    free(q);
}