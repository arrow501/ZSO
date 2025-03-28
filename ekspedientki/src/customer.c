#include "customer.h"
#include "queue.h"
#include "parameters.h"
#include "shop.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// External references to global variables
extern pthread_mutex_t queue_mutex;
extern pthread_mutex_t customers_mutex;
extern int customers_remaining;
extern queue* clerk_queues[];

// Helper functions for condition checking
static inline bool is_clerk_ready(customer_t* customer) {
    return customer->state == CUSTOMER_WAITING_FOR_CLERK;
}

static inline bool is_item_processed(customer_t* customer) {
    return customer->state != CUSTOMER_WAITING_FOR_ITEM;
}

static inline bool is_receipt_ready(customer_t* customer) {
    return customer->receipt != NULL;
}

// Forward declarations of helper functions
static int find_shortest_queue(void);
static void request_items(customer_t* customer);
static void process_payment(customer_t* customer);
static void cleanup_resources(customer_t* customer);

/**
 * Main function for the customer thread.
 */
void* customer_thread(void* arg) {
    customer_t* self = (customer_t*)arg;

    // Initialize customer state
    self->current_item_index = 0;
    self->state = CUSTOMER_WAITING_FOR_CLERK;
    
    #if ENABLE_ASSERTS
    assert(self != NULL);
    assert(self->id >= 0);
    assert(self->wallet > 0);
    assert(self->shopping_list != NULL);
    assert(self->shopping_list_size > 0);
    #endif

    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer %d has entered the shop\n", self->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Find and join the shortest queue
    int shortest_queue_idx = find_shortest_queue();
    queue_push(clerk_queues[shortest_queue_idx], self);
    
    // Begin shopping process
    pthread_mutex_lock(&self->mutex);
    
    // Wait for clerk to be ready to serve us
    while (!is_clerk_ready(self)) {
        pthread_cond_wait(&self->cond, &self->mutex);
    }
    
    // Request items one by one
    request_items(self);
    
    // Process payment
    process_payment(self);
    
    pthread_mutex_unlock(&self->mutex);

    // Update remaining customers count and notify clerks if needed
    pthread_mutex_lock(&customers_mutex);
    customers_remaining--;
    if (customers_remaining == 0) {
        // All customers are done, add sentinel to all queues
        for (int i = 0; i < NUM_CLERKS; i++) {
            queue_push(clerk_queues[i], SENTINEL_VALUE);
        }
    }
    pthread_mutex_unlock(&customers_mutex);

    // Leave the shop
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer %d has left the shop\n", self->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif

    // Clean up resources
    cleanup_resources(self);
    
    // Signal that a customer has exited, allowing a new one to be created
    signal_customer_exit();

    return NULL;
}

/**
 * Finds the clerk queue with the fewest waiting customers
 */
static int find_shortest_queue(void) {
    pthread_mutex_lock(&queue_mutex);
    
    int shortest_queue_idx = 0;
    int shortest_length = queue_size(clerk_queues[0]);
    
    for (int i = 1; i < NUM_CLERKS; i++) {
        int current_length = queue_size(clerk_queues[i]);
        if (current_length < shortest_length) {
            shortest_length = current_length;
            shortest_queue_idx = i;
        }
    }
    
    pthread_mutex_unlock(&queue_mutex);
    return shortest_queue_idx;
}

/**
 * Requests each item on the customer's shopping list
 */
static void request_items(customer_t* customer) {
    for (customer->current_item_index = 0; 
         customer->current_item_index < customer->shopping_list_size; 
         customer->current_item_index++) {
         
        // Set the current item to request
        customer->current_item = customer->shopping_list[customer->current_item_index];
        
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Customer %d requesting item %d (%d/%d)\n", 
               customer->id, customer->current_item, 
               customer->current_item_index + 1, customer->shopping_list_size);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        // Signal the clerk we have a request
        customer->state = CUSTOMER_REQUESTING_ITEM;
        pthread_cond_signal(&customer->cond);
        
        // Now waiting for clerk to process our request
        customer->state = CUSTOMER_WAITING_FOR_ITEM;
        
        // Wait for clerk to process our request
        while (!is_item_processed(customer)) {
            pthread_cond_wait(&customer->cond, &customer->mutex);
        }
        
        // If this is not the last item, prepare for requesting the next one
        if (customer->current_item_index < customer->shopping_list_size - 1) {
            customer->state = CUSTOMER_REQUESTING_ITEM;
        }
        
        // Signal we're ready for the next item or for concluding the transaction
        pthread_cond_signal(&customer->cond);
    }
}

/**
 * Processes the receipt and makes payment
 */
static void process_payment(customer_t* customer) {
    // Set state to processing receipt
    customer->state = CUSTOMER_PROCESSING_RECEIPT;
    
    // Wait for receipt from clerk
    while (!is_receipt_ready(customer)) {
        pthread_cond_wait(&customer->cond, &customer->mutex);
    }

    #if ENABLE_ASSERTS
    assert(customer->receipt != NULL);
    assert(customer->receipt->total >= 0);
    #endif

    // Make payment
    customer->state = CUSTOMER_PAYING;
    customer->wallet -= customer->receipt->total;
    customer->receipt->paid = customer->receipt->total;

    #if ENABLE_ASSERTS
    assert(customer->receipt->paid == customer->receipt->total);
    #endif

    // Signal the clerk that payment has been made
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer %d is paying the clerk\n", customer->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    pthread_cond_signal(&customer->cond);

    // Wait for clerk to confirm receipt of payment
    customer->state = CUSTOMER_TRANSACTION_COMPLETE;
    pthread_cond_wait(&customer->cond, &customer->mutex);
}

/**
 * Cleans up resources allocated for this customer
 */
static void cleanup_resources(customer_t* customer) {
    #if ENABLE_ASSERTS
    assert(customer->receipt != NULL);
    assert(customer->receipt->items != NULL || customer->receipt->items_size == 0);
    #endif

    // Free allocated memory
    free(customer->receipt->items);
    free(customer->receipt);
    free(customer->shopping_list);
    
    // Destroy synchronization primitives
    pthread_mutex_destroy(&customer->mutex);
    pthread_cond_destroy(&customer->cond);
    
    // Finally, free the customer structure
    free(customer);
}

