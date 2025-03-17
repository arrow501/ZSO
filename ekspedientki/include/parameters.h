#ifndef PARAMETERS_H
#define PARAMETERS_H

/**
 * Parameters Module
 * 
 * This module defines global configuration parameters used
 * throughout the shop simulation system.
 */

/** Number of customers in the simulation */
#define NUM_CUSTOMERS 100 // Any positive integer

/** Number of clerks serving customers */
#define NUM_CLERKS 3  // Any positive integer

/** Scales the work needed to prepare a product */
#define ASSISTANT_WORK_INTENSITY 100 // Any positive integer

/** Controls debug output (1 = enabled, 0 = disabled) */
#define ENABLE_PRINTING 1 // Set to 0 to disable all print statements

/** Controls runtime assertions (1 = enabled, 0 = disabled) */
#define ENABLE_ASSERTS 1 // Set to 0 to disable all assert statements

/** Special value to signify the end of a queue */
#define SENTINEL_VALUE ((void*)(-1))

#endif /* PARAMETERS_H */