#include <stdio.h>
#include <pthread.h>

// Shared data with built-in synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;
int max_count = 100;

void* increment_thread(void* arg) {
    int thread_id = *((int*)arg);
    int local_value;
    
    while (1) {
        // Critical section
        pthread_mutex_lock(&mutex);
        
        // Exit if we've reached the limit
        if (counter >= max_count) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        // Increment and save value atomically
        local_value = ++counter;
        
        pthread_mutex_unlock(&mutex);
        
        // Print outside the critical section
        printf("Thread %d incremented counter to %d\n", thread_id, local_value);
    }
    
    return NULL;
}

int main() {
    pthread_t threads[2];
    int thread_ids[2] = {1, 2};
    
    printf("Initial counter: %d\n", counter);
    
    // Create both threads
    pthread_create(&threads[0], NULL, increment_thread, &thread_ids[0]);
    pthread_create(&threads[1], NULL, increment_thread, &thread_ids[1]);
    
    // Print halfway point (mutex not needed here because we're just reading)
    printf("Halfway counter: %d\n", counter);
    
    // Wait for threads to finish
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    
    printf("Final counter: %d\n", counter);
    pthread_mutex_destroy(&mutex);
    
    return 0;
}