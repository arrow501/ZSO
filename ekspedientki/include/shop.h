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

#endif /* SHOP_H */