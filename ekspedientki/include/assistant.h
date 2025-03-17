#ifndef ASSISTANT_H
#define ASSISTANT_H

#include <pthread.h>
#include <math.h>

#include "queue.h"
#include "parameters.h"

/**
 * Assistant Module
 * 
 * This module provides functionality for a shop assistant who helps clerks
 * prepare special products that require additional processing.
 * The assistant runs in a separate thread and processes jobs from a queue.
 */

/* Global Variables */
extern queue* assistant_queue;           // Queue for assistant tasks
extern pthread_t assistant_thread_id;    // Assistant thread ID

/**
 * Represents a job for the assistant to process.
 * Each job corresponds to a product that needs special preparation.
 */
typedef struct assistant_job_t {
    int product_id;           // Product that needs assistance
    int clerk_id;             // ID of the clerk requesting assistance
    pthread_mutex_t* mutex;   // Mutex for synchronization
    pthread_cond_t* cond;     // Condition variable for signaling completion
    int* completed;           // Flag to indicate completion (1 = completed)
} assistant_job_t;

/**
 * Main function for the assistant thread.
 * Processes jobs from the assistant_queue until receiving a SENTINEL_VALUE.
 * 
 * @param arg Unused, should be NULL
 * @return Always returns NULL
 */
void* assistant_thread(void* arg);

#endif
