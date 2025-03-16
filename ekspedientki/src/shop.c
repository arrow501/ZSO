#include "queue.h"
#include "product.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_CUSTOMERS 100
#define NUM_CLERKS 1

#define ENABLE_PRINTING 0
#define ENABLE_ASSERTS 0

// Define the transaction for passing between customer and clerk
typedef struct clerk_t {
    int id;
    int cash_register;

} clerk_t;


typedef struct transaction_t {
    int total;
    int paid;
    int* items;
    int items_size;
} transaction_t;


// Define the structure
typedef struct customer_t {
    struct customer_t* myself;
    int id;
    int wallet;
    int* shopping_list;
    int shopping_list_size;

    transaction_t* reciecpt;

    pthread_cond_t cond;
    pthread_mutex_t mutex;
} customer_t;

// Global Variables
queue* customer_queue;
int total_merchandise_value = 0; // Track total value of all sold items

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
    queue_push(customer_queue, self);

    pthread_mutex_lock(&self->mutex);

    // Wait for the clerk to serve me
    while (self->reciecpt == NULL) {
        pthread_cond_wait(&self->cond, &self->mutex);
    }

    #if ENABLE_ASSERTS
    // Verify receipt is valid
    assert(self->reciecpt != NULL);
    assert(self->reciecpt->total >= 0);
    // Ensure customer has enough money
    assert(self->wallet >= self->reciecpt->total);
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
    // Add this new condition variable to signal transaction completion
    pthread_cond_wait(&self->cond, &self->mutex);

    pthread_mutex_unlock(&self->mutex);

    // Leave the shop
    #if ENABLE_PRINTING
    printf("Customer %d has left the shop\n", self->id);
    #endif

    // Clean up my properties - only after transaction is fully complete
    #if ENABLE_ASSERTS
    assert(self->reciecpt != NULL);
    assert(self->reciecpt->items != NULL || self->reciecpt->items_size == 0);
    #endif
    
    free(self->reciecpt->items);
    free(self->reciecpt);
    free(self->shopping_list);
    pthread_mutex_destroy(&self->mutex);
    pthread_cond_destroy(&self->cond);
    free(self);

    return NULL;
}

void* clerk_thread(void* arg) {
    clerk_t* self = (clerk_t*)arg;
    int clerk_total_merchandise_sold = 0; // Track value of merchandise sold by this clerk
    
    #if ENABLE_ASSERTS
    assert(self != NULL);
    assert(self->id >= 0);
    assert(self->cash_register == 0);
    #endif
    
    #if ENABLE_PRINTING
    printf("Clerk %d has entered the shop\n", self->id);
    #endif

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        // pop customer from queue; this is blocking
        customer_t* c = (customer_t*)queue_pop(customer_queue);
        
        #if ENABLE_ASSERTS
        assert(c != NULL);
        assert(c->id >= 0);
        assert(c->wallet > 0);
        assert(c->shopping_list != NULL);
        assert(c->shopping_list_size > 0);
        #endif
        
        #if ENABLE_PRINTING
        printf("Clerk is serving customer %d\n", c->id);
        #endif

        // allocate maximum space for the purchased items
        int* purchaed_items = (int*)malloc(sizeof(int) * c->shopping_list_size);
        if (purchaed_items == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }

        // ring up the customer
        int total = 0, item_count = 0;

        #if ENABLE_PRINTING
        printf("Clerk %d\n is ringing up customer %d\n", self->id, c->id);
        #endif
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
                clerk_total_merchandise_sold += price;
                
                #if ENABLE_ASSERTS
                assert(price > 0);
                #endif
            }
            // Add a debug print here:
            else {
                #if ENABLE_PRINTING
                printf("Product %d out of stock for customer %d\n", product_id, c->id);
                #endif
            }
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

        if (t->items == NULL) {
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

        #if ENABLE_ASSERTS || ENABLE_PRINTING
        int customer_wallet = c->wallet;
        #endif

        // Signal the customer to pay    
        pthread_cond_signal(&c->cond);

        if (total == 0) {
            // Special case: No items purchased, no need to wait for payment
            #if ENABLE_PRINTING
            printf("No items purchased by customer %d\n", c->id);
            #endif

            // The customer will still signal us, so we should wait for that signal
            pthread_cond_wait(&c->cond, &c->mutex);

            // The rest remains the same
            t->paid = 0; // Ensure paid is 0
        } else {
            #if ENABLE_PRINTING
            printf("Clerk is waiting for customer %d to pay\n", c->id);
            #endif

            // Wait for the customer to pay
            while (t->paid < t->total) {
                pthread_cond_wait(&c->cond, &c->mutex);
            }
        }

        #if ENABLE_PRINTING
        printf("total: %d\n", total);
        printf("reciept total: %d\n", t->total);
        printf("paid: %d\n", t->paid);
        printf("customer_wallet: %d\n", customer_wallet);
        printf("expected wallet: %d\n", customer_wallet - total);
        printf("actual wallet: %d\n", c->wallet);
        #endif

        #if ENABLE_ASSERTS
        assert(total == t->total); // check if the total is correct
        assert(t->paid == t->total); // check if the customer paid the correct amount
        assert(customer_wallet - total == c->wallet); // check if the customer paid the correct amount
        #endif

        // Update the cash register
        self->cash_register += t->paid;

        #if ENABLE_ASSERTS
        // Verify the cash register has increased by the correct amount
        assert(self->cash_register >= t->paid);
        #endif

        #if ENABLE_PRINTING
        printf("Clerk has been paid by customer %d\n", c->id);
        #endif

        // Signal the customer that transaction is fully complete before releasing the mutex
        pthread_cond_signal(&c->cond);
        pthread_mutex_unlock(&c->mutex);
    }

    // Atomically add to the total merchandise value
    __sync_fetch_and_add(&total_merchandise_value, clerk_total_merchandise_sold);

    #if ENABLE_ASSERTS
    // Verify the clerk's profits match what they sold
    assert(self->cash_register == clerk_total_merchandise_sold);
    #endif

    #if ENABLE_PRINTING
    printf("Clerk %d has made %d\n dollars and is leaving the shop", self->id, self->cash_register);
    #endif
    free(self);
    return NULL;
}

