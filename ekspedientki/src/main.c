#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

// Simple product structure
struct product {
    int id;
    char* name;
    int price;
    int stock;
};

// Simple client structure
struct client {
    int id;
    int* shoppingList;  
    int listSize;
    bool isDone;
    int total;
};

// Simple clerk structure
struct clerk {
    int id;
    bool busy;
};

// Global variables
struct product products[2];  // Just 2 products
struct clerk shopClerk;      // Just 1 clerk
struct client customer;      // Just 1 customer

// Synchronization variables
pthread_mutex_t shop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clerk_available = PTHREAD_COND_INITIALIZER;
pthread_cond_t client_served = PTHREAD_COND_INITIALIZER;
bool client_waiting = false;
bool client_being_served = false;

// Initialize the shop
void initShop() {
    // Initialize products
    products[0].id = 0;
    products[0].name = "milk";
    products[0].price = 599;
    products[0].stock = 5;
    
    products[1].id = 1;
    products[1].name = "bread";
    products[1].price = 349;
    products[1].stock = 3;
    
    // Initialize clerk
    shopClerk.id = 1;
    shopClerk.busy = false;
}

// Request a product from the shop (must be called with mutex locked)
bool requestProduct(struct client* client, int productId) {
    if (productId >= 2 || products[productId].stock <= 0) {
        printf("Clerk: Sorry, we're out of %s\n", 
               productId < 2 ? products[productId].name : "that product");
        return false;
    }
    
    printf("Clerk gives %s to client\n", products[productId].name);
    client->total += products[productId].price;
    
    // Decrease stock
    products[productId].stock--;
    return true;
}

// Process payment (must be called with mutex locked)
void pay(struct client* client) {
    int total = 0;
    
    for (int i = 0; i < client->listSize; i++) {
        int productId = client->shoppingList[i];
        if (productId < 2) {
            total += products[productId].price;
        }
    }
    
    printf("Client pays $%.2f to clerk\n", total / 100.0);
    
    // Simulate payment time
    #ifdef ENABLE_DELAYS
    sleep(1);
    #endif
    
    client->isDone = true;
}

// Clerk thread function
void* clerk_thread(void* arg) {
    printf("Clerk is ready to serve customers\n");
    
    while (1) {
        // Lock the mutex to access shared data
        pthread_mutex_lock(&shop_mutex);
        
        // Wait until a client arrives
        while (!client_waiting) {
            #ifdef ENABLE_PRINTING
            printf("Clerk is waiting for clients...\n");
            #endif
            pthread_cond_wait(&clerk_available, &shop_mutex);
        }
        
        // Start serving the client
        #ifdef ENABLE_PRINTING
        printf("Clerk starts srving client\n");
        #endif
        shopClerk.busy = true;
        client_being_served = true;
        
        // Process each item in client's shopping list
        for (int i = 0; i < customer.listSize; i++) {
            int productId = customer.shoppingList[i];
            requestProduct(&customer, productId);
        }
        
        // Process payment
        pay(&customer);
        
        // Finished serving
        #ifdef ENABLE_PRINTING
        printf("Clerk finishedserving client\n");
        #endif
        shopClerk.busy = false;
        client_being_served = false;
        client_waiting = false;
        
        // Signal that the client has been served
        pthread_cond_signal(&client_served);
        
        // Unlock the mutex
        pthread_mutex_unlock(&shop_mutex);
        
        // Determine if we should exit the thread
        // (In a real application, you'd have a proper termination condition)
        #ifdef ENABLE_DELAYS
        sleep(1);
        #endif
        
        // For this example, we'll exit after serving one client
        if (customer.isDone) {
            break;
        }
    }
    #ifdef ENABLE_PRINTING
    printf("Clerk is going home\n");
    #endif
    return NULL;
}

// Client thread function
void* client_thread(void* arg) {
    printf("Client enters the shop\n");
    
    // Lock the mutex to access shared data
    pthread_mutex_lock(&shop_mutex);
    
    // Signal arrival to clerk
    #ifdef ENABLE_PRINTING
    printf("Client is waiting for clerk\n");
    #endif
    client_waiting = true;
    pthread_cond_signal(&clerk_available);
    
    // Wait until service is complete
    while (!customer.isDone) {
        pthread_cond_wait(&client_served, &shop_mutex);
    }
    #ifdef ENABLE_PRINTING
    printf("Client leaves the shop\n");
    #endif
    // Unlock the mutex
    pthread_mutex_unlock(&shop_mutex);
    
    return NULL;
}

// Function to wrap the multithreaded shop simulation
void projekt_zso() {
    // Initialize the shop
    initShop();
    
    // Create a client who wants milk and bread
    int shoppingList[] = {0, 1};
    customer.id = 1;
    customer.shoppingList = shoppingList;
    customer.listSize = 2;
    customer.isDone = false;
    
    // Create threads for clerk and client
    pthread_t clerk_tid, client_tid;
    
    pthread_create(&clerk_tid, NULL, clerk_thread, NULL);
    pthread_create(&client_tid, NULL, client_thread, NULL);
    
    // Wait for threads to finish
    pthread_join(client_tid, NULL);
    pthread_join(clerk_tid, NULL);
    
    // Clean up
    pthread_mutex_destroy(&shop_mutex);
    pthread_cond_destroy(&clerk_available);
    pthread_cond_destroy(&client_served);
}

int main() {
    // Run the simulation
    projekt_zso();
    
    return 0;
}