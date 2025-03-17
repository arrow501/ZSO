#include "customer.h"
#include "queue.h"
#include "parameters.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// External references to global variables
extern pthread_mutex_t queue_mutex;
extern pthread_mutex_t customers_mutex;
extern int customers_remaining;
extern queue* clerk_queues[];

void* customer_thread(void* arg) {
    customer_t* self = (customer_t*)arg;
    
    #if ENABLE_ASSERTS
    assert(self != NULL);
    assert(self->id >= 0);
    assert(self->wallet > 0);
    assert(self->shopping_list != NULL);
    assert(self->shopping_list_size > 0);
    #endif

    #if ENABLE_PRINTING
    printf("Customer %d has entered the shop\n", self->id);
    #endif
    
    // Find the shortest queue
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
    
    // Join the shortest queue
    queue_push(clerk_queues[shortest_queue_idx], self);
    pthread_mutex_unlock(&queue_mutex);

    pthread_mutex_lock(&self->mutex);

    // Wait for the clerk to serve me
    while (self->reciecpt == NULL) {
        pthread_cond_wait(&self->cond, &self->mutex);
    }

    #if ENABLE_ASSERTS
    // Verify receipt is valid
    assert(self->reciecpt != NULL);
    assert(self->reciecpt->total >= 0);
    // I decided Customers can go into debt
    // // Ensure customer has enough money
    // assert(self->wallet >= self->reciecpt->total);
    #endif

    // Check the receipt and pay
    self->wallet -= self->reciecpt->total;
    self->reciecpt->paid = self->reciecpt->total;

    #if ENABLE_ASSERTS
    // Verify payment was made correctly
    assert(self->reciecpt->paid == self->reciecpt->total);
    #endif

    // Signal the clerk that I have paid
    #if ENABLE_PRINTING
    printf("Customer %d is paying the clerk\n", self->id);
    #endif
    pthread_cond_signal(&self->cond);

    // Wait for clerk to confirm receipt of payment
    pthread_cond_wait(&self->cond, &self->mutex);

    pthread_mutex_unlock(&self->mutex);

    // Update remaining customers count and signal clerks if needed
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
    printf("Customer %d has left the shop\n", self->id);
    #endif

    #if ENABLE_ASSERTS
    assert(self->reciecpt != NULL);
    assert(self->reciecpt->items != NULL || self->reciecpt->items_size == 0);
    #endif

    // Clean up my properties - only after transaction is fully complete
    free(self->reciecpt->items);
    free(self->reciecpt);
    free(self->shopping_list);
    pthread_mutex_destroy(&self->mutex);
    pthread_cond_destroy(&self->cond);
    free(self);

    return NULL;
}