// Function to generate deterministic pseudo-random numbers
unsigned int get_pseudo_random(unsigned int seed, int min, int max) {
    // Linear Congruential Generator parameters (from glibc)
    const unsigned int a = 1103515245;
    const unsigned int c = 12345;
    const unsigned int m = 0x7fffffff; // 2^31 - 1

    // Generate next value in sequence
    unsigned int next = (a * seed + c) & m;

    // Scale to desired range
    return min + (next % (max - min + 1));
}

int zso() {
    pthread_t customers[NUM_CUSTOMERS];
    pthread_t clerks[NUM_CLERKS];
    int total_cash_register = 0;

    initialize_products();
    customer_queue = queue_create();
    
    #if ENABLE_ASSERTS
    assert(customer_queue != NULL);
    #endif

    // create clerks
    for (int i = 0; i < NUM_CLERKS; i++) {
        clerk_t* c = (clerk_t*)malloc(sizeof(clerk_t));
        if (c == NULL) {            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }
        c->id = i;
        c->cash_register = 0;
        
        #if ENABLE_ASSERTS
        int result = pthread_create(&clerks[i], NULL, clerk_thread, c);
        assert(result == 0);
        #else
        pthread_create(&clerks[i], NULL, clerk_thread, c);
        #endif
    }

    // create customers
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        customer_t* c = (customer_t*)malloc(sizeof(customer_t)); // customer must remember to free itself
        if (c == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }

        c->myself = c;
        c->id = i;
        c->wallet = get_pseudo_random(i, 100, 5000); 
        c->reciecpt = NULL;

        // Determine shopping list size (between 2 and 11 items)
        c->shopping_list_size = get_pseudo_random(i, 1, 10);

        // Allocate memory for shopping list
        c->shopping_list = (int*)malloc(sizeof(int) * c->shopping_list_size);
        if (c->shopping_list == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }

        // Generate shopping list using improved pseudo-random generator
        unsigned int seed = 12345 + i * 17; // Base seed unique to each customer
        for (int j = 0; j < c->shopping_list_size; j++) {
            // Update seed for each item to improve distribution
            seed = seed + j * 31;

            // Generate product ID in range 0 to (MAX_PRODUCTS-1)
            c->shopping_list[j] = get_pseudo_random(seed, 0, MAX_PRODUCTS - 1);
        }

        // Initialize synchronization primitives
        pthread_cond_init(&c->cond, NULL);
        pthread_mutex_init(&c->mutex, NULL);

        // Create customer thread
        #if ENABLE_ASSERTS
        int result = pthread_create(&customers[i], NULL, customer_thread, c);
        assert(result == 0);
        #else
        pthread_create(&customers[i], NULL, customer_thread, c);
        #endif
    }

    #if ENABLE_PRINTING
    printf("All customers and clerks have been created\n");
    #endif
    // Join all customer threads
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_join(customers[i], NULL);
    }
    #if ENABLE_PRINTING
    printf("All customers have left the shop\n");
    #endif
    
    // Join all clerk threads
    for (int i = 0; i < NUM_CLERKS; i++) {
        void* ret;
        pthread_join(clerks[i], &ret);
        // Get the final cash register values to verify total
        clerk_t* clerk = (clerk_t*)ret;
        if (clerk) {
            total_cash_register += clerk->cash_register;
        }
    }
    
    #if ENABLE_ASSERTS
    // Final verification that total profits match total merchandise value
    assert(total_cash_register == total_merchandise_value);
    #endif
    
    #if ENABLE_PRINTING
    printf("All clerks have left the shop\n");
    printf("Total profits: %d, Total merchandise value: %d\n", 
           total_cash_register, total_merchandise_value);
    #endif
    
    queue_destroy(customer_queue);
    destroy_products();
    return 0;
}

