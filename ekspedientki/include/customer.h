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
 * Customer state in the shopping process
 * 
 * State transitions:
 * 1. CUSTOMER_WAITING_FOR_CLERK → CUSTOMER_REQUESTING_ITEM (when clerk signals ready)
 * 2. CUSTOMER_REQUESTING_ITEM → CUSTOMER_WAITING_FOR_ITEM (after requesting an item)
 * 3. CUSTOMER_WAITING_FOR_ITEM → CUSTOMER_PROCESSING_RECEIPT (when clerk provides item/receipt)
 * 4. CUSTOMER_PROCESSING_RECEIPT → CUSTOMER_REQUESTING_ITEM (if there are more items)
 * 5. CUSTOMER_PROCESSING_RECEIPT → CUSTOMER_PAYING (when receiving final receipt)
 * 6. CUSTOMER_PAYING → CUSTOMER_TRANSACTION_COMPLETE (after payment submitted)
 */
typedef enum {
    CUSTOMER_WAITING_FOR_CLERK,        // Waiting for clerk to be ready
    CUSTOMER_REQUESTING_ITEM,          // Customer is requesting an item
    CUSTOMER_WAITING_FOR_ITEM,         // Waiting for clerk to process item
    CUSTOMER_PROCESSING_RECEIPT,       // Customer is reviewing receipt
    CUSTOMER_PAYING,                   // Customer is making payment
    CUSTOMER_TRANSACTION_COMPLETE      // Transaction is complete
} customer_state_t;

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

    // Shopping process management
    int current_item_index;      // Index of current item being processed
    int current_item;            // ID of the current item
    customer_state_t state;      // Current state in the shopping process
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
