#include "assistant.h"
#include "customer.h" // Include for printf_mutex
#include <stdio.h>
#include <stdlib.h>

/* Global Variables */
queue* assistant_queue = NULL;    // Queue for assistant tasks
queue** clerk_inboxes = NULL;     // Array of queues, one per clerk
pthread_t assistant_thread_id;    // Assistant thread ID
int assistant_running = 1;        // Flag to control assistant thread
static int next_job_id = 0;       // Counter for job IDs

/**
 * Initialize clerk inboxes
 */
void initialize_clerk_inboxes() {
    clerk_inboxes = (queue**)malloc(sizeof(queue*) * NUM_CLERKS);
    if (clerk_inboxes == NULL) {
        fprintf(stderr, "Error: malloc failed for clerk inboxes\n");
        exit(1);
    }
    
    for (int i = 0; i < NUM_CLERKS; i++) {
        clerk_inboxes[i] = queue_create();
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Initialized %d clerk inboxes\n", NUM_CLERKS);
    pthread_mutex_unlock(&printf_mutex);
    #endif
}

/**
 * Clean up clerk inboxes
 */
void cleanup_clerk_inboxes() {
    if (clerk_inboxes == NULL) {
        return;
    }
    
    for (int i = 0; i < NUM_CLERKS; i++) {
        queue_destroy(clerk_inboxes[i]);
    }
    
    free(clerk_inboxes);
    clerk_inboxes = NULL;
}

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
    job->job_id = __sync_fetch_and_add(&next_job_id, 1); // Atomic increment
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d created job %d for product %d\n", clerk_id, job->job_id, product_id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    return job;
}

/**
 * Waits for all jobs created by this clerk to complete.
 * The pending_jobs parameter indicates how many jobs the clerk is waiting for.
 */
void wait_for_clerk_jobs(int clerk_id, int pending_jobs) {
    if (pending_jobs <= 0) {
        return; // No jobs to wait for
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d waiting for %d assistant jobs to complete\n", clerk_id, pending_jobs);
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    // Wait for the specified number of jobs to be completed
    for (int i = 0; i < pending_jobs; i++) {
        // This is a blocking call that waits until a job is available in the clerk's inbox
        assistant_job_t* job = (assistant_job_t*)queue_pop(clerk_inboxes[clerk_id]);
        
        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Clerk %d received completed job %d for product %d\n", 
               clerk_id, job->job_id, job->product_id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        // Free the job
        free_assistant_job(job);
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Clerk %d finished waiting for assistant jobs\n", clerk_id);
    pthread_mutex_unlock(&printf_mutex);
    #endif
}

/**
 * Cleans up resources used by an assistant job.
 */
void free_assistant_job(assistant_job_t* job) {
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
        printf("Assistant is preparing product %d for clerk %d (job %d)\n", 
               job->product_id, job->clerk_id, job->job_id);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        // Simulate the work of preparing the product
        #if ENABLE_ASSERTS || ENABLE_PRINTING
        double result = prepare_product();
        #else
        prepare_product();
        #endif

        #if ENABLE_PRINTING
        pthread_mutex_lock(&printf_mutex);
        printf("Assistant finished preparing product %d (job %d, calculated %f)\n", 
               job->product_id, job->job_id, result);
        pthread_mutex_unlock(&printf_mutex);
        #endif
        
        // Add the completed job to the clerk's inbox
        queue_push(clerk_inboxes[job->clerk_id], job);
    }
    
    #if ENABLE_PRINTING
    pthread_mutex_lock(&printf_mutex);
    printf("Assistant is leaving the shop\n");
    pthread_mutex_unlock(&printf_mutex);
    #endif
    
    return NULL;
}
