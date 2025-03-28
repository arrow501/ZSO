#include "assistant.h"
#include "customer.h" // Include for printf_mutex
#include <stdio.h>
#include <stdlib.h>

/* Global Variables */
queue* assistant_queue = NULL;        // Queue for assistant tasks
pthread_t assistant_thread_id;        // Assistant thread ID
int assistant_running = 1;            // Flag to control assistant thread

/**
 * Creates a new assistant job with proper initialization.
 */
assistant_job_t* create_assistant_job(int product_id, int clerk_id) {
    assistant_job_t* job = malloc(sizeof(assistant_job_t));
    if (!job) {
        fprintf(stderr, "Error: malloc failed for assistant job\n");
        exit(1);
    }
    
    job->product_id = product_id;
    job->clerk_id = clerk_id;
    job->completed = false;
    
    // Create synchronization primitives for this job
    job->mutex = malloc(sizeof(pthread_mutex_t));
    job->cond = malloc(sizeof(pthread_cond_t));
    
    if (!job->mutex || !job->cond) {
        fprintf(stderr, "Error: malloc failed for synchronization primitives\n");
        exit(1);
    }
    
    pthread_mutex_init(job->mutex, NULL);
    pthread_cond_init(job->cond, NULL);
    
    return job;
}

/**
 * Waits for an assistant job to complete. Uses a descriptive condition.
 */
void wait_for_assistant_job(assistant_job_t* job) {
    pthread_mutex_lock(job->mutex);
    
    // Wait until job is marked as completed
    while (!job->completed) {
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Clerk %d waiting for assistant to prepare product %d\n", job->clerk_id, job->product_id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        pthread_cond_wait(job->cond, job->mutex);
    }
    
    pthread_mutex_unlock(job->mutex);
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d received prepared product %d\n", job->clerk_id, job->product_id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
}

/**
 * Cleans up resources used by an assistant job.
 */
void free_assistant_job(assistant_job_t* job) {
    pthread_mutex_destroy(job->mutex);
    pthread_cond_destroy(job->cond);
    free(job->mutex);
    free(job->cond);
    free(job);
}

/**
 * Simulates the work required to prepare a special product.
 * Uses CPU-intensive math operations to represent the preparation time.
 * 
 * @return A dummy result value representing the completed preparation
 */
static double prepare_product(void) {
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
 */
void* assistant_thread(void* arg) {
    (void)arg; // Suppress unused parameter warning
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Assistant has entered the shop\n");
    pthread_mutex_unlock(&printf_mutex);
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
        pthread_mutex_lock(&printf_mutex);
        printf("Assistant is preparing product %d for clerk %d\n", job->product_id, job->clerk_id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        // Simulate the work of preparing the product
        #if ENABLE_ASSERTS || ENABLE_PRINTING
        double result = prepare_product();
        #else
        prepare_product();
        #endif

        // Mark job as completed and notify the waiting clerk
        pthread_mutex_lock(job->mutex);
        job->completed = true;
        pthread_cond_signal(job->cond);
        pthread_mutex_unlock(job->mutex);
        
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Assistant finished preparing product %d (calculated %f)\n", job->product_id, result);
        pthread_mutex_unlock(&printf_mutex);
        #endif
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Assistant is leaving the shop\n");
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    return NULL;
}
