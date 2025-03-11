#include <stdint.h>
#include "shop.h"
#include "queue.h"

// In clerk.c
void* clerk_thread(void* arg) {
    int clerk_id = (int)(intptr_t)arg;
    
    while (1) {
        // Get next client from queue
        client_t* client = (client_t*)queue_pop(&client_queue);
        
        #ifdef ENABLE_DEBUG
        printf("Clerk %d serving client %d\n", clerk_id, client->id);
        #endif
        
        // Process client's shopping list
        for (int i = 0; i < client->list_size; i++) {
            int product_id = client->shopping_list[i];
            
            // Check if product needs assistant
            if (product_needs_assistant(product_id)) {
                // Create task for assistant
                assistant_task_t* task = create_assistant_task(product_id, clerk_id);
                queue_push(&task_queue, task);
                
                // Wait for task completion
                pthread_mutex_lock(&task->mutex);
                while (!task->is_complete) {
                    pthread_cond_wait(&task->completed, &task->mutex);
                }
                pthread_mutex_unlock(&task->mutex);
                
                // Free the task
                pthread_mutex_destroy(&task->mutex);
                pthread_cond_destroy(&task->completed);
                free(task);
            } else {
                // Try to get product directly
                try_get_product(product_id);
            }
        }
        
        // Client has been served
        #ifdef ENABLE_DEBUG
        printf("Clerk %d finished serving client %d\n", clerk_id, client->id);
        #endif
        
        // Free client resources
        free(client->shopping_list);
        free(client);
        
        // Check if we should exit (e.g., if all clients have been served)
        // This would need a separate synchronization mechanism
    }
    
    return NULL;
}