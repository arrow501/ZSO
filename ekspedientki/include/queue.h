#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>


typedef struct
{
    struct queue_node* head;
    struct queue_node* tail;
    int size;
    
    pthread_mutex_t lock;
    pthread_cond_t cond;
} queue;

typedef struct
{
    struct queue_node* prev;
    void* data;
} queue_node;

void* queue_pop(queue* q);

void queue_push(queue* q, void* data);

queue* queue_create();

void queue_destroy(queue* q);

#endif