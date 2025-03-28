#ifndef PARAMETERS_H
#define PARAMETERS_H

/**
 * Parameters Module
 * 
 * This module defines global configuration parameters used
 * throughout the shop simulation system.
 */

/** Number of simulations to run */
#ifndef NUM_SIMULATIONS
#define NUM_SIMULATIONS 10 // Any positive integer
#endif

/** Number of customers in the simulation */
#ifndef NUM_CUSTOMERS
#define NUM_CUSTOMERS 20 // Any positive integer
#endif

/* Maximum number of concurrent customers in the shop */
#ifndef MAX_CONCURRENT_CUSTOMERS
#define MAX_CONCURRENT_CUSTOMERS 10 // Any positive integer, larger values may not be compatible with your system
#endif

/** Number of clerks serving customers */
#ifndef NUM_CLERKS
#define NUM_CLERKS 3  // Should be less than or equal to the number of concurent customers
#endif

/** Scales the work needed to prepare a product */
#ifndef ASSISTANT_WORK_INTENSITY
#define ASSISTANT_WORK_INTENSITY 10 // Any positive integer
#endif

/** Controls debug output (1 = enabled, 0 = disabled) */
#ifndef ENABLE_PRINTING
#define ENABLE_PRINTING 1 // Set to 0 to disable all print statements
#endif

/** Controls debug output (1 = enabled, 0 = disabled) */
#ifndef ENABLE_ASSERTS
#define ENABLE_ASSERTS 1 // Set to 0 to disable all assert statements
#endif

/** Special value to signify the end of a queue */
#define SENTINEL_VALUE ((void*)(-1))

#endif /* PARAMETERS_H */