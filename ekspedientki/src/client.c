#include "shop.h"
#include "queue.h"

// Helper function to create a deterministic shopping list for a client
static int* create_shopping_list(int client_id, int* list_size) {
    // Deterministic pattern based on client ID
    // Each client will have 2-5 items
    *list_size = (client_id % 4) + 2;
    
    int* shopping_list = (int*)malloc(*list_size * sizeof(int));
    
    // Fill shopping list with product IDs
    // We use a deterministic pattern based on client ID
    for (int i = 0; i < *list_size; i++) {
        // This formula ensures different clients want different products
        // but in a consistent, reproducible way
        shopping_list[i] = (client_id + i * 3) % num_products;
    }
    
    return shopping_list;
}

// Create clients and add them directly to the queue
void create_and_queue_clients(queue_t* queue) {
    // Create and add each client to the queue
    for (int i = 0; i < MAX_CLIENTS; i++) {
        // Allocate a new client
        client_t* client = (client_t*)malloc(sizeof(client_t));
        
        // Initialize the client
        client->id = i + 1; // Client IDs start from 1
        client->current_item = 0;
        client->is_done = false;
        
        // Create shopping list
        client->shopping_list = create_shopping_list(i, &client->list_size);
        
        // Add client directly to queue
        queue_push(queue, client);
    }
}