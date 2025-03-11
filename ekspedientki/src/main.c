#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#include "shop.h"
#include "queue.h"

// Global queue definitions
client_queue_t client_queue;
task_queue_t task_queue;

// Function to run the shop simulation
void projekt_zso() {
    // Initialize resources
    initialize_products();
    
    // Initialize queues
    client_queue_init(&client_queue, MAX_CLIENTS);
    task_queue_init(&task_queue, MAX_CLIENTS * 2);
    
    // Initialize mutex
    pthread_mutex_init(&inventory_mutex, NULL);
    
    // Create clients
    // (Create client data structures and add to queue)
    
    // Create threads
    pthread_t clerk_threads[NUM_CLERKS];
    pthread_t assistant_thread_id;
    
    // Start threads
    for (int i = 0; i < NUM_CLERKS; i++) {
        pthread_create(&clerk_threads[i], NULL, clerk_thread, (void*)(intptr_t)i);
    }
    pthread_create(&assistant_thread_id, NULL, assistant_thread, NULL);
    
    // Join threads
    for (int i = 0; i < NUM_CLERKS; i++) {
        pthread_join(clerk_threads[i], NULL);
    }
    pthread_join(assistant_thread_id, NULL);
    
    // Cleanup
    client_queue_cleanup(&client_queue);
    task_queue_cleanup(&task_queue);
    pthread_mutex_destroy(&inventory_mutex);
}

int main() {
    for (int i = 0; i < 10; i++) {
        projekt_zso();
    }
    return 0;
}