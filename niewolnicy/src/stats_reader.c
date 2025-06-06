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
static pid_t master_pid = 0;

static void handle_signal(int sig) {
    should_exit = 1;
}

static pid_t read_master_pid(void) {
    FILE *f = fopen(MASTER_PID_FILE, "r");
    DEBUG_ASSERT(f != NULL, "Should be able to read PID file");
    
    pid_t pid;
    int ret = fscanf(f, "%d", &pid);
    fclose(f);
    
    DEBUG_ASSERT(ret == 1, "Should read PID from file");
    DEBUG_ASSERT(pid > 0, "PID should be valid");
    
    return pid;
}

static void display_stats(const stats_t *stats) {
    DEBUG_ASSERT(stats != NULL, "Stats should not be NULL");
    DEBUG_ASSERT(stats->magic == STATS_MAGIC, "Stats should be valid");
    
    time_t now = time(NULL);
    printf("\n=== Master Statistics ===\n");
    printf("Time: %s", ctime(&now));
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

static void request_stats(void) {
    DEBUG_ASSERT(master_pid > 0, "Master PID should be valid");
    
    if (kill(master_pid, SIGUSR1) == 0) {
        printf("Stats request sent to master (PID %d)\n", master_pid);
    } else {
        perror("Failed to send signal to master");
        printf("Master may have exited\n");
    }
}

static void wait_and_display_stats(stats_t *stats, sem_t *sem) {
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 2;  // 2 second timeout
    
    int ret = sem_timedwait(sem, &timeout);
    
    if (ret == 0) {
        // Stats are ready
        pthread_mutex_lock(&stats->mutex);
        display_stats(stats);
        pthread_mutex_unlock(&stats->mutex);
    } else if (errno == ETIMEDOUT) {
        printf("Timeout waiting for stats update\n");
    } else {
        perror("sem_timedwait");
    }
}

int main(void) {
    stats_t *stats = NULL;
    sem_t *stats_sem = NULL;
    
    // Setup signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Read master PID
    master_pid = read_master_pid();
    printf("Stats Reader: Master PID is %d\n", master_pid);
    
    // Open shared memory
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    DEBUG_ASSERT(shm_fd >= 0, "Should open shared memory");
    
    stats = mmap(NULL, sizeof(stats_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    DEBUG_ASSERT(stats != MAP_FAILED, "Should map shared memory");
    DEBUG_ASSERT(stats->magic == STATS_MAGIC, "Stats should be initialized");
    
    // Open semaphore
    stats_sem = sem_open(SEM_NAME, 0);
    DEBUG_ASSERT(stats_sem != SEM_FAILED, "Should open semaphore");
    
    printf("\nStats Reader: Ready\n");
    printf("Commands:\n");
    printf("  ENTER - Request and display stats\n");
    printf("  q + ENTER - Quit\n");
    printf("> ");
    fflush(stdout);
    
    // Interactive loop
    while (!should_exit) {
        char input[256];
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        if (input[0] == 'q' && (input[1] == '\n' || input[1] == '\0')) {
            break;
        }
        
        if (input[0] == '\n') {
            request_stats();
            wait_and_display_stats(stats, stats_sem);
        }
        
        printf("> ");
        fflush(stdout);
    }
    
    // Cleanup
    if (stats_sem != NULL) {
        sem_close(stats_sem);
    }
    if (stats != NULL) {
        munmap(stats, sizeof(stats_t));
    }
    
    printf("\nStats Reader: Exiting\n");
    return 0;
}