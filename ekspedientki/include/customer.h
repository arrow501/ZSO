#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <pthread.h>
#include "transaction.h"
#include <stdbool.h>

/**
 * Customer Module
 * 
 * This module defines the customer entity and its behavior in the shop
 * simulation. Each customer enters the shop, selects goods, and completes
 * a purchase transaction with a clerk.
 */

/**
 * Represents a customer shopping in the store.
 */
typedef struct customer_t {
    struct customer_t* myself;   // Self-reference for thread safety
    int id;                      // Unique customer identifier
    int wallet;                  // Customer's money in cents
    int* shopping_list;          // Array of product IDs to purchase
    int shopping_list_size;      // Number of items in shopping list

    transaction_t* receipt;      // Transaction receipt from clerk

    pthread_cond_t cond;         // Condition variable for synchronization
    pthread_mutex_t mutex;       // Mutex for thread safety

    // New fields for item-by-item processing
    int current_item_index;
    int current_item;
    bool waiting_for_response;
    bool clerk_ready;
} customer_t;

/**
 * Main function for the customer thread.
 * Implements the customer's shopping behavior.
 * 
 * @param arg Pointer to customer_t structure
 * @return Always returns NULL
 */
void* customer_thread(void* arg);

/**
 * Global mutex for synchronizing printf calls.
 */
extern pthread_mutex_t printf_mutex;

#endif /* CUSTOMER_H */
