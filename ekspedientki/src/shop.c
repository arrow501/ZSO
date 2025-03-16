#include "queue.h"
#include "product.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_CUSTOMERS 100
#define NUM_CLERKS 1


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



void* customer_thread(void* arg) {
    customer_t* self = (customer_t*)arg;

    printf("Customer %d has entered the shop\n", self->id);
    queue_push(customer_queue, self);

    pthread_mutex_lock(&self->mutex);

    // Wait for the clerk to serve me
    while (self->reciecpt == NULL) {
        pthread_cond_wait(&self->cond, &self->mutex);
    }

    // Check the receipt and pay
    self->wallet -= self->reciecpt->total;
    self->reciecpt->paid = self->reciecpt->total;

    // Signal the clerk that I have paid
    printf("Customer %d is paying the clerk\n", self->id);
    pthread_cond_signal(&self->cond);

    // Wait for clerk to confirm receipt of payment
    // Add this new condition variable to signal transaction completion
    pthread_cond_wait(&self->cond, &self->mutex);

    pthread_mutex_unlock(&self->mutex);

    // Leave the shop
    printf("Customer %d has left the shop\n", self->id);

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
    printf("Clerk %d has entered the shop\n", self->id);

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        // pop customer from queue; this is blocking
        customer_t* c = (customer_t*)queue_pop(customer_queue);
        printf("Clerk is serving customer %d\n", c->id);



        // allocate maximum space for the purchased items
        int* purchaed_items = (int*)malloc(sizeof(int) * c->shopping_list_size);
        if (purchaed_items == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }

        // ring up the customer
        int total = 0, item_count = 0;

        printf("Clerk %d\n is ringing up customer %d\n", self->id, c->id);
        for (int i = 0; i < c->shopping_list_size; i++) {
            int product_id = c->shopping_list[i];

            bool in_stock = try_get_product(product_id);
            if (in_stock) {
                total += get_product_price(product_id);
                purchaed_items[item_count++] = product_id;
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
        c->reciecpt = t; // give the receipt to the customer
        int customer_wallet = c->wallet;

        // Signal the customer to pay    
        pthread_cond_signal(&c->cond);

        printf("Clerk is waiting for customer %d to pay\n", c->id);

        // Wait for the customer to pay
        while (t->paid < t->total) {
            pthread_cond_wait(&c->cond, &c->mutex);
        }

        printf("total: %d\n", total);
        printf("reciept total: %d\n", t->total);
        printf("paid: %d\n", t->paid);
        printf("customer_wallet: %d\n", customer_wallet);
        printf("expected wallet: %d\n", customer_wallet - total);
        printf("actual wallet: %d\n", c->wallet);

        assert(total == t->total); // check if the total is correct
        assert(t->paid == t->total); // check if the customer paid the correct amount
        assert(customer_wallet - total == c->wallet); // check if the customer paid the correct amount

        // Update the cash register
        self->cash_register += t->paid;

        printf("Clerk has been paid by customer %d\n", c->id);

        // Signal the customer that transaction is fully complete before releasing the mutex
        pthread_cond_signal(&c->cond);
        pthread_mutex_unlock(&c->mutex);
    }

    printf("Clerk %d has made %d\n dollars and is leaving the shop", self->id, self->cash_register);
    free(self);
    return NULL;
}

int main() {
    pthread_t customers[NUM_CUSTOMERS];
    pthread_t clerks[NUM_CLERKS];

    initialize_products();
    customer_queue = queue_create();

    // create customers
    for (int i = 0; i < NUM_CUSTOMERS; i++) {


        customer_t* c = (customer_t*)malloc(sizeof(customer_t)); // customer must remember to free itself
        if (c == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }

        c->myself = c;
        c->id = i;
        c->wallet = 100;
        c->reciecpt = NULL;

        // procedurally generate the shopping list
        c->shopping_list_size = i % 10 + 2;
        c->shopping_list = (int*)malloc(sizeof(int) * c->shopping_list_size);

        if (c->shopping_list == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }
        int pseudo_random = 1;
        for (int j = 0; j < c->shopping_list_size; j++) {
            pseudo_random = ((pseudo_random * (i + 1) * (j + 1)) % 50) + 1; // Range 1-50
            c->shopping_list[j] = pseudo_random - 1;
        }


        pthread_cond_init(&c->cond, NULL);
        pthread_mutex_init(&c->mutex, NULL);
        pthread_create(&customers[i], NULL, customer_thread, c);
    }

    // create clerks
    for (int i = 0; i < NUM_CLERKS; i++) {
        clerk_t* c = (clerk_t*)malloc(sizeof(clerk_t));
        if (c == NULL) {
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }
        c->id = i;
        c->cash_register = 0;
        pthread_create(&clerks[i], NULL, clerk_thread, c);

    }

    printf("All customers and clerks have been created\n");
    // Join all customer threads
    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_join(customers[i], NULL);
    }
    printf("All customers have left the shop\n");
    // Join all clerk threads
    for (int i = 0; i < NUM_CLERKS; i++) {
        pthread_join(clerks[i], NULL);
    }
    printf("All clerks have left the shop\n");
    queue_destroy(customer_queue);
    destroy_products();
    return 0;

}

