#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "../include/common.h"

// Global state for cleanup
static pid_t master_pid = 0;
static pid_t slave_pids[NUM_SLAVES];
static int num_slaves_started = 0;
static volatile sig_atomic_t should_exit = 0;

static void handle_signal(int sig) {
    should_exit = 1;
}

static void cleanup_processes(void) {
    printf("Main: Cleaning up processes...\n");
    
    // Terminate slaves first
    for (int i = 0; i < num_slaves_started; i++) {
        if (slave_pids[i] > 0) {
            printf("Main: Terminating slave %d (PID=%d)\n", i, slave_pids[i]);
            kill(slave_pids[i], SIGTERM);
        }
    }
    
    // Give slaves time to unregister gracefully
    for (int i = 0; i < 3; i++) {
        int status;
        if (waitpid(-1, &status, WNOHANG) > 0) {
            // A process exited
        } else {
            usleep(100000); // 100ms
        }
    }
    
    // Terminate master
    if (master_pid > 0) {
        printf("Main: Terminating master (PID=%d)\n", master_pid);
        kill(master_pid, SIGTERM);
    }
    
    // Wait for all children
    int status;
    while (wait(&status) > 0) {
        // Reap children
    }
    
    printf("Main: All processes terminated\n");
}

static void wait_for_master_ready(void) {
    // Wait for master to create PID file
    for (int i = 0; i < 50; i++) { // 5 seconds max
        if (file_exists(MASTER_PID_FILE)) {
            printf("Main: Master is ready\n");
            return;
        }
        usleep(100000); // 100ms
    }
    
    printf("Main: Warning - Master PID file not found, continuing anyway\n");
}

int main(int argc, char *argv[]) {
    int num_slaves = NUM_SLAVES;
    
    // Parse command line arguments
    if (argc > 1) {
        num_slaves = atoi(argv[1]);
        if (num_slaves <= 0 || num_slaves > NUM_SLAVES) {
            fprintf(stderr, "Error: Number of slaves must be 1-%d\n", NUM_SLAVES);
            return 1;
        }
    }
    
    // Initialize arrays
    for (int i = 0; i < NUM_SLAVES; i++) {
        slave_pids[i] = 0;
    }
    
    // Setup signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Register cleanup function
    atexit(cleanup_processes);
    
    printf("Main: Starting IPC system with %d slaves\n", num_slaves);
    
    // Start master process
    master_pid = fork();
    DEBUG_ASSERT(master_pid >= 0, "Should fork master successfully");
    
    if (master_pid == 0) {
        // Child process - exec master
        printf("Main: Starting master process\n");
        execl("./master", "master", NULL);
        perror("Failed to exec master");
        exit(1);
    }
    
    printf("Main: Master started (PID=%d)\n", master_pid);
    
    // Wait for master to be ready
    wait_for_master_ready();
    
    // Start slave processes
    for (int i = 0; i < num_slaves; i++) {
        slave_pids[i] = fork();
        DEBUG_ASSERT(slave_pids[i] >= 0, "Should fork slave successfully");
        
        if (slave_pids[i] == 0) {
            // Child process - exec slave
            char slave_id_str[16];
            snprintf(slave_id_str, sizeof(slave_id_str), "%d", i);
            
            printf("Main: Starting slave %d\n", i);
            execl("./slave", "slave", slave_id_str, NULL);
            perror("Failed to exec slave");
            exit(1);
        }
        
        printf("Main: Slave %d started (PID=%d)\n", i, slave_pids[i]);
        num_slaves_started++;
        
        // Small delay between slave starts
        usleep(100000); // 100ms
    }
    
    printf("Main: All processes started\n");
    printf("Main: You can now run './stats_reader' in another terminal\n");
    printf("Main: Press Ctrl+C to stop all processes\n");
    
    // Main monitoring loop
    while (!should_exit) {
        int status;
        pid_t exited_pid = waitpid(-1, &status, WNOHANG);
        
        if (exited_pid > 0) {
            // A child process exited
            if (exited_pid == master_pid) {
                printf("Main: Master process exited\n");
                master_pid = 0;
                should_exit = 1; // If master dies, exit
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
            // No children exited, sleep briefly
            usleep(100000); // 100ms
        }
    }
    
    printf("Main: Shutting down\n");
    return 0;
}