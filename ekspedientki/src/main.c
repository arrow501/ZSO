#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

// Function to be executed by the thread
void *thread_function(void *arg) {
    int thread_id = *((int *)arg);
    printf("Thread %d is running\n", thread_id);
    sleep(1);
    printf("Thread %d is finished\n", thread_id);
    return NULL;
}

int main() {
    pthread_t threads[3];
    int thread_args[3] = {1, 2, 3};
    int i;

    printf("Creating threads...\n");

    // Create threads
    for (i = 0; i < 3; i++) {
        int result = pthread_create(&threads[i], NULL, thread_function, &thread_args[i]);
        if (result != 0) {
            printf("Error creating thread %d: %d\n", i, result);
            return 1;
        }
        printf("Thread %d created successfully\n", i + 1);
    }

    // Wait for all threads to complete
    for (i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All threads have completed\n");
    return 0;
}