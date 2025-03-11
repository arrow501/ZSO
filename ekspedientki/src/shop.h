#ifndef SHOP_H
#define SHOP_H

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

// Constants
#define MAX_PRODUCTS 50
#define MAX_CLIENTS 100
#define NUM_CLERKS 3

// Product structure
typedef struct {
    int id;
    char* name;
    int price;
    int stock;
    bool needs_assistant;
} product_t;

// Client structure
typedef struct {
    int id;
    int* shopping_list;
    int list_size;
    int current_item;
    bool is_done;
} client_t;

// Assistant task structure
typedef struct {
    int product_id;
    int clerk_id;
    bool is_complete;
    pthread_mutex_t mutex;
    pthread_cond_t completed;
} assistant_task_t;

// Queues for clients and assistant tasks
extern queue_t client_queue;
extern queue_t task_queue;

// Function declarations
void* clerk_thread(void* arg);
void* assistant_thread(void* arg);

// Product-related functions
extern product_t products[MAX_PRODUCTS];
extern pthread_mutex_t inventory_mutex;
extern int num_products;

void initialize_products();
bool try_get_product(int product_id);
assistant_task_t* create_assistant_task(int product_id, int clerk_id);

#endif // SHOP_H