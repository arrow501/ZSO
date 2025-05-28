#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include "../include/common.h"

static volatile sig_atomic_t should_exit = 0;

static void handle_signal(int sig) {
    (void)sig;
    should_exit = 1;
}

static void display_stats(const stats_t *stats) {
    DEBUG_ASSERT(stats != NULL, "Stats pointer should not be NULL");
    DEBUG_ASSERT(stats->magic == STATS_MAGIC, "Stats magic should be valid");
    
    printf("\n=== Master Statistics (from shared memory) ===\n");
    printf("Time: %s", ctime(&(time_t){time(NULL)}));
    
    int total_sent = 0, total_received = 0;
    
    printf("\nActive slaves:\n");
    for (int i = 0; i < MAX_SLAVES; i++) {
        if (stats->active_slaves[i]) {
            DEBUG_ASSERT(stats->slave_pids[i] > 0, "Active slave should have valid PID");
            
            printf("  Slave %d: PID=%d, sent=%d, received=%d\n",
                   i, stats->slave_pids[i], 
                   stats->messages_sent[i], 
                   stats->messages_received[i]);
            total_sent += stats->messages_sent[i];
            total_received += stats->messages_received[i];
        }
    }
    
    printf("\nTotal messages: sent=%d, received=%d\n", 
           total_sent, total_received);
    printf("==============================================\n");
}

int main(void) {
    stats_t *stats = NULL;
    sem_t *stats_ready_sem = NULL;
    
    // Set up signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Open shared memory
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        fprintf(stderr, "Is the master process running?\n");
        return 1;
    }
    
    // Map shared memory for reading - but we need write access for mutex
    stats = mmap(NULL, sizeof(stats_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    
    if (stats == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    
    DEBUG_ASSERT(stats != NULL && stats != MAP_FAILED, "Stats mapping should be valid");
    
    // Verify stats are initialized
    if (stats->magic != STATS_MAGIC) {
        fprintf(stderr, "ERROR: Stats not properly initialized (magic=%x, expected=%x)\n",
                stats->magic, STATS_MAGIC);
        munmap(stats, sizeof(stats_t));
        return 1;
    }
    
    // Open semaphore
    stats_ready_sem = sem_open(SEM_NAME, 0);
    if (stats_ready_sem == SEM_FAILED) {
        perror("sem_open");
        munmap(stats, sizeof(stats_t));
        return 1;
    }
    
    DEBUG_ASSERT(stats_ready_sem != SEM_FAILED, "Semaphore should be opened successfully");
    
    printf("Stats reader: Waiting for statistics updates...\n");
    printf("(Send SIGUSR1 to master process to trigger update)\n");
    
    while (!should_exit) {
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 2;  // 2 second timeout
        
        // Wait for stats to be ready (with timeout)
        int ret = sem_timedwait(stats_ready_sem, &timeout);
        
        if (ret == 0) {
            // Stats are ready, display them safely
            pthread_mutex_lock((pthread_mutex_t *)&stats->mutex);
            display_stats(stats);
            pthread_mutex_unlock((pthread_mutex_t *)&stats->mutex);
        } else if (errno != ETIMEDOUT) {
            perror("sem_timedwait");
            break;
        }
    }
    
    // Cleanup
    if (stats_ready_sem != NULL) {
        sem_close(stats_ready_sem);
    }
    if (stats != NULL) {
        munmap(stats, sizeof(stats_t));
    }
    
    printf("\nStats reader: Exiting\n");
    return 0;
}
