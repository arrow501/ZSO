#include "customer.h"
#include "queue.h"
#include "parameters.h"
#include "shop.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>  // For nanosleep

// External references to global variables
extern pthread_mutex_t queue_mutex;
extern int customers_remaining;
extern queue* clerk_queues[];

// Global mutex for synchronized printing
pthread_mutex_t printf_mutex = PTHREAD_MUTEX_INITIALIZER;

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

    // Initialize customer status
    self->current_item_index = 0;
    self->waiting_for_response = false;
    self->clerk_ready = false;
    self->transaction_complete = 0;
    
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
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer %d is joining queue %d\n", self->id, shortest_queue_idx);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    queue_push(clerk_queues[shortest_queue_idx], self);
    
    // Begin shopping process
    pthread_mutex_lock(&self->mutex);
    
    // Wait for clerk to be ready to serve us
    while (!self->clerk_ready) {
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Customer %d is waiting for a clerk\n", self->id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        pthread_cond_wait(&self->cond, &self->mutex);
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer %d is now being served\n", self->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Request items one by one
    request_items(self);
    
    // Process payment
    process_payment(self);
    
    pthread_mutex_unlock(&self->mutex);

    // Update remaining customers count
    pthread_mutex_lock(&customers_mutex);
    customers_remaining--;
    
    // If all customers are done, signal clerks to stop
    if (customers_remaining == 0) {
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Last customer is leaving, signaling clerks to close shop\n");
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        // Add sentinel to all queues to signal clerks to stop
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
    
    // Signal that a customer has exited, allowing a new one to enter
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
        printf("Customer %d requesting item %d\n", customer->id, customer->current_item);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        // Signal the clerk we have a request
        customer->waiting_for_response = true;
        pthread_cond_signal(&customer->cond);
        
        // Wait for clerk to process our request
        while (customer->waiting_for_response) {
            #if ENABLE_PRINTING
            pthread_mutex_lock(&printf_mutex);
            printf("Customer %d waiting for clerk to process item %d\n", 
                  customer->id, customer->current_item);
            pthread_mutex_unlock(&printf_mutex);
            #endif
            
            pthread_cond_wait(&customer->cond, &customer->mutex);
        }
        
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Customer %d received response for item %d\n", 
               customer->id, customer->shopping_list[customer->current_item_index]);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        // Signal we're ready for the next item or for payment
        pthread_cond_signal(&customer->cond);
    }
}

/**
 * Processes the receipt and makes payment
 */
static void process_payment(customer_t* customer) {
    // Wait for receipt from clerk
    while (customer->receipt == NULL) {
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Customer %d waiting for receipt\n", customer->id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        pthread_cond_wait(&customer->cond, &customer->mutex);
    }

    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer %d received receipt for %d cents\n", 
          customer->id, customer->receipt->total);
    pthread_mutex_unlock(&printf_mutex);
    #endif

    #if ENABLE_ASSERTS
    // Verify receipt is valid
    assert(customer->receipt != NULL);
    assert(customer->receipt->total >= 0);
    #endif

    // Make payment
    customer->wallet -= customer->receipt->total;
    customer->receipt->paid = customer->receipt->total;

    #if ENABLE_ASSERTS
    // Verify payment was made correctly
    assert(customer->receipt->paid == customer->receipt->total);
    #endif

    // Signal the clerk that payment has been made
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer %d is paying %d cents\n", customer->id, customer->receipt->total);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    pthread_cond_signal(&customer->cond);

    // Wait for clerk to mark transaction as complete
    while (!customer->receipt->paid || !customer->transaction_complete) {
        pthread_cond_wait(&customer->cond, &customer->mutex);
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer %d payment processed successfully\n", customer->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
}

/**
 * Cleans up resources allocated for this customer
 */
static void cleanup_resources(customer_t* customer) {
    // We still check if transaction is complete before cleanup
    pthread_mutex_lock(&customer->mutex);
    if (!customer->transaction_complete) {
        // This shouldn't happen if our protocol is correct
        fprintf(stderr, "Warning: Customer %d attempting cleanup before transaction complete\n", 
                customer->id);
    }
    pthread_mutex_unlock(&customer->mutex);
    
    // Don't destroy mutex or condition variable here
    // They will be destroyed in shop.c after all threads join
    
    #if ENABLE_ASSERTS
    if (customer->receipt != NULL) {
        assert(customer->receipt->items != NULL || customer->receipt->items_size == 0);
    }
    #endif
    
    #if ENABLE_PRINTING
    int id = customer->id;
    #endif
    
    // Free shopping list
    free(customer->shopping_list);
    
    // Free receipt if it exists
    if (customer->receipt != NULL) {
        if (customer->receipt->items != NULL) {
            free(customer->receipt->items);
        }
        free(customer->receipt);
    }
    
    // Don't free the customer structure here
    // It will be freed in shop.c after thread joins
    customer = NULL;
        
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer %d has self destruct\n", id);  // Fixed extra comma in format string
    pthread_mutex_unlock(&printf_mutex);
    #endif
}

