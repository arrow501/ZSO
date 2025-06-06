#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <pthread.h>
#include "../include/common.h"

// Stats monitoring functions from stats_reader
int setup_stats_monitoring(pid_t master_pid, stats_t **stats_ptr, sem_t **sem_ptr);
int trigger_and_display_stats(pid_t master_pid, stats_t *stats, sem_t *sem);

// Global state for cleanup
static pid_t master_pid = 0;
static pid_t slave_pids[NUM_SLAVES];
static int num_slaves_started = 0;
static volatile sig_atomic_t should_exit = 0;
static stats_t *stats = NULL;
static sem_t *stats_sem = NULL;

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        should_exit = 1;
    } else if (sig == SIGUSR1) {
        // Trigger stats display
        if (master_pid > 0 && stats != NULL && stats_sem != NULL) {
            trigger_and_display_stats(master_pid, stats, stats_sem);
        }
    }
    // Avoid unused parameter warning by using sig
    (void)sig;
}

static void cleanup_processes(void) {
    printf("Main: Cleaning up processes...\n");
    
    // Terminate slaves first
    for (int i = 0; i < num_slaves_started; i++) {
        if (slave_pids[i] > 0) {
            kill(slave_pids[i], SIGTERM);
        }
    }
    
    // Terminate master
    if (master_pid > 0) {
        kill(master_pid, SIGTERM);
    }
    
    // Wait for all children
    int status;
    while (wait(&status) > 0) {
        // Reap children
    }
    
    // Cleanup stats monitoring
    if (stats_sem != NULL) {
        sem_close(stats_sem);
    }
    if (stats != NULL) {
        munmap(stats, sizeof(stats_t));
    }
    
    printf("Main: All processes terminated\n");
}

static int wait_for_shared_memory_ready(void) {
    // Wait for master to create initialization semaphore
    sem_t *init_sem = NULL;
    
    for (int i = 0; i < 1000; i++) {
        init_sem = sem_open(SEM_INIT_NAME, 0);
        if (init_sem != SEM_FAILED) {
            // Semaphore exists, now wait for master to signal initialization complete
            if (sem_wait(init_sem) == 0) {
                sem_close(init_sem);
                return 0;
            } else {
                perror("sem_wait for initialization");
                sem_close(init_sem);
                return -1;
            }
        }
        // Brief CPU pause without time dependency
        for (volatile int j = 0; j < 10000; j++);
    }
    
    printf("Main: Timeout waiting for master to create initialization semaphore\n");
    return -1;
}

static void show_help(const char *program_name) {
    printf("Usage: %s <num_slaves>\n", program_name);
    printf("  num_slaves: 1-%d\n", NUM_SLAVES);
}

int main(int argc, char *argv[]) {
    int num_slaves;
    
    // Parse command line arguments
    if (argc != 2) {
        show_help(argv[0]);
        return 1;
    }
    
    num_slaves = atoi(argv[1]);
    if (num_slaves <= 0 || num_slaves > NUM_SLAVES) {
        fprintf(stderr, "Error: Number of slaves must be 1-%d\n", NUM_SLAVES);
        show_help(argv[0]);
        return 1;
    }
    
    // Initialize arrays
    for (int i = 0; i < NUM_SLAVES; i++) {
        slave_pids[i] = 0;
    }
    
    // Setup signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);
    
    // Register cleanup function
    atexit(cleanup_processes);
    
    printf("Main: Starting IPC system with %d slaves\n", num_slaves);
    
    // Start master process
    master_pid = fork();
    DEBUG_ASSERT(master_pid >= 0, "Should fork master successfully");
    
    if (master_pid == 0) {
        execl("./master", "master", NULL);
        perror("Failed to exec master");
        exit(1);
    }
    
    printf("Main: Master started (PID=%d)\n", master_pid);
    
    // Wait for master to be ready (shared memory fully initialized)
    if (wait_for_shared_memory_ready() < 0) {
        printf("Main: Master failed to initialize shared memory\n");
        return 1;
    }
    
    // Setup stats monitoring - shared memory is now guaranteed to be ready
    if (setup_stats_monitoring(master_pid, &stats, &stats_sem) < 0) {
        printf("Main: Failed to setup stats monitoring\n");
        return 1;
    }
    
    // Start slave processes
    for (int i = 0; i < num_slaves; i++) {
        slave_pids[i] = fork();
        DEBUG_ASSERT(slave_pids[i] >= 0, "Should fork slave successfully");
        
        if (slave_pids[i] == 0) {
            char slave_id_str[16];
            snprintf(slave_id_str, sizeof(slave_id_str), "%d", i);
            execl("./slave", "slave", slave_id_str, NULL);
            perror("Failed to exec slave");
            exit(1);
        }
        
        printf("Main: Slave %d started (PID=%d)\n", i, slave_pids[i]);
        num_slaves_started++;
    }
    
    printf("Main: All processes started\n");
    printf("Main: Send SIGUSR1 to this process (PID=%d) to display stats\n", getpid());
    printf("Main: Press Ctrl+C to stop all processes\n");
    
    // Main monitoring loop (no time-based logic)
    while (!should_exit) {
        int status;
        pid_t exited_pid = waitpid(-1, &status, WNOHANG);
        
        if (exited_pid > 0) {
            if (exited_pid == master_pid) {
                printf("Main: Master process exited\n");
                master_pid = 0;
                should_exit = 1;
            } else {
                // Find which slave exited
                for (int i = 0; i < num_slaves; i++) {
                    if (slave_pids[i] == exited_pid) {
                        printf("Main: Slave %d exited\n", i);
                        slave_pids[i] = 0;
                        break;
                    }
                }
                
                // Check if all slaves have exited
                int slaves_alive = 0;
                for (int i = 0; i < num_slaves; i++) {
                    if (slave_pids[i] > 0) slaves_alive++;
                }
                
                if (slaves_alive == 0) {
                    printf("Main: All slaves have exited\n");
                    should_exit = 1;
                }
            }
        } else if (exited_pid < 0 && errno != ECHILD) {
            perror("waitpid");
            break;
        } else {
            // No children exited, brief CPU pause
            for (volatile int i = 0; i < 10000; i++);
        }
    }
    
    printf("Main: Shutting down\n");
    return 0;
}