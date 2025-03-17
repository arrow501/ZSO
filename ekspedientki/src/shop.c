#include "shop.h"

#include "queue.h"
#include "product.h"
#include "assistant.h"
#include "clerk.h"
#include "parameters.h"
#include "transaction.h"
#include "customer.h"

// Global Variables
pthread_mutex_t queue_mutex;      // For atomic queue size checking
int customers_remaining;          // Track how many customers haven't finished
pthread_mutex_t customers_mutex;  // Protect the counter
pthread_mutex_t printf_mutex;     // Protect printf calls

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
    pthread_mutex_init(&printf_mutex, NULL);
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
    pthread_mutex_lock(&printf_mutex);
    printf("All customers and clerks have been created\n");
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Join all customer threads
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_join(customers[i], NULL);
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("All customers have left the shop\n");
    pthread_mutex_unlock(&printf_mutex);
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
    pthread_mutex_destroy(&printf_mutex);
    
    destroy_products();
    return 0;
}

