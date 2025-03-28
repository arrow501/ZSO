#include "shop.h"
#include "queue.h"
#include "product.h"
#include "assistant.h"
#include "clerk.h"
#include "parameters.h"
#include "transaction.h"
#include "customer.h"

/* Global Variables */
// Mutex for atomic queue operations
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// Customer tracking
int customers_remaining = 0;          // Track how many customers haven't finished
pthread_mutex_t customers_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global variables for customer spawning
pthread_mutex_t spawner_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t spawner_cond = PTHREAD_COND_INITIALIZER;
int active_customers = 0;             // Currently active customer threads
int customers_spawned = 0;            // Total customers created so far
int spawner_running = 1;              // Flag to control spawner thread
pthread_t spawner_thread_id;

// Global variables for shop earnings
pthread_mutex_t safe_mutex = PTHREAD_MUTEX_INITIALIZER;
int shop_earnings = 0;                // Total earnings collected from all clerks

/**
 * Generates deterministic pseudo-random numbers.
 * 
 * @param seed Seed value for random generation 
 * @param min Minimum value in range (inclusive)
 * @param max Maximum value in range (inclusive)
 * @return A pseudo-random value between min and max
 */
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
 * Creates a customer with a shopping list and initializes their thread.
 * 
 * @param customer_id Unique identifier for the customer
 * @param customers Array to store thread ID
 * @return true if customer was created successfully, false otherwise
 */
static bool create_customer(int customer_id, pthread_t* customers) {
    customer_t* c = (customer_t*)malloc(sizeof(customer_t));
    if (c == NULL) {
        fprintf(stderr, "Error: malloc failed for customer\n");
        return false;
    }
    
    // Initialize customer
    c->id = customer_id;
    c->wallet = get_pseudo_random(customer_id, 100, 5000); 
    c->receipt = NULL;
    
    // Determine shopping list size (between 1 and 10 items)
    c->shopping_list_size = get_pseudo_random(customer_id, 1, 10);
    
    // Allocate memory for shopping list
    c->shopping_list = (int*)malloc(sizeof(int) * c->shopping_list_size);
    if (c->shopping_list == NULL) {
        fprintf(stderr, "Error: malloc failed for shopping list\n");
        free(c);
        return false;
    }
    
    // Generate shopping list using pseudo-random generator
    unsigned int seed = 12345 + customer_id * 17; // Base seed unique to each customer
    for (int j = 0; j < c->shopping_list_size; j++) {
        // Update seed for each item to improve distribution
        seed = seed + j * 31;
        // Generate product ID in range 0 to (MAX_PRODUCTS-1)
        c->shopping_list[j] = get_pseudo_random(seed, 0, MAX_PRODUCTS - 1);
    }
    
    // Initialize synchronization primitives
    int result = pthread_cond_init(&c->cond, NULL);
    if (result != 0) {
        fprintf(stderr, "Error: pthread_cond_init failed for customer %d, error: %d\n", 
                customer_id, result);
        free(c->shopping_list);
        free(c);
        return false;
    }
    
    result = pthread_mutex_init(&c->mutex, NULL);
    if (result != 0) {
        fprintf(stderr, "Error: pthread_mutex_init failed for customer %d, error: %d\n", 
                customer_id, result);
        pthread_cond_destroy(&c->cond);
        free(c->shopping_list);
        free(c);
        return false;
    }
    
    // Create customer thread
    result = pthread_create(&customers[customer_id], NULL, customer_thread, c);
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
        
        return false;
    }
    
    return true;
}

/**
 * Creates clerk threads and initializes their data structures.
 * 
 * @param clerks Array to store clerk thread IDs
 */
static void create_clerks(pthread_t clerks[]) {
    for (int i = 0; i < NUM_CLERKS; i++) {
        clerk_t* c = (clerk_t*)malloc(sizeof(clerk_t));
        if (c == NULL) {
            fprintf(stderr, "Error: malloc failed for clerk\n");
            exit(1);
        }
        
        c->id = i;
        c->cash_register = 0;
        c->customer_queue = clerk_queues[i];
        
        int result = pthread_create(&clerks[i], NULL, clerk_thread, c);
        if (result != 0) {
            fprintf(stderr, "Error: Failed to create clerk thread %d, error: %d\n", i, result);
            exit(1);
        }
        
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Created clerk thread %d\n", i);
        pthread_mutex_unlock(&printf_mutex);
        #endif
    }
}

/**
 * Thread function that gradually creates customer threads throughout the simulation.
 */
