#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_CUSTOMERS 1
#define NUM_CLERKS 1

// Forward declaration
struct customer;

// Define the structure
typedef struct customer {
    struct customer* myself;
    int id;
    int wallet;
    // int* shopping_list;
    // int shopping_list_size;
    // int* reciept;
    // int reciept_size;

    pthread_cond_t cond;
    pthread_mutex_t mutex;
} customer;


// typedef struct clerk
// {
    
// } clerk;

// typedef struct transaction
// {
//     customer* c;
//     clerk* cl;
//     int total;
//     int* shopping_list;
//     int shopping_list_size;
//     int* reciept;
//     int reciept_size;
// } transaction;


queue* customer_queue;

void* customer_thread(void* arg){
    customer* self = (customer*)arg;
    
    printf("Customer %d has entered the shop\n", self->id);
    // Add myself to the queue
    queue_push(customer_queue, self);

    //Q: how to wait util cleark pops me and is ready to serve me
    //A: use a condition variable

    // wait for the clerk to serve me
    pthread_mutex_lock(&self->mutex);
    pthread_cond_wait(&self->cond, &self->mutex);

    // pay the clerk
    printf("Customer %d is paying the clerk\n", self->id);
    pthread_mutex_unlock(&self->mutex);

    // leave the shop
    printf("Customer %d has left the shop\n", self->id);

    free(self);
    return NULL;

}

void* clerk_thread(void* arg){
    // clerk* self = (clerk*)arg;

  
    // pop customer from queue; this is blocking
    customer* c = (customer*)queue_pop(customer_queue);
    printf("Clerk is serving customer %d\n", c->id);

    
    // signal customer to pay
    pthread_mutex_lock(&c->mutex); // lock the customer
    
    pthread_cond_signal(&c->cond); // signal the customer, does itunlock the customer?
    
    printf("Clerk is waiting for customer %d to pay\n", c->id);
    
    // wait loop with another condition variable
    
    printf("Clerk has been paid by customer %d\n", c->id);

    pthread_mutex_unlock(&c->mutex); // unlock the customer

    // clerk is ready to serve next customer
    
    return NULL;
}

int main(){
    pthread_t custormer;
    pthread_t clerk;

    customer_queue = queue_create();

    // create customers
    for(int i = 0; i < NUM_CUSTOMERS; i++){
        customer* c = (customer*)malloc(sizeof(customer)); // customer must remember to free itself
        c->myself = c;
        c->id = i;
        c->wallet = 100; 
        pthread_cond_init(&c->cond, NULL);
        pthread_mutex_init(&c->mutex, NULL);
        pthread_create(&custormer, NULL, customer_thread, c);        
    }
    
    // create clerks
    for(int i = 0; i < NUM_CLERKS; i++){
        // clerk* c = (clerk*)malloc(sizeof(clerk));
        // c->id = i;
        // pthread_create(&clerk, NULL, clerk_thread, c);
        pthread_create(&clerk, NULL, clerk_thread, NULL);
    }

    pthread_join(custormer, NULL);
    pthread_join(clerk, NULL);

    queue_destroy(customer_queue);
    return 0;

}

