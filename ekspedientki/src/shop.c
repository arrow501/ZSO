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

// Global variables for customer spawning
pthread_mutex_t spawner_mutex;
pthread_cond_t spawner_cond;
int active_customers = 0;
int customers_spawned = 0;
int spawner_running = 1;
pthread_t spawner_thread_id;

// Global variables for shop earnings
pthread_mutex_t safe_mutex;
int shop_earnings = 0;  // Total earnings collected from all clerks

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

/**
 * Collects money from a clerk into the shop's safe.
 * 
 * @param amount Amount of money to add to the safe
 */
void deposit_to_safe(int amount) {
    pthread_mutex_lock(&safe_mutex);
    shop_earnings += amount;
    pthread_mutex_unlock(&safe_mutex);
}

/**
 * Create clerk threads and initialize their data structures
 * 
 * @param clerks Array to store clerk thread IDs
 */
void create_clerks(pthread_t clerks[]) {
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
}

/**
 * Thread function that creates customers gradually throughout the simulation
 */
void* customer_spawner_thread(void* arg) {
    pthread_t* customers = (pthread_t*)arg;
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer spawner thread started\n");
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    while (1) {
        pthread_mutex_lock(&spawner_mutex);
        
        // Wait until we have room for another customer
        while (active_customers >= MAX_CONCURRENT_CUSTOMERS && spawner_running) {
            pthread_cond_wait(&spawner_cond, &spawner_mutex);
        }
        
        // Check if we should exit
        if (!spawner_running) {
            pthread_mutex_unlock(&spawner_mutex);
            break;
        }
        
        // Check if we've created all required customers
        if (customers_spawned >= NUM_CUSTOMERS) {
            pthread_mutex_unlock(&spawner_mutex);
            break;
        }
        
        // Create a new customer
        int customer_id = customers_spawned;
        customer_t* c = (customer_t*)malloc(sizeof(customer_t));
        if (c == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            pthread_mutex_unlock(&spawner_mutex);
            continue; // Try again later
        }

        c->myself = c;
        c->id = customer_id;
        c->wallet = get_pseudo_random(customer_id, 100, 5000); 
        c->receipt = NULL;

        // Determine shopping list size (between 2 and 11 items)
        c->shopping_list_size = get_pseudo_random(customer_id, 1, 10);

        // Allocate memory for shopping list
        c->shopping_list = (int*)malloc(sizeof(int) * c->shopping_list_size);
        if (c->shopping_list == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            free(c);
            pthread_mutex_unlock(&spawner_mutex);
            continue; // Try again later
        }

        // Generate shopping list using improved pseudo-random generator
        unsigned int seed = 12345 + customer_id * 17; // Base seed unique to each customer
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
        int result = pthread_create(&customers[customer_id], NULL, customer_thread, c);
        if (result != 0) {
            // Creation failed, clean up
            pthread_mutex_destroy(&c->mutex);
            pthread_cond_destroy(&c->cond);
            free(c->shopping_list);
            free(c);
            
            #if ENABLE_PRINTING
            pthread_mutex_lock(&printf_mutex);
            printf("Failed to create customer thread %d, error: %d\n", customer_id, result);
            pthread_mutex_unlock(&printf_mutex);
            #endif
            
            pthread_mutex_unlock(&spawner_mutex);
            continue; // Try again later
        }
        
        // Increment counters
        active_customers++;
        customers_spawned++;
        
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Created customer %d (active: %d, total: %d)\n", 
               customer_id, active_customers, customers_spawned);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        pthread_mutex_unlock(&spawner_mutex);
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer spawner thread finished, created %d customers\n", customers_spawned);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    return NULL;
}

/**
 * Signal that a customer has left the shop, allowing a new one to be created
 */
void signal_customer_exit() {
    pthread_mutex_lock(&spawner_mutex);
    active_customers--;
    pthread_cond_signal(&spawner_cond);
    pthread_mutex_unlock(&spawner_mutex);
}

/**
 * Modify the zso function to use the customer spawner thread
 */
int zso() {
    pthread_t customers[NUM_CUSTOMERS];
    pthread_t clerks[NUM_CLERKS];

    initialize_products();
    
    // Initialize mutexes
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&customers_mutex, NULL);
    pthread_mutex_init(&printf_mutex, NULL);
    pthread_mutex_init(&spawner_mutex, NULL);
    pthread_mutex_init(&safe_mutex, NULL);
    pthread_cond_init(&spawner_cond, NULL);
    
    customers_remaining = NUM_CUSTOMERS;
    active_customers = 0;
    customers_spawned = 0;
    spawner_running = 1;
    shop_earnings = 0;
    
    // Create queues for each clerk
    for (int i = 0; i < NUM_CLERKS; i++) {
        clerk_queues[i] = queue_create();
    }

    // Create assistant queue and thread
    assistant_queue = queue_create();
    pthread_create(&assistant_thread_id, NULL, assistant_thread, NULL);

    // Create clerks 
    create_clerks(clerks);

    // Create customer spawner thread instead of creating all customers at once
    pthread_create(&spawner_thread_id, NULL, customer_spawner_thread, customers);

    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("All clerks and customer spawner have been created\n");
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Join the spawner thread (it will exit when all customers are created)
    pthread_join(spawner_thread_id, NULL);
    
    // Join all customer threads
    for (int i = 0; i < customers_spawned; i++) {
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
    
    // Print total earnings (not gated by ENABLE_PRINTING)
    printf("The shop made a total of %d cents during this simulation\n", shop_earnings);

    // Clean up resources
    for (int i = 0; i < NUM_CLERKS; i++) {
        queue_destroy(clerk_queues[i]);
    }
    queue_destroy(assistant_queue);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&customers_mutex);
    pthread_mutex_destroy(&printf_mutex);
    pthread_mutex_destroy(&spawner_mutex);
    pthread_mutex_destroy(&safe_mutex);
    pthread_cond_destroy(&spawner_cond);
    
    destroy_products();
    return 0;
}

