#include "queue.h"
#include "product.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_CUSTOMERS 1
#define NUM_CLERKS 1


// Define the transaction for passing between customer and clerk
typedef struct clerk_t {
    int id;
    int cash_register;

} clerk_t;


typedef struct transaction_t{
    int total;
    int paid;
    int* items;
    int items_size;
    clerk_t* clerk;
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



void* customer_thread(void* arg){
    customer_t* self = (customer_t*)arg;
    
    printf("Customer %d has entered the shop\n", self->id);
    // Add myself to the queue
    queue_push(customer_queue, self);

    //Q: how to wait util cleark pops me and is ready to serve me
    //A: use a condition variable

    // wait for the clerk to serve me
    pthread_mutex_lock(&self->mutex);
    pthread_cond_wait(&self->cond, &self->mutex);
    // check the reciept
    self->wallet -= self->reciecpt->total;
    self->reciecpt->paid = self->reciecpt->total;
    // signal the clerk that I have paid
    pthread_cond_signal(&self->cond);
    // pay the clerk
    printf("Customer %d is paying the clerk\n", self->id);
    pthread_mutex_unlock(&self->mutex);

    // leave the shop
    printf("Customer %d has left the shop\n", self->id);


    // Clean up my properties
    free(self->reciecpt->items);
    free(self->reciecpt);
    free(self->shopping_list);
    pthread_mutex_destroy(&self->mutex);
    pthread_cond_destroy(&self->cond);
    free(self);

    return NULL;
}

void* clerk_thread(void* arg){
    clerk_t* self = (clerk_t*)arg;
    printf("Clerk %d has entered the shop\n", self->id);

    // pop customer from queue; this is blocking
    customer_t* c = (customer_t*)queue_pop(customer_queue);
    printf("Clerk is serving customer %d\n", c->id);
    
    
    
    // create an array for the reciept
    int* reciept = (int*)malloc(sizeof(int)*c->shopping_list_size);
    if(reciept == NULL){
        fprintf(stderr, "Error: malloc failed\n");
        exit(1);
    }
    
    // ring up the customer
    int total = 0, item_count = 0;

    printf("Clerk %d\n is ringing up customer %d\n",self->id, c->id);
    for(int i = 0; i < c->shopping_list_size; i++){
        int product_id = c->shopping_list[i];
        
        bool in_stock = try_get_product(product_id); 
        if(in_stock){
            total += get_product_price(product_id);
            reciept[item_count++] = product_id;
        }
    }
    // trim the reciept
    reciept = realloc(reciept, sizeof(int)*item_count);
    if(reciept == NULL){
        fprintf(stderr, "Error: realloc failed\n");
        exit(1);
    }
    // create a transaction
    transaction_t* t = (transaction_t*)malloc(sizeof(transaction_t));
    if(t == NULL){
        fprintf(stderr, "Error: malloc failed\n");
        exit(1);
    }
    t->total = total;
    t->items = reciept;
    t->items_size = item_count;
    t->clerk = self;

    pthread_mutex_lock(&c->mutex); // lock the customer
    c->reciecpt = t; // give the reciept to the customer
    int customer_wallet = c->wallet;

    // signal the customer to pay    
    pthread_cond_signal(&c->cond); // signal the customer, does itunlock the customer?
    
    printf("Clerk is waiting for customer %d to pay\n", c->id);
    
    // wait for the customer to pay
    while(t->paid < t->total){
        pthread_cond_wait(&c->cond, &c->mutex);
    }
    assert(customer_wallet - total == c->wallet); // check if the customer paid the correct amount
    // consider implementing change

    // update the cash register
    self->cash_register += t->paid;
    
    
    printf("Clerk has been paid by customer %d\n", c->id);

    pthread_mutex_unlock(&c->mutex); // unlock the customer

    //Q: should i free the reciept here?
    //A: no, the customer will free it

    // clerk is ready to serve next customer
    
    return NULL;
}

int main(){
    pthread_t custormers[NUM_CUSTOMERS];
    pthread_t clerks[NUM_CLERKS];

    initialize_products();
    customer_queue = queue_create();

    // create customers
    for(int i = 0; i < NUM_CUSTOMERS; i++){
        customer_t* c = (customer_t*)malloc(sizeof(customer_t)); // customer must remember to free itself
        if(c == NULL){
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }
        
        c->myself = c;
        c->id = i;
        c->wallet = 100; 
        c->shopping_list = (int*)malloc(sizeof(int)*10);
        c->shopping_list_size = 10;
        c->reciecpt = NULL;

        if(c->shopping_list == NULL){
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }

        pthread_cond_init(&c->cond, NULL);
        pthread_mutex_init(&c->mutex, NULL);
        pthread_create(&custormers[i], NULL, customer_thread, c);        
    }
    
    // create clerks
    for(int i = 0; i < NUM_CLERKS; i++){
        clerk_t* c = (clerk_t*)malloc(sizeof(clerk_t));
        if(c == NULL){
            fprintf(stderr, "Error: malloc failed\n");
            exit(1);
        }
        c->id = i;
        c->cash_register = 0;
        // pthread_cond_init(&c->cond, NULL);
        // pthread_mutex_init(&c->mutex, NULL);
        pthread_create(&clerks[i], NULL, clerk_thread, c);
        
    }

    pthread_join(custormers[0], NULL);
    pthread_join(clerks[0], NULL);

    queue_destroy(customer_queue);
    return 0;

}

