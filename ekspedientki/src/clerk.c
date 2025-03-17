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

        // allocate maximum space for the purchased items
        int* purchaed_items = (int*)malloc(sizeof(int) * c->shopping_list_size);
        if (purchaed_items == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }

        // ring up the customer
        int total = 0, item_count = 0;

        // Keep track of special products that need assistant
        int special_products_count = 0;
        
        // Allocate mutex and cond on the heap to avoid premature destruction
        pthread_mutex_t* assistant_wait_mutex = NULL;
        pthread_cond_t* assistant_wait_cond = NULL;
        
        if (c->shopping_list_size > 0) {
            assistant_wait_mutex = malloc(sizeof(pthread_mutex_t));
            assistant_wait_cond = malloc(sizeof(pthread_cond_t));
            
            if (!assistant_wait_mutex || !assistant_wait_cond) {
                fprintf(stderr, "Error: malloc failed for synchronization primitives\n");
                exit(1);
            }
            
            pthread_mutex_init(assistant_wait_mutex, NULL);
            pthread_cond_init(assistant_wait_cond, NULL);
        }
        
        int* special_products_completed = NULL;

        // First count how many special products we have
        for (int i = 0; i < c->shopping_list_size; i++) {
            int product_id = c->shopping_list[i];
            if (product_needs_assistant(product_id)) {
                special_products_count++;
            }
        }

        // Allocate tracking array if we have special products
        if (special_products_count > 0) {
            special_products_completed = (int*)calloc(special_products_count, sizeof(int));
            if (special_products_completed == NULL) {
                fprintf(stderr, "Error: calloc failed\n");
                exit(1);
            }
        }

        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Clerk %d is ringing up customer %d\n", self->id, c->id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        int special_index = 0;
        for (int i = 0; i < c->shopping_list_size; i++) {
            int product_id = c->shopping_list[i];
            
            #if ENABLE_ASSERTS
            assert(product_id >= 0 && product_id < MAX_PRODUCTS);
            #endif

            bool in_stock = try_get_product(product_id);
            if (in_stock) {
                int price = get_product_price(product_id);
                total += price;
                purchaed_items[item_count++] = product_id;
                
                #if ENABLE_ASSERTS
                assert(price > 0);
                #endif
                
                // If this product needs assistant preparation
                if (product_needs_assistant(product_id)) {
                    #if ENABLE_PRINTING
                    pthread_mutex_lock(&printf_mutex);
                    printf("Clerk %d requests assistant for product %d\n", self->id, product_id);
                    pthread_mutex_unlock(&printf_mutex);
                    #endif
                    
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
                    job->completed = &special_products_completed[special_index++];
                    
                    // Add job to assistant queue
                    queue_push(assistant_queue, job);
                }
            }
            // Add a debug print here:
            else {
                #if ENABLE_PRINTING
                pthread_mutex_lock(&printf_mutex);
                printf("Product %d out of stock for customer %d\n", product_id, c->id);
                pthread_mutex_unlock(&printf_mutex);
                #endif
            }
        }

        // Wait for all special products to be prepared by the assistant
        if (special_products_count > 0) {
            pthread_mutex_lock(assistant_wait_mutex);
            
            // Check if all special products are completed
            int all_completed = 0;
            while (!all_completed) {
                all_completed = 1;
                for (int i = 0; i < special_products_count; i++) {
                    if (!special_products_completed[i]) {
                        all_completed = 0;
                        break;
                    }
                }
                
                if (!all_completed) {
                    #if ENABLE_PRINTING
                    pthread_mutex_lock(&printf_mutex);
                    printf("Clerk %d waiting for assistant to prepare products\n", self->id);
                    pthread_mutex_unlock(&printf_mutex);
                    #endif
                    pthread_cond_wait(assistant_wait_cond, assistant_wait_mutex);
                }
            }
            
            pthread_mutex_unlock(assistant_wait_mutex);
            #if ENABLE_PRINTING
            pthread_mutex_lock(&printf_mutex);
            printf("Clerk %d has received all prepared products\n", self->id);
            pthread_mutex_unlock(&printf_mutex);
            #endif
            
            // Free the tracking array - it's safe to free this now
            free(special_products_completed);
            
            // Note: We'll free the mutex and condvar later after the transaction is complete
            // This ensures the assistant thread has fully completed its work
        }

        // create a transaction
        transaction_t* t = (transaction_t*)malloc(sizeof(transaction_t));
        if (t == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }
        t->paid = 0;
        t->total = total;
        t->items = malloc(sizeof(int) * item_count);
        t->items_size = item_count;

        if (t->items == NULL && item_count > 0) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }
        // copy the 
        for (int i = 0; i < item_count; i++) {
            t->items[i] = purchaed_items[i];
        }
        free(purchaed_items);

        pthread_mutex_lock(&c->mutex);
        
        #if ENABLE_ASSERTS
        assert(c->reciecpt == NULL);  // Customer shouldn't have a receipt yet
        #endif
        
        c->reciecpt = t; // give the receipt to the customer

        #if ENABLE_PRINTING || ENABLE_ASSERTS
        int customer_wallet = c->wallet;
        #endif

        // Signal the customer to pay    
        pthread_cond_signal(&c->cond);

        if (total == 0) {
            // Special case: No items purchased, no need to wait for payment
            #if ENABLE_PRINTING
            pthread_mutex_lock(&printf_mutex);
            printf("No items purchased by customer %d\n", c->id);
            pthread_mutex_unlock(&printf_mutex);
            #endif

            // The customer will still signal us, so we should wait for that signal
            pthread_cond_wait(&c->cond, &c->mutex);

            // The rest remains the same
            t->paid = 0; // Ensure paid is 0
        } else {
            #if ENABLE_PRINTING
            pthread_mutex_lock(&printf_mutex);
            printf("Clerk is waiting for customer %d to pay\n", c->id);
            pthread_mutex_unlock(&printf_mutex);
            #endif

            // Wait for the customer to pay
            while (t->paid < t->total) {
                pthread_cond_wait(&c->cond, &c->mutex);
            }
        }

        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("total: %d\n", total);
        printf("reciept total: %d\n", t->total);
        printf("paid: %d\n", t->paid);
        printf("customer_wallet: %d\n", customer_wallet);
        printf("expected wallet: %d\n", customer_wallet - total);
        printf("actual wallet: %d\n", c->wallet);
        pthread_mutex_unlock(&printf_mutex);
        #endif

        #if ENABLE_ASSERTS
        assert(total == t->total); // check if the total is correct
        assert(t->paid == t->total); // check if the customer paid the correct amount
        assert(customer_wallet - total == c->wallet); // check if the customer paid the correct amount
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
        // The transaction is complete, and we're sure the assistant thread is done with these objects
        if (assistant_wait_mutex != NULL) {
            pthread_mutex_destroy(assistant_wait_mutex);
            free(assistant_wait_mutex);
        }
        
        if (assistant_wait_cond != NULL) {
            pthread_cond_destroy(assistant_wait_cond);
            free(assistant_wait_cond);
        }
    }

    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d has made %d dollars and is leaving the shop\n", self->id, self->cash_register);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    free(self);
    return NULL;
}
