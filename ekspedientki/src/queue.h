#ifndef QUEUE_H
#define QUEUE_H

#include "shop.h"

// Assistant task structure
typedef struct assistant_task {
    int product_id;
    int clerk_id;
    bool is_complete;
    pthread_mutex_t mutex;
    pthread_cond_t completed;
} assistant_task_t;

// Client queue structure
typedef struct client_queue {
    client_t** clients;
    int capacity;
    int size;
    int front;
    int rear;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} client_queue_t;

// Task queue structure
typedef struct task_queue {
    assistant_task_t* tasks;
    int capacity;
    int size;
    int front;
    int rear;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
} task_queue_t;

// External queue variables
extern client_queue_t client_queue;
extern task_queue_t task_queue;

// Client queue operations
void client_queue_init(client_queue_t* queue, int capacity);
void client_queue_push(client_queue_t* queue, client_t* client);
client_t* client_queue_pop(client_queue_t* queue);
void client_queue_cleanup(client_queue_t* queue);

// Task queue operations
void task_queue_init(task_queue_t* queue, int capacity);
void task_queue_push(task_queue_t* queue, assistant_task_t task);
assistant_task_t task_queue_pop(task_queue_t* queue);
void task_queue_cleanup(task_queue_t* queue);

// Create assistant task
assistant_task_t create_assistant_task(int product_id, int clerk_id);

#endif // QUEUE_H