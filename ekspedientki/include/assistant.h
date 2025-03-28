#ifndef ASSISTANT_H
#define ASSISTANT_H

#include <pthread.h>
#include <math.h>
#include <stdbool.h>

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
 * Represents a job for the assistant to process.
 */
typedef struct assistant_job_t {
    int product_id;           // Product that needs assistance
    int clerk_id;             // ID of the clerk requesting assistance
    bool completed;           // Flag to indicate completion (true = completed)
    pthread_mutex_t* mutex;   // Mutex for synchronization
    pthread_cond_t* cond;     // Condition variable for signaling completion
} assistant_job_t;

/**
 * Creates a new assistant job.
 * 
 * @param product_id ID of the product requiring assistance
 * @param clerk_id ID of the clerk requesting assistance
 * @return Pointer to the newly created job
 */
assistant_job_t* create_assistant_job(int product_id, int clerk_id);

/**
 * Wait for an assistant job to complete.
 * 
 * @param job Pointer to the assistant job
 */
void wait_for_assistant_job(assistant_job_t* job);

/**
 * Clean up an assistant job after completion.
 * 
 * @param job Pointer to the assistant job
 */
void free_assistant_job(assistant_job_t* job);

/**
 * Main function for the assistant thread.
 * Processes jobs from the assistant_queue until receiving a SENTINEL_VALUE.
 * 
 * @param arg Unused, should be NULL
 * @return Always returns NULL
 */
void* assistant_thread(void* arg);

#endif /* ASSISTANT_H */
