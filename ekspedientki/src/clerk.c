#include "clerk.h"
#include "customer.h"
#include "shop.h"  // Include for deposit_to_safe function

/* Global Variables */
queue* clerk_queues[NUM_CLERKS];  // Array of queues, one per clerk

// Helper functions for condition checking
static inline bool is_customer_requesting_item(customer_t* customer) {
    return customer->state == CUSTOMER_REQUESTING_ITEM;
}

static inline bool is_customer_ready_for_next_item(customer_t* customer) {
    return (customer->current_item_index < customer->shopping_list_size && 
            customer->state == CUSTOMER_REQUESTING_ITEM);
}

static inline bool is_payment_complete(transaction_t* transaction) {
    return transaction->paid == transaction->total;
}

static inline bool is_assistant_job_complete(int clerk_id) {
    return assistant_job_completed[clerk_id];
}

// Forward declarations of helper functions
static transaction_t* create_transaction(int shopping_list_size);
static void process_item_request(clerk_t* clerk, customer_t* customer, transaction_t* transaction);
static void request_assistant_help(clerk_t* clerk, int product_id);
static void finalize_transaction(clerk_t* clerk, customer_t* customer, transaction_t* transaction);

/**
 * Main function for the clerk thread.
 */
void* clerk_thread(void* arg) {
    clerk_t* self = (clerk_t*)arg;
    
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

        // Setup for item-by-item processing
        pthread_mutex_lock(&customer->mutex);
        
        // Signal clerk is ready to serve
        customer->state = CUSTOMER_REQUESTING_ITEM;
        pthread_cond_signal(&customer->cond);
        
        // Create a new transaction for this customer
        transaction_t* transaction = create_transaction(customer->shopping_list_size);
        
        // Process all items in the customer's shopping list
        while (customer->current_item_index < customer->shopping_list_size) {
            process_item_request(self, customer, transaction);
        }
        
        // Complete the transaction and handle payment
        finalize_transaction(self, customer, transaction);
        
        // Signal the customer that transaction is fully complete before releasing the mutex
        pthread_cond_signal(&customer->cond);
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
 * Processes a single item request from the customer
 */
static void process_item_request(clerk_t* clerk, customer_t* customer, transaction_t* transaction) {
    // Wait for customer to request an item
    while (!is_customer_requesting_item(customer)) {
        pthread_cond_wait(&customer->cond, &customer->mutex);
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d processing item request %d for customer %d\n", 
          clerk->id, customer->current_item, customer->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Process the requested item
    int product_id = customer->current_item;
    bool in_stock = try_get_product(product_id);
    
    if (in_stock) {
        int price = get_product_price(product_id);
        transaction->total += price;
        transaction->items[transaction->items_size++] = product_id;
        
        // If this product needs assistant preparation
        if (product_needs_assistant(product_id)) {
            request_assistant_help(clerk, product_id);
        }
    } else {
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Product %d out of stock for customer %d\n", product_id, customer->id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
    }
    
    // Signal customer we've processed this item
    customer->state = CUSTOMER_PROCESSING_RECEIPT;
    pthread_cond_signal(&customer->cond);
    
    // Wait for customer to acknowledge and be ready for next item
    while (!is_customer_ready_for_next_item(customer)) {
        pthread_cond_wait(&customer->cond, &customer->mutex);
    }
}

/**
 * Requests help from the assistant for special products
 */
static void request_assistant_help(clerk_t* clerk, int product_id) {
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d requests assistant for product %d\n", clerk->id, product_id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Create a job for the assistant
    assistant_job_t* job = malloc(sizeof(assistant_job_t));
    if (job == NULL) {
        fprintf(stderr, "Error: malloc failed for assistant job\n");
        exit(1);
    }
    
    job->product_id = product_id;
    job->clerk_id = clerk->id;
    
    // Reset completion status before posting the job
    pthread_mutex_lock(&assistant_mutex);
    assistant_job_completed[clerk->id] = 0;
    pthread_mutex_unlock(&assistant_mutex);
    
    // Add job to assistant queue
    queue_push(assistant_queue, job);
    
    // Wait for assistant to complete
    pthread_mutex_lock(&assistant_mutex);
    while (!is_assistant_job_complete(clerk->id)) {
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Clerk %d waiting for assistant to prepare product %d\n", clerk->id, product_id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        pthread_cond_wait(&clerk_cond_vars[clerk->id], &assistant_mutex);
    }
    pthread_mutex_unlock(&assistant_mutex);
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d received prepared product %d\n", clerk->id, product_id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
}

/**
 * Finalizes the transaction, sets the receipt and waits for payment
 */
static void finalize_transaction(clerk_t* clerk, customer_t* customer, transaction_t* transaction) {
    // Transaction complete, give receipt to customer
    customer->receipt = transaction;
    
    #if ENABLE_PRINTING || ENABLE_ASSERTS
    int customer_wallet = customer->wallet;
    #endif
    
    // Signal customer that receipt is ready
    pthread_cond_signal(&customer->cond);
    
    // Wait for payment
    if (transaction->total > 0) {
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Clerk %d is waiting for customer %d to pay\n", clerk->id, customer->id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        // Wait for customer to pay
        while (!is_payment_complete(transaction)) {
            pthread_cond_wait(&customer->cond, &customer->mutex);
        }
    } else {
        // For zero-total transactions, still need to wait for customer to reach PAYING state
        while (customer->state != CUSTOMER_PAYING) {
            pthread_cond_wait(&customer->cond, &customer->mutex);
        }
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
    
    // IMPORTANT: Set the transaction complete state and explicitly signal
    customer->state = CUSTOMER_TRANSACTION_COMPLETE;
    pthread_cond_signal(&customer->cond);
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d completed transaction with customer %d\n", clerk->id, customer->id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
}
