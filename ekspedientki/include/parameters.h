#ifndef PARAMETERS_H
#define PARAMETERS_H

// Number of customers and clerks
#define NUM_CUSTOMERS 100 // Any positive integer
#define NUM_CLERKS 3  // Any positive integer

// Scales the work needed to prepare a product
#define ASSISTANT_WORK_INTENSITY 100 // Any positive integer

#define ENABLE_PRINTING 1 // Set to 0 to disable all print statements
#define ENABLE_ASSERTS 1 // Set to 0 to disable all assert statements

// Special value to signify the end of a queue
#define SENTINEL_VALUE ((void*)(-1))

#endif