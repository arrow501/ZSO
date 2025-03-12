/* clerk.h - Clerk thread interface */
#ifndef CLERK_H
#define CLERK_H

#include "shop.h"

/**
 * Initialize a clerk
 * 
 * @param clerk Pointer to clerk structure to initialize
 * @param id Clerk identifier
 * @return 0 on success, non-zero on failure
 */
int clerk_init(clerk_t* clerk, int id);

/**
 * Start a clerk thread
 * 
 * @param clerk Pointer to clerk structure
 * @param shop Pointer to shop structure
 * @return 0 on success, non-zero on failure
 */
int clerk_start(clerk_t* clerk, shop_t* shop);

/**
 * Clean up clerk resources
 * 
 * @param clerk Pointer to clerk structure to clean up
 */
void clerk_cleanup(clerk_t* clerk);

/**
 * Stop a clerk's processing
 * 
 * @param clerk Pointer to clerk structure to stop
 */
void clerk_stop(clerk_t* clerk);

/**
 * Add a customer to clerk's queue
 * 
 * @param clerk Pointer to clerk structure
 * @param customer Pointer to customer to add
 * @return 0 on success, non-zero on failure
 */
int clerk_add_customer(clerk_t* clerk, customer_t* customer);

/**
 * Process a customer's shopping list
 * 
 * @param clerk Pointer to clerk structure
 * @param customer Pointer to customer to process
 * @param shop Pointer to shop structure
 * @return Receipt pointer on success, NULL on failure
 */
receipt_t* clerk_process_customer(clerk_t* clerk, customer_t* customer, shop_t* shop);

/**
 * Request product preparation from assistant
 * 
 * @param clerk_id Clerk identifier
 * @param product_id Product to prepare
 * @param shop Pointer to shop structure
 * @return true if preparation request was successful, false otherwise
 */
bool clerk_request_product_preparation(int clerk_id, int product_id, shop_t* shop);

/**
 * Get current queue length for a clerk
 * 
 * @param clerk Pointer to clerk structure
 * @return Current number of customers in queue
 */
int clerk_queue_length(clerk_t* clerk);

/* Clerk thread entry point */
void* clerk_thread(void* arg);

#endif /* CLERK_H */