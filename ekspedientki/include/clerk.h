#ifndef CLERK_H
#define CLERK_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "transaction.h"
#include "customer.h"
#include "queue.h"
#include "parameters.h"
#include "product.h"
#include "assistant.h"

/**
 * Clerk Module
 * 
 * This module provides functionality for shop clerks who serve customers.
 * Each clerk:
 * 1. Takes customers from their queue
 * 2. Processes their shopping list
 * 3. Requests assistant help for special products
 * 4. Provides a receipt to the customer
 * 5. Collects payment
 */

/* Global Variables */
extern queue* clerk_queues[NUM_CLERKS];  // Array of queues, one per clerk

/**
 * Represents a clerk in the shop.
 * Each clerk has their own queue and cash register.
 */
typedef struct clerk_t {
    int id;                  // Unique ID for the clerk
    int cash_register;       // Amount of money collected
    queue* customer_queue;   // Queue of customers waiting for this clerk
} clerk_t;

/**
 * Main function for the clerk thread.
 * Processes customers from the queue until receiving a SENTINEL_VALUE.
 * 
 * @param arg Pointer to a clerk_t structure
 * @return Always returns NULL
 */
void* clerk_thread(void* arg);

#endif
