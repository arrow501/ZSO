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
 * Clerk inbox queues. Each clerk has their own inbox for receiving completed jobs.
 */
extern queue** clerk_inboxes;

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
    int job_id;               // Unique ID for this job
} assistant_job_t;

/**
 * Initialize clerk inboxes. Call this before starting the assistant thread.
 */
void initialize_clerk_inboxes();

/**
 * Clean up clerk inboxes. Call this after all clerks have finished.
 */
void cleanup_clerk_inboxes();

/**
 * Creates a new assistant job.
 * 
 * @param product_id ID of the product requiring assistance
 * @param clerk_id ID of the clerk requesting assistance
 * @return Pointer to the newly created job
 */
assistant_job_t* create_assistant_job(int product_id, int clerk_id);

/**
 * Wait for all assistant jobs created by this clerk to complete.
 * 
 * @param clerk_id ID of the clerk whose jobs to wait for
 * @param pending_jobs Number of jobs the clerk is waiting for
 */
void wait_for_clerk_jobs(int clerk_id, int pending_jobs);

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
