#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <pthread.h>
#include "transaction.h"

// Define the customer structure
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

// Function declarations
void* customer_thread(void* arg);

// Random number generator helper used for customer creation
unsigned int get_pseudo_random(unsigned int seed, int min, int max);

#endif /* CUSTOMER_H */
