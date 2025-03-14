#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 10

// Shared synchronization variables
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turn_cond = PTHREAD_COND_INITIALIZER;
int current_turn = 0;

void* thread_function(void* arg) {
    int my_id = *((int*)arg);
    
    for (int i = 0; i < 5; i++) {  // Each thread will take 5 turns
        // Enter critical section
        pthread_mutex_lock(&mutex);
        
        // Wait until it's my turn
        while (current_turn != my_id) {
            pthread_cond_wait(&turn_cond, &mutex);
        }
        
        // Do work
        printf("Thread %d's turn (#%d)\n", my_id, i);
        
        // Pass the turn to the next thread
        current_turn = (my_id + 1) % NUM_THREADS;
        
        // Signal all waiting threads
        pthread_cond_broadcast(&turn_cond);
        
        // Exit critical section
        pthread_mutex_unlock(&mutex);
        
        // Optional: simulate some work
        usleep(100000);  // 100ms
    }
    
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]);
    }
    
    // Wait for all threads to finish
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Clean up
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&turn_cond);
    
    return 0;
}