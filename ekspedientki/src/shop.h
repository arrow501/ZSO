#ifndef SHOP_H
#define SHOP_H

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

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

// Forward declarations for queue structures
typedef struct client_queue client_queue_t;
typedef struct task_queue task_queue_t;
typedef struct assistant_task assistant_task_t;

// Function declarations for clerk and assistant threads
void* clerk_thread(void* arg);
void* assistant_thread(void* arg);

// Global variables declaration (defined in main.c)
extern product_t products[MAX_PRODUCTS];
extern pthread_mutex_t inventory_mutex;
extern int num_products;

// Product functions
void initialize_products();
bool product_in_stock(int product_id);
bool product_needs_assistant(int product_id);

#endif // SHOP_H