void* customer_spawner_thread(void* arg) {
    pthread_t* customers = (pthread_t*)arg;
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Customer spawner thread started\n");
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    while (spawner_running) {
        pthread_mutex_lock(&spawner_mutex);
        
        // Wait until we have room for another customer
        while (active_customers >= MAX_CONCURRENT_CUSTOMERS && spawner_running) {
            pthread_cond_wait(&spawner_cond, &spawner_mutex);
        }
        
        // Check if we should exit
        if (!spawner_running || customers_spawned >= NUM_CUSTOMERS) {
            pthread_mutex_unlock(&spawner_mutex);
            break;
        }
        
        // Create a new customer
        int customer_id = customers_spawned;
        bool success = create_customer(customer_id, customers);
        
        if (success) {
            // Increment counters
            active_customers++;
            customers_spawned++;
            
            #if ENABLE_PRINTING
            pthread_mutex_lock(&printf_mutex);
            printf("Created customer %d (active: %d, total: %d)\n", 
                   customer_id, active_customers, customers_spawned);
            pthread_mutex_unlock(&printf_mutex);
            #endif
        }
        
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
 * Signal that a customer has left the shop, allowing a new one to be created.
 */
void signal_customer_exit() {
    pthread_mutex_lock(&spawner_mutex);
    active_customers--;
    pthread_cond_signal(&spawner_cond);
    pthread_mutex_unlock(&spawner_mutex);
}

/**
 * Clean up resources after simulation.
 */
static void cleanup_resources() {
    // Clean up queues
    for (int i = 0; i < NUM_CLERKS; i++) {
        queue_destroy(clerk_queues[i]);
    }
    queue_destroy(assistant_queue);
    
    // Clean up clerk inboxes
    cleanup_clerk_inboxes();
    
    // Clean up products
    destroy_products();
}

/**
 * Main simulation function.
 */
int zso() {
    pthread_t customers[NUM_CUSTOMERS];
    pthread_t clerks[NUM_CLERKS];
    
    // Initialize products inventory
    initialize_products();
    
    // Initialize simulation state
    customers_remaining = NUM_CUSTOMERS;
    active_customers = 0;
    customers_spawned = 0;
    spawner_running = 1;
    shop_earnings = 0;
    
    // Create queues for each clerk
    for (int i = 0; i < NUM_CLERKS; i++) {
        clerk_queues[i] = queue_create();
    }
    
    // Create assistant queue and clerk inboxes
    assistant_queue = queue_create();
    initialize_clerk_inboxes();
    
    // Create assistant thread
    int result = pthread_create(&assistant_thread_id, NULL, assistant_thread, NULL);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to create assistant thread, error: %d\n", result);
        exit(1);
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Created assistant thread\n");
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Create clerk threads
    create_clerks(clerks);
    
    // Create customer spawner thread
    result = pthread_create(&spawner_thread_id, NULL, customer_spawner_thread, customers);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to create customer spawner thread, error: %d\n", result);
        exit(1);
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("All clerks and customer spawner have been created\n");
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Join the spawner thread (it will exit when all customers are created)
    result = pthread_join(spawner_thread_id, NULL);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to join customer spawner thread, error: %d\n", result);
        exit(1);
    }
    
    // Join all customer threads
    for (int i = 0; i < customers_spawned; i++) {
        result = pthread_join(customers[i], NULL);
        if (result != 0) {
            fprintf(stderr, "Warning: Failed to join customer thread %d, error: %d\n", i, result);
            // Not critical, continue execution
        }
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("All customers have left the shop\n");
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Signal the assistant to stop and join
    queue_push(assistant_queue, SENTINEL_VALUE);
    result = pthread_join(assistant_thread_id, NULL);
    if (result != 0) {
        fprintf(stderr, "Error: Failed to join assistant thread, error: %d\n", result);
        exit(1);
    }
    
    // Join all clerk threads
    for (int i = 0; i < NUM_CLERKS; i++) {
        queue_push(clerk_queues[i], SENTINEL_VALUE);
    }
    
    for (int i = 0; i < NUM_CLERKS; i++) {
        result = pthread_join(clerks[i], NULL);
        if (result != 0) {
            fprintf(stderr, "Error: Failed to join clerk thread %d, error: %d\n", i, result);
            exit(1);
        }
    }
    
    // Print total earnings
    printf("The shop made a total of %d cents during this simulation\n", shop_earnings);
    
    // Clean up resources
    cleanup_resources();
    
    return 0;
}

