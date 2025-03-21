#include "clerk.h"
#include "customer.h"

/* Global Variables */
queue* clerk_queues[NUM_CLERKS];  // Array of queues, one per clerk

/**
 * Main function for the clerk thread.
 * Continuously processes customers from the queue until a SENTINEL_VALUE is received.
 * 
 * @param arg Pointer to a clerk_t structure
 * @return Always returns NULL
 */

 void* clerk_thread(void* arg) {
    clerk_t* self = (clerk_t*)arg;
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d has entered the shop\n", self->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif

    while (1) {
        // Pop customer from queue; this is blocking
        void* customer_ptr = queue_pop(self->customer_queue);
        
        // Check if this is the sentinel value signaling to stop
        if (customer_ptr == SENTINEL_VALUE) {
            break;
        }
        
        customer_t* c = (customer_t*)customer_ptr;
        
        #if ENABLE_ASSERTS
        assert(c != NULL);
        assert(c->id >= 0);
        assert(c->wallet > 0);
        assert(c->shopping_list != NULL);
        assert(c->shopping_list_size > 0);
        #endif
        
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Clerk is serving customer %d\n", c->id);
        pthread_mutex_unlock(&printf_mutex);
        #endif

        // Initialize customer for item-by-item processing
        pthread_mutex_lock(&c->mutex);
        c->current_item_index = 0;
        c->waiting_for_response = false;
        c->clerk_ready = true;  // Set this flag so customer knows clerk is ready
        
        // Signal customer we're ready to serve them
        pthread_cond_signal(&c->cond);
        
        // Create transaction
        transaction_t* t = (transaction_t*)malloc(sizeof(transaction_t));
        if (t == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }
        t->paid = 0;
        t->total = 0;
        t->items_size = 0;
        t->items = malloc(sizeof(int) * c->shopping_list_size); // Allocate max size
        
        if (t->items == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }
        
        // Allocate mutex and cond for assistant communication
        pthread_mutex_t* assistant_wait_mutex = malloc(sizeof(pthread_mutex_t));
        pthread_cond_t* assistant_wait_cond = malloc(sizeof(pthread_cond_t));
        
        if (!assistant_wait_mutex || !assistant_wait_cond) {
            fprintf(stderr, "Error: malloc failed for synchronization primitives\n");
            exit(1);
        }
        
        pthread_mutex_init(assistant_wait_mutex, NULL);
        pthread_cond_init(assistant_wait_cond, NULL);
        
        // Process items one by one
        while (c->current_item_index < c->shopping_list_size) {
            // Wait for customer to request an item
            while (!c->waiting_for_response) {
                pthread_cond_wait(&c->cond, &c->mutex);
            }
            
            #if ENABLE_PRINTING
            pthread_mutex_lock(&printf_mutex);
            printf("Clerk %d processing item request %d for customer %d\n", 
                  self->id, c->current_item, c->id);
            pthread_mutex_unlock(&printf_mutex);
            #endif
            
            // Process the requested item
            int product_id = c->current_item;
            bool in_stock = try_get_product(product_id);
            
            if (in_stock) {
                int price = get_product_price(product_id);
                t->total += price;
                t->items[t->items_size++] = product_id;
                
                // If this product needs assistant preparation
                if (product_needs_assistant(product_id)) {
                    #if ENABLE_PRINTING
                    pthread_mutex_lock(&printf_mutex);
                    printf("Clerk %d requests assistant for product %d\n", self->id, product_id);
                    pthread_mutex_unlock(&printf_mutex);
                    #endif
                    
                    // Use a stack variable for the completed flag instead of a heap variable
                    // This ensures it's valid throughout the function scope
                    int completed = 0;
                    
                    // Create a job for the assistant
                    assistant_job_t* job = (assistant_job_t*)malloc(sizeof(assistant_job_t));
                    if (job == NULL) {
                        fprintf(stderr, "Error: malloc failed\n");
                        exit(1);
                    }
                    
                    job->product_id = product_id;
                    job->clerk_id = self->id;
                    job->mutex = assistant_wait_mutex;
                    job->cond = assistant_wait_cond;
                    job->completed = &completed;
                    
                    // Add job to assistant queue
                    queue_push(assistant_queue, job);
                    
                    // Wait for assistant to complete
                    pthread_mutex_lock(assistant_wait_mutex);
                    while (!completed) {
                        #if ENABLE_PRINTING
                        pthread_mutex_lock(&printf_mutex);
                        printf("Clerk %d waiting for assistant to prepare product %d\n", 
                              self->id, product_id);
                        pthread_mutex_unlock(&printf_mutex);
                        #endif
                        pthread_cond_wait(assistant_wait_cond, assistant_wait_mutex);
                    }
                    pthread_mutex_unlock(assistant_wait_mutex);
                    
                    #if ENABLE_PRINTING
                    pthread_mutex_lock(&printf_mutex);
                    printf("Clerk %d received prepared product %d\n", self->id, product_id);
                    pthread_mutex_unlock(&printf_mutex);
                    #endif
                }
            }
            else {
                #if ENABLE_PRINTING
                pthread_mutex_lock(&printf_mutex);
                printf("Product %d out of stock for customer %d\n", product_id, c->id);
                pthread_mutex_unlock(&printf_mutex);
                #endif
            }
            
            // Signal customer we've processed this item
            c->waiting_for_response = false;
            pthread_cond_signal(&c->cond);
            
            // Wait for customer to acknowledge receipt
            while (c->current_item_index < c->shopping_list_size && 
                  !c->waiting_for_response) {
                pthread_cond_wait(&c->cond, &c->mutex);
            }
        }
        
        // Transaction complete, finalize
        c->receipt = t;
        
        #if ENABLE_PRINTING || ENABLE_ASSERTS
        int customer_wallet = c->wallet;
        #endif
        
        // Signal customer to pay
        c->waiting_for_response = false;
        pthread_cond_signal(&c->cond);
        
        // Wait for payment
        if (t->total > 0) {
            #if ENABLE_PRINTING
            pthread_mutex_lock(&printf_mutex);
            printf("Clerk is waiting for customer %d to pay\n", c->id);
            pthread_mutex_unlock(&printf_mutex);
            #endif
            
            while (t->paid < t->total) {
                pthread_cond_wait(&c->cond, &c->mutex);
            }
        } else {
            pthread_cond_wait(&c->cond, &c->mutex);
        }
        
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("total: %d\n", t->total);
        printf("receipt total: %d\n", t->total);
        printf("paid: %d\n", t->paid);
        printf("customer_wallet: %d\n", customer_wallet);
        printf("expected wallet: %d\n", customer_wallet - t->total);
        printf("actual wallet: %d\n", c->wallet);
        pthread_mutex_unlock(&printf_mutex);
        #endif

        #if ENABLE_ASSERTS
        assert(t->total >= 0); // check if the total is correct// check if the total is correct
        assert(t->paid == t->total); // check if the customer paid the correct amount
        assert(customer_wallet - t->total == c->wallet); // check if the customer paid the correct amount
        #endif

        // Update the cash register
        self->cash_register += t->paid;

        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Clerk has been paid by customer %d\n", c->id);
        pthread_mutex_unlock(&printf_mutex);
        #endif

        // Signal the customer that transaction is fully complete before releasing the mutex
        pthread_cond_signal(&c->cond);
        pthread_mutex_unlock(&c->mutex);

        // NOW we can safely clean up the assistant synchronization objects
        // Make sure we have the mutex lock before destroying it
        pthread_mutex_lock(assistant_wait_mutex);
        pthread_mutex_unlock(assistant_wait_mutex);
        pthread_mutex_destroy(assistant_wait_mutex);
        free(assistant_wait_mutex);
        
        pthread_cond_destroy(assistant_wait_cond);
        free(assistant_wait_cond);
    }

    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d has made %d dollars and is leaving the shop\n", self->id, self->cash_register);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    free(self);
    return NULL;
}
