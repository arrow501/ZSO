#include <stdio.h>
#include <stdbool.h>

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
};

// Simple clerk structure
struct clerk {
    int id;
    bool busy;
};

// Global variables
struct product products[2]; // Just 2 products
struct clerk shopClerk;     // Just 1 clerk

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

// Request a product from the shop
bool requestProduct(struct client* client, int productId) {
    if (productId >= 2 || products[productId].stock <= 0) {
        printf("Clerk: Sorry, we're out of %s\n", 
               productId < 2 ? products[productId].name : "that product");
        return false;
    }
    
    printf("Clerk gives %s to client\n", products[productId].name);
    
    // Decrease stock
    products[productId].stock--;
    return true;
}

// Process payment
void pay(struct client* client) {
    int total = 0;
    
    for (int i = 0; i < client->listSize; i++) {
        int productId = client->shoppingList[i];
        if (productId < 2) {
            total += products[productId].price;
        }
    }
    
    printf("Client pays $%.2f to clerk\n", total / 100.0);
    client->isDone = true;
}

// Serve a client
void serveClient(struct client* client) {
    printf("Clerk starts serving client\n");
    shopClerk.busy = true;
    
    // Process each item in shopping list
    for (int i = 0; i < client->listSize; i++) {
        int productId = client->shoppingList[i];
        requestProduct(client, productId);
    }
    
    // Process payment
    pay(client);
    
    printf("Clerk finished serving client\n");
    shopClerk.busy = false;
}

int main() {
    // Initialize the shop
    initShop();
    
    // Create a client who wants milk and bread
    int shoppingList[] = {0, 1};
    struct client customer;
    customer.id = 1;
    customer.shoppingList = shoppingList;
    customer.listSize = 2;
    customer.isDone = false;
    
    // Serve the client
    serveClient(&customer);
    
    return 0;
}