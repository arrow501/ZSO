/* customer.h - Customer thread interface */
#ifndef CUSTOMER_H
#define CUSTOMER_H

#include "shop.h"

/**
 * Initialize a customer
 * 
 * @param customer Pointer to customer structure to initialize
 * @param id Customer identifier
 * @return 0 on success, non-zero on failure
 */
int customer_init(customer_t* customer, int id);

/**
 * Start a customer thread
 * 
 * @param customer Pointer to customer structure
 * @param shop Pointer to shop structure
 * @return 0 on success, non-zero on failure
 */
int customer_start(customer_t* customer, shop_t* shop);

/**
 * Clean up customer resources
 * 
 * @param customer Pointer to customer structure to clean up
 */
void customer_cleanup(customer_t* customer);

/**
 * Generate a random shopping list for a customer
 * 
 * @param customer Pointer to customer structure
 * @param min_items Minimum number of items in list
 * @param max_items Maximum number of items in list
 */
void customer_generate_shopping_list(customer_t* customer, int min_items, int max_items);

/**
 * Find the clerk with shortest queue
 * 
 * @param shop Pointer to shop structure
 * @return Index of clerk with shortest queue
 */
int customer_find_shortest_queue(shop_t* shop);

/**
 * Process customer payment and generate receipt
 * 
 * @param customer Pointer to customer structure
 * @param total_price Total price of purchased items
 * @param items Array of receipt items
 * @param num_items Number of receipt items
 * @return Pointer to receipt structure, or NULL on failure
 */
receipt_t* customer_process_payment(customer_t* customer, int total_price, receipt_item_t* items, int num_items);

/**
 * Get current customer state
 * 
 * @param customer Pointer to customer structure
 * @return Current customer state
 */
customer_state_t customer_get_state(customer_t* customer);

/**
 * Set customer state
 * 
 * @param customer Pointer to customer structure
 * @param state New customer state
 */
void customer_set_state(customer_t* customer, customer_state_t state);

/**
 * Wait for customer state change
 * 
 * @param customer Pointer to customer structure
 * @param state State to wait for
 * @return true if state changed to specified state, false otherwise
 */
bool customer_wait_for_state(customer_t* customer, customer_state_t state);

/* Customer thread entry point */
void* customer_thread(void* arg);

#endif /* CUSTOMER_H */