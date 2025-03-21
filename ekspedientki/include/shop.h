#ifndef SHOP_H
#define SHOP_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/**
 * Shop Module
 * 
 * This module provides the main shop simulation functionality,
 * coordinating customers, clerks, and assistant interactions
 * in a multi-threaded environment.
 */

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

/**
 * Signal that a customer has left the shop, allowing a new one to be created.
 */
void signal_customer_exit();

#endif /* SHOP_H */