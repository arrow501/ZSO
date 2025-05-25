#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include "stats.h"
#include "parameters.h"

static volatile int should_exit = 0;

static void signal_handler(int sig) {
    (void)sig;
    should_exit = 1;
}

int main() {
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Open shared memory
    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        fprintf(stderr, "Is master running?\n");
        return 1;
    }
    
    // Map shared memory
    stats_t *stats = mmap(NULL, sizeof(stats_t), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (stats == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return 1;
    }
    
    close(shm_fd);
    
    // Open semaphore
    sem_t *stats_sem = sem_open(SEM_NAME, 0);
    if (stats_sem == SEM_FAILED) {
        perror("sem_open");
        munmap(stats, sizeof(stats_t));
        return 1;
    }
    
    printf("Stats reader started. Send SIGUSR1 to master (or press Ctrl+C to exit)\n");
    
    // Wait for stats to be ready
    while (!should_exit) {
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1;  // 1 second timeout
        
        if (sem_timedwait(stats_sem, &timeout) == 0) {
            // Stats are ready, display them
            pthread_mutex_lock(&stats->mutex);
            
            printf("\n=== Master Statistics (from shared memory) ===\n");
            printf("Total messages sent: %d\n", stats->total_sent);
            printf("Total messages received: %d\n", stats->total_received);
            
            for (int i = 0; i < MAX_SLAVES; i++) {
                if (stats->messages_sent[i] > 0 || stats->messages_received[i] > 0) {
                    printf("Slave %d: sent=%d, received=%d\n", 
                           i, stats->messages_sent[i], stats->messages_received[i]);
                }
            }
            printf("==============================================\n\n");
            
            pthread_mutex_unlock(&stats->mutex);
        }
    }
    
    // Cleanup
    sem_close(stats_sem);
    munmap(stats, sizeof(stats_t));
    
    return 0;
}
