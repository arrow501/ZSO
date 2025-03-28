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

/**
 * Queue for assistant tasks.
 */
extern queue* assistant_queue;

/**
 * Assistant thread ID.
 */
extern pthread_t assistant_thread_id;

/**
 * Global synchronization primitives for assistant-clerk communication.
 * Each clerk accesses these arrays using their clerk ID.
 */
extern pthread_cond_t clerk_cond_vars[NUM_CLERKS];   // One condition variable per clerk
extern pthread_mutex_t assistant_mutex;              // Single mutex for all synchronization
extern int assistant_job_completed[NUM_CLERKS];      // Completion status for each clerk (1 = completed)

/**
 * Represents a job for the assistant to process.
 */
typedef struct assistant_job_t {
    int product_id;           // Product that needs assistance
    int clerk_id;             // ID of the clerk requesting assistance
} assistant_job_t;

/**
 * Main function for the assistant thread.
 * Processes jobs from the assistant_queue until receiving a SENTINEL_VALUE.
 * 
 * @param arg Unused, should be NULL
 * @return Always returns NULL
 */
void* assistant_thread(void* arg);

/**
 * Initialize assistant synchronization primitives.
 * Must be called once before any clerk or assistant threads start.
 */
void initialize_assistant_sync(void);

/**
 * Clean up assistant synchronization primitives.
 * Should be called when shutting down the shop.
 */
void cleanup_assistant_sync(void);

#endif /* ASSISTANT_H */
