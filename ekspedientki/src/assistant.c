#include "assistant.h"
#include <stdio.h>
#include <stdlib.h>

/* Global Variables */
queue* assistant_queue;           // Queue for assistant tasks
pthread_t assistant_thread_id;    // Assistant thread ID
int assistant_running = 1;        // Flag to control assistant thread

/**
 * Simulates the work required to prepare a special product.
 * Uses CPU-intensive math operations to represent the preparation time.
 * 
 * @return A dummy result value representing the completed preparation
 */
double prepare_product() {
    // Simulate work with some math operations
    double result = 0;
    for (int i = 0; i < ASSISTANT_WORK_INTENSITY * 100; i++) {
        result += sin(i) * cos(i);
        if (i % ASSISTANT_WORK_INTENSITY == 0) {
            result = fmod(result, 10.0);  // Keep the number manageable
        }
    }
    return result;
}

/**
 * Main function for the assistant thread.
 * Continuously processes jobs from the queue until a SENTINEL_VALUE is received.
 * For each job, the assistant:
 * 1. Prepares the product (simulated work)
 * 2. Notifies the clerk that the product is ready
 * 3. Frees the job resources
 * 
 * @param arg Unused, should be NULL
 * @return Always returns NULL
 */
void* assistant_thread(void* arg) {
    (void)arg; // Suppress unused parameter warning
    
    #if ENABLE_PRINTING
    printf("Assistant has entered the shop\n");
    #endif

    while (assistant_running) {
        // Get a job from the queue (blocking operation)
        void* job_ptr = queue_pop(assistant_queue);
        
        // Check if this is the sentinel value signaling to stop
        if (job_ptr == SENTINEL_VALUE) {
            break;
        }
        
        assistant_job_t* job = (assistant_job_t*)job_ptr;
        
        #if ENABLE_PRINTING
        printf("Assistant is preparing product %d for clerk %d\n", job->product_id, job->clerk_id);
        #endif
        
        // Simulate the work of preparing the product
        double result = prepare_product();
        
        // Mark job as completed and notify the waiting clerk
        pthread_mutex_lock(job->mutex);
        *job->completed = 1;
        pthread_cond_signal(job->cond);
        pthread_mutex_unlock(job->mutex);
        
        #if ENABLE_PRINTING
        printf("Assistant finished preparing product %d (calculated %f)\n", job->product_id, result);
        #endif
        
        // Free the job structure after completion
        free(job);
    }
    
    #if ENABLE_PRINTING
    printf("Assistant is leaving the shop\n");
    #endif
    
    return NULL;
}
