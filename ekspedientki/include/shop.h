/* shop.h - Core shop simulation types and constants */
#ifndef SHOP_H
#define SHOP_H

#include <pthread.h>
#include <stdbool.h>
#include "queue.h"
#include "product.h"

/* Configuration */
#define NUM_CLERKS 3
#define NUM_ASSISTANTS 1
#define MAX_SHOPPING_LIST 15
#define SIMULATION_TIME_SEC 5

/* Customer states */
typedef enum {
    CUSTOMER_ENTERING,    /* Customer just entered the shop */
    CUSTOMER_QUEUED,      /* Customer is in queue waiting for a clerk */
    CUSTOMER_SHOPPING,    /* Customer is being served by a clerk */
    CUSTOMER_PAYING,      /* Customer is paying for items */
    CUSTOMER_EXITING,     /* Customer has paid and is leaving */
    CUSTOMER_DONE         /* Customer has left the shop */
} customer_state_t;

/* Shopping list item */
typedef struct {
    int product_id;       /* ID of product to purchase */
    bool fulfilled;       /* Whether item was fulfilled */
} shopping_list_item_t;

/* Receipt item */
typedef struct {
    int product_id;       /* ID of purchased product */
    int price;            /* Price of purchased product */
} receipt_item_t;

/* Receipt structure */
typedef struct {
    receipt_item_t* items;    /* Array of purchased items */
    int num_items;            /* Number of items in receipt */
    int total_price;          /* Total price of all items */
} receipt_t;

/* Customer structure */
typedef struct {
    int id;                           /* Customer ID */
    pthread_t thread;                 /* Customer thread */
    customer_state_t state;           /* Current customer state */
    shopping_list_item_t shopping_list[MAX_SHOPPING_LIST]; /* Shopping list */
    int shopping_list_size;           /* Number of items in shopping list */
    int current_item;                 /* Index of item being processed */
    int selected_clerk;               /* ID of selected clerk */
    receipt_t* receipt;               /* Receipt after shopping */
    pthread_mutex_t mutex;            /* Mutex for customer state access */
    pthread_cond_t state_changed;     /* CV for state changes */
} customer_t;

/* Assistant task types */
typedef enum {
    TASK_PREPARE_PRODUCT  /* Prepare a product for a customer */
} assistant_task_type_t;

/* Assistant task structure */
typedef struct {
    assistant_task_type_t type;       /* Type of task */
    int product_id;                   /* Product to prepare */
    int clerk_id;                     /* Requesting clerk ID */
    bool completed;                   /* Whether task is complete */
    pthread_mutex_t mutex;            /* Mutex for task state access */
    pthread_cond_t completed_cond;    /* CV for task completion */
} assistant_task_t;

/* Clerk structure */
typedef struct {
    int id;                  /* Clerk ID */
    pthread_t thread;        /* Clerk thread */
    queue_t customer_queue;  /* Queue of waiting customers */
    int total_sales;         /* Total sales processed */
    int items_processed;     /* Number of items processed */
    bool active;             /* Whether clerk is active */
} clerk_t;

/* Assistant structure */
typedef struct {
    pthread_t thread;        /* Assistant thread */
    queue_t task_queue;      /* Queue of pending tasks */
    bool active;             /* Whether assistant is active */
} assistant_t;

/* Shop structure */
typedef struct {
    clerk_t clerks[NUM_CLERKS];           /* Array of clerks */
    assistant_t assistant;                /* Shop assistant */
    queue_t task_queue;                   /* Queue for assistant tasks */
    int customers_processed;              /* Number of customers served */
    bool shutdown;                        /* Shop shutdown flag */
    pthread_mutex_t mutex;                /* Mutex for shop state access */
    pthread_cond_t shutdown_complete;     /* CV for shutdown completion */
} shop_t;

/* Main entry point for the simulation */
int projekt_zso(void);

#endif /* SHOP_H */