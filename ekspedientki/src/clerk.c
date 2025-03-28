#include "clerk.h"
#include "customer.h"
#include "shop.h"  // Include for deposit_to_safe function

/* Global Variables */
queue* clerk_queues[NUM_CLERKS];  // Array of queues, one per clerk

// Forward declarations of helper functions
static transaction_t* create_transaction(int shopping_list_size);
static bool process_customer_item(clerk_t* clerk, customer_t* customer, transaction_t* transaction);
static void finalize_transaction(clerk_t* clerk, customer_t* customer, transaction_t* transaction);

/**
 * Main function for the clerk thread.
 */
void* clerk_thread(void* arg) {
    clerk_t* self = (clerk_t*)arg;
    
    // Initialize pending_jobs counter
    self->pending_jobs = 0;
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d has entered the shop\n", self->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif

    while (1) {
        // Wait for the next customer (blocking call)
        void* customer_ptr = queue_pop(self->customer_queue);
        
        // Check if this is the sentinel value signaling to stop
        if (customer_ptr == SENTINEL_VALUE) {
            break;
        }
        
        customer_t* customer = (customer_t*)customer_ptr;
        
        #if ENABLE_ASSERTS
        assert(customer != NULL);
        assert(customer->id >= 0);
        assert(customer->wallet > 0);
        assert(customer->shopping_list != NULL);
        assert(customer->shopping_list_size > 0);
        #endif
        
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Clerk %d is serving customer %d\n", self->id, customer->id);
        pthread_mutex_unlock(&printf_mutex);
        #endif

        // Begin serving customer
        pthread_mutex_lock(&customer->mutex);
        
        // Signal customer we're ready to serve them
        customer->clerk_ready = true;
        pthread_cond_signal(&customer->cond);
        
        // Create a new transaction for this customer
        transaction_t* transaction = create_transaction(customer->shopping_list_size);
        
        // Process all items in the customer's shopping list
        bool shopping_complete = false;
        while (!shopping_complete) {
            shopping_complete = process_customer_item(self, customer, transaction);
        }
        
        // Wait for all assistant jobs to complete before finalizing the transaction
        if (self->pending_jobs > 0) {
            wait_for_clerk_jobs(self->id, self->pending_jobs);
            self->pending_jobs = 0; // Reset counter after waiting
        }
        
        // Complete the transaction and handle payment
        finalize_transaction(self, customer, transaction);
        
        pthread_mutex_unlock(&customer->mutex);
    }

    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d has made %d dollars and is leaving the shop\n", self->id, self->cash_register);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Deposit earnings to shop's safe
    deposit_to_safe(self->cash_register);
    
    free(self);
    return NULL;
}

/**
 * Creates and initializes a new transaction
 */
static transaction_t* create_transaction(int shopping_list_size) {
    transaction_t* transaction = malloc(sizeof(transaction_t));
    if (transaction == NULL) {
        fprintf(stderr, "Error: malloc failed for transaction\n");
        exit(1);
    }
    
    transaction->paid = 0;
    transaction->total = 0;
    transaction->items_size = 0;
    transaction->items = malloc(sizeof(int) * shopping_list_size);
    
    if (transaction->items == NULL) {
        fprintf(stderr, "Error: malloc failed for transaction items\n");
        exit(1);
    }
    
    return transaction;
}

/**
 * Process a single customer item request
 * 
 * @return true if shopping is complete, false if more items remain
 */
static bool process_customer_item(clerk_t* clerk, customer_t* customer, transaction_t* transaction) {
    // Wait until customer is ready with an item request or has finished shopping
    while (!customer->waiting_for_response && customer->current_item_index < customer->shopping_list_size) {
        pthread_cond_wait(&customer->cond, &customer->mutex);
    }
    
    // Check if customer has completed their shopping list
    if (customer->current_item_index >= customer->shopping_list_size) {
        return true; // Shopping complete
    }
    
    int product_id = customer->current_item;
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d processing item request %d for customer %d\n", 
          clerk->id, product_id, customer->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Process the requested item
    bool in_stock = try_get_product(product_id);
    
    if (in_stock) {
        int price = get_product_price(product_id);
        transaction->total += price;
        transaction->items[transaction->items_size++] = product_id;
        
        // If this product needs assistant preparation
        if (product_needs_assistant(product_id)) {
            // Create a new job for the assistant
            assistant_job_t* job = create_assistant_job(product_id, clerk->id);
            
            // Increment pending jobs counter
            clerk->pending_jobs++;
            
            // Add job to assistant queue
            queue_push(assistant_queue, job);
        }
    } else {
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Product %d out of stock for customer %d\n", product_id, customer->id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
    }
    
    // Signal customer we've processed this item
    customer->waiting_for_response = false;
    pthread_cond_signal(&customer->cond);
    
    return false; // More items may remain
}

/**
 * Finalizes the transaction, gives the receipt and collects payment
 */
static void finalize_transaction(clerk_t* clerk, customer_t* customer, transaction_t* transaction) {
    // Transaction complete, give receipt to customer
    customer->receipt = transaction;
    
    #if ENABLE_PRINTING || ENABLE_ASSERTS
    int customer_wallet = customer->wallet;
    #endif
    
    // Signal customer about receipt and wait for payment
    pthread_cond_signal(&customer->cond);
    
    // Wait for payment
    if (transaction->total > 0) {
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Clerk %d is waiting for customer %d to pay\n", clerk->id, customer->id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        // Wait for customer to make payment
        while (transaction->paid < transaction->total) {
            pthread_cond_wait(&customer->cond, &customer->mutex);
        }
    } else {
        // Even with zero total, wait for customer acknowledgment
        pthread_cond_wait(&customer->cond, &customer->mutex);
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d - Customer %d: Total: %d, Paid: %d, Customer wallet before: %d, after: %d\n", 
           clerk->id, customer->id, transaction->total, transaction->paid, customer_wallet, customer->wallet);
    pthread_mutex_unlock(&printf_mutex);
    #endif

    #if ENABLE_ASSERTS
    assert(transaction->total >= 0);
    assert(transaction->paid == transaction->total);
    assert(customer_wallet - transaction->total == customer->wallet);
    #endif

    // Update the cash register
    clerk->cash_register += transaction->paid;

    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d has been paid by customer %d\n", clerk->id, customer->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Signal customer transaction is complete
    pthread_cond_signal(&customer->cond);
}
