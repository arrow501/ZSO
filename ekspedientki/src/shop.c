#include "queue.h"
#include "product.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>

#define NUM_CUSTOMERS 100
#define NUM_CLERKS 3  // Changed to 3 clerks

#define ENABLE_PRINTING 1
#define ENABLE_ASSERTS 1

// Special value to signal clerk to stop
#define SENTINEL_VALUE ((void*)(-1))

// Define the transaction for passing between customer and clerk
typedef struct clerk_t {
    int id;
    int cash_register;
    queue* customer_queue;  // Each clerk has their own queue
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

// Assistant job structure
typedef struct assistant_job_t {
    int product_id;           // Product that needs assistance
    int clerk_id;             // ID of the clerk requesting assistance
    pthread_mutex_t* mutex;   // Mutex for synchronization
    pthread_cond_t* cond;     // Condition variable for signaling
    int* completed;           // Flag to indicate completion
} assistant_job_t;

// Global Variables
queue* clerk_queues[NUM_CLERKS];  // Array of queues, one per clerk
pthread_mutex_t queue_mutex;      // For atomic queue size checking
int customers_remaining;          // Track how many customers haven't finished
pthread_mutex_t customers_mutex;  // Protect the counter

// Assistant variables
queue* assistant_queue;           // Queue for assistant tasks
pthread_t assistant_thread_id;    // Assistant thread ID
int assistant_running = 1;        // Flag to control assistant thread

// Assistant thread function
void* assistant_thread(void* arg) {
    #if ENABLE_PRINTING
    printf("Assistant has entered the shop\n");
    #endif

    while (1) {
        // Get a job from the queue
        void* job_ptr = queue_pop(assistant_queue);
        
        // Check if this is the sentinel value signaling to stop
        if (job_ptr == SENTINEL_VALUE) {
            break;
        }
        
        assistant_job_t* job = (assistant_job_t*)job_ptr;
        
        #if ENABLE_PRINTING
        printf("Assistant is preparing product %d for clerk %d\n", job->product_id, job->clerk_id);
        #endif
        
        // Simulate work with some math operations
        double result = 0;
        for (int i = 0; i < 1000000; i++) {
            result += sin(i) * cos(i);
            if (i % 100000 == 0) {
                result = fmod(result, 10.0);  // Keep the number manageable
            }
        }
        
        // Mark job as completed
        pthread_mutex_lock(job->mutex);
        *job->completed = 1;
        pthread_cond_signal(job->cond);
        pthread_mutex_unlock(job->mutex);
        
        #if ENABLE_PRINTING
        printf("Assistant finished preparing product %d (calculated %f)\n", job->product_id, result);
        #endif
        
        // Free the job structure
        free(job);
    }
    
    #if ENABLE_PRINTING
    printf("Assistant is leaving the shop\n");
    #endif
    
    return NULL;
}

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
    // Add this new condition variable to signal transaction completion
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

void* clerk_thread(void* arg) {
    clerk_t* self = (clerk_t*)arg;
    #if ENABLE_PRINTING
    printf("Clerk %d has entered the shop\n", self->id);
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

        // Keep track of special products that need assistant
        int special_products_count = 0;
        pthread_mutex_t assistant_wait_mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_cond_t assistant_wait_cond = PTHREAD_COND_INITIALIZER;
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
        printf("Clerk %d is ringing up customer %d\n", self->id, c->id);
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
                    printf("Clerk %d requests assistant for product %d\n", self->id, product_id);
                    #endif
                    
                    // Create a job for the assistant
                    assistant_job_t* job = (assistant_job_t*)malloc(sizeof(assistant_job_t));
                    if (job == NULL) {
                        fprintf(stderr, "Error: malloc failed\n");
                        exit(1);
                    }
                    
                    job->product_id = product_id;
                    job->clerk_id = self->id;
                    job->mutex = &assistant_wait_mutex;
                    job->cond = &assistant_wait_cond;
                    job->completed = &special_products_completed[special_index++];
                    
                    // Add job to assistant queue
                    queue_push(assistant_queue, job);
                }
            }
            // Add a debug print here:
            else {
                #if ENABLE_PRINTING
                printf("Product %d out of stock for customer %d\n", product_id, c->id);
                #endif
            }
        }

        // Wait for all special products to be prepared by the assistant
        if (special_products_count > 0) {
            pthread_mutex_lock(&assistant_wait_mutex);
            
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
                    printf("Clerk %d waiting for assistant to prepare products\n", self->id);
                    #endif
                    pthread_cond_wait(&assistant_wait_cond, &assistant_wait_mutex);
                }
            }
            
            pthread_mutex_unlock(&assistant_wait_mutex);
            #if ENABLE_PRINTING
            printf("Clerk %d has received all prepared products\n", self->id);
            #endif
            
            // Free the tracking array
            free(special_products_completed);
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

        #if ENABLE_PRINTING || ENABLE_ASSERTS
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

        // Clean up assistant synchronization objects
        pthread_mutex_destroy(&assistant_wait_mutex);
        pthread_cond_destroy(&assistant_wait_cond);

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

        #if ENABLE_PRINTING
        printf("Clerk has been paid by customer %d\n", c->id);
        #endif

        // Signal the customer that transaction is fully complete before releasing the mutex
        pthread_cond_signal(&c->cond);
        pthread_mutex_unlock(&c->mutex);
    }

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

    initialize_products();
    
    // Initialize mutexes
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&customers_mutex, NULL);
    customers_remaining = NUM_CUSTOMERS;
    
    // Create queues for each clerk
    for (int i = 0; i < NUM_CLERKS; i++) {
        clerk_queues[i] = queue_create();
    }

    // Create assistant queue and thread
    assistant_queue = queue_create();
    pthread_create(&assistant_thread_id, NULL, assistant_thread, NULL);

    // Create clerks
    for (int i = 0; i < NUM_CLERKS; i++) {
        clerk_t* c = (clerk_t*)malloc(sizeof(clerk_t));
        if (c == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }
        c->id = i;
        c->cash_register = 0;
        c->customer_queue = clerk_queues[i];
        
        #if ENABLE_ASSERTS
        int result = pthread_create(&clerks[i], NULL, clerk_thread, c);
        assert(result == 0);
        #else
        pthread_create(&clerks[i], NULL, clerk_thread, c);
        #endif
    }

    // Create customers
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

    // Signal the assistant to stop and join
    queue_push(assistant_queue, SENTINEL_VALUE);
    pthread_join(assistant_thread_id, NULL);
    
    // Join all clerk threads
    for (int i = 0; i < NUM_CLERKS; i++) {
        pthread_join(clerks[i], NULL);
    }

    // Clean up resources
    for (int i = 0; i < NUM_CLERKS; i++) {
        queue_destroy(clerk_queues[i]);
    }
    queue_destroy(assistant_queue);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&customers_mutex);
    
    destroy_products();
    return 0;

}

