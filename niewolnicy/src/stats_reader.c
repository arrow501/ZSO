#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include "../include/common.h"

static volatile sig_atomic_t should_exit = 0;

static void handle_signal(int sig) {
    should_exit = 1;
}

static void display_stats(const stats_t *stats) {
    DEBUG_ASSERT(stats != NULL, "Stats should not be NULL");
    DEBUG_ASSERT(stats->magic == STATS_MAGIC, "Stats should be valid");
    
    printf("\n=== Master Statistics ===\n");
    printf("Master PID: %d\n", stats->master_pid);
    
    int total_sent = 0, total_received = 0, active_count = 0;
    
    printf("\nSlave Status:\n");
    for (int i = 0; i < NUM_SLAVES; i++) {
        if (stats->active_slaves[i]) {
            printf("  Slave %d: ACTIVE, sent=%d, received=%d\n", 
                   i, stats->messages_sent[i], stats->messages_received[i]);
            active_count++;
        } else {
            printf("  Slave %d: INACTIVE\n", i);
        }
        total_sent += stats->messages_sent[i];
        total_received += stats->messages_received[i];
    }
    
    printf("\nTotals: %d active slaves, %d sent, %d received\n", 
           active_count, total_sent, total_received);
    printf("========================\n");
}

static void wait_and_display_stats(stats_t *stats, sem_t *sem) {
    // Simple blocking wait without timeout
    int ret = sem_wait(sem);
    
    if (ret == 0) {
        pthread_mutex_lock(&stats->mutex);
        display_stats(stats);
        pthread_mutex_unlock(&stats->mutex);
    } else {
        perror("sem_wait");
    }
}

int setup_stats_monitoring(pid_t master_pid, stats_t **stats_ptr, sem_t **sem_ptr) {
    // Open shared memory
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        return -1;
    }
    
    *stats_ptr = mmap(NULL, sizeof(stats_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    
    if (*stats_ptr == MAP_FAILED) {
        perror("mmap");
        return -1;
    }
    
    DEBUG_ASSERT((*stats_ptr)->magic == STATS_MAGIC, "Stats should be initialized");
    
    // Open semaphore
    *sem_ptr = sem_open(SEM_NAME, 0);
    if (*sem_ptr == SEM_FAILED) {
        perror("sem_open");
        munmap(*stats_ptr, sizeof(stats_t));
        return -1;
    }
    
    printf("Stats monitoring setup for master PID %d\n", master_pid);
    return 0;
}

int trigger_and_display_stats(pid_t master_pid, stats_t *stats, sem_t *sem) {
    if (kill(master_pid, SIGUSR1) != 0) {
        perror("Failed to send signal to master");
        return -1;
    }
    
    printf("Stats request sent to master (PID %d)\n", master_pid);
    wait_and_display_stats(stats, sem);
    return 0;
}