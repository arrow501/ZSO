#ifndef SHOP_H
#define SHOP_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "customer.h"

/**
 * Shop Module
 * 
 * This module provides the main shop simulation functionality,
 * coordinating customers, clerks, and assistant interactions
 * in a multi-threaded environment.
 */


// Structure to track customer objects for safe cleanup
typedef struct {
    customer_t* customer;
    pthread_t thread_id;
} customer_record_t;

// Global array to track all customer objects
extern customer_record_t* customer_records;

/**
 * Main shop simulation function.
 * Creates and manages all threads and resources for the shop simulation.
 * 
 * @return 0 on successful completion of the simulation
 */
int zso();

/* Global variables for customer spawning */
extern pthread_mutex_t spawner_mutex;
extern pthread_cond_t spawner_cond;
extern int active_customers;     // Currently active customer threads
extern int customers_spawned;    // Total customers created so far
extern int spawner_running;      // Flag to control spawner thread

/* Global variables for shop earnings */
extern pthread_mutex_t safe_mutex;
extern int shop_earnings;        // Total earnings collected from all clerks

/**
 * Signal that a customer has left the shop, allowing a new one to be created.
 */
void signal_customer_exit();

/**
 * Collects money from a clerk into the shop's safe.
 * 
 * @param amount Amount of money to add to the safe
 */
void deposit_to_safe(int amount);

#endif /* SHOP_H */