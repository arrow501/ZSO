#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "../include/common.h"

// Global state - keep it minimal
static pid_t master_pid = 0;
static pid_t slave_pids[MAX_SLAVES];
static int num_slaves = 0;
static volatile sig_atomic_t should_exit = 0;

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        should_exit = 1;
        
        // Forward signal to all children
        if (master_pid > 0) {
            kill(master_pid, sig);
        }
        for (int i = 0; i < num_slaves; i++) {
            if (slave_pids[i] > 0) {
                kill(slave_pids[i], sig);
            }
        }
    }
}

static void cleanup_processes(void) {
    // Send SIGTERM to any remaining processes
    if (master_pid > 0) {
        kill(master_pid, SIGTERM);
    }
    for (int i = 0; i < num_slaves; i++) {
        if (slave_pids[i] > 0) {
            kill(slave_pids[i], SIGTERM);
        }
    }
    
    // Wait for children to exit
    int status;
    while (wait(&status) > 0) {
        // Reap children
    }
}

static void wait_for_master_ready(void) {
    // Wait for master FIFO to exist instead of PID file
    for (int i = 0; i < 100; i++) {
        if (file_exists(MASTER_FIFO)) {
            return;
        }
        // Small delay without using time functions
        for (volatile int j = 0; j < 100000; j++);
    }
    fprintf(stderr, "Timeout waiting for master to be ready\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    // Parse arguments
    if (argc != 2) {
        printf("Usage: %s <num_slaves>\n", argv[0]);
        printf("  num_slaves: 1-%d\n", MAX_SLAVES);
        return 1;
    }
    
    num_slaves = atoi(argv[1]);
    if (num_slaves <= 0 || num_slaves > MAX_SLAVES) {
        fprintf(stderr, "Error: Number of slaves must be 1-%d\n", MAX_SLAVES);
        return 1;
    }
    
    // Initialize
    for (int i = 0; i < MAX_SLAVES; i++) {
        slave_pids[i] = 0;
    }
    
    // Setup signal handling
    signal(SIGINT, handle_signal);    // Ctrl+C for stats (unusual but works)
    signal(SIGTERM, handle_signal);   // Terminate 
    signal(SIGQUIT, handle_signal);   // Ctrl+\ to terminate
    signal(SIGUSR2, handle_signal);   // External signal forwarding for tests
    atexit(cleanup_processes);
    
#if ENABLE_PRINTING
    printf("Main: Starting IPC system with %d slaves\n", num_slaves);
#endif
    
    // Start master
    master_pid = fork();
    if (master_pid < 0) {
        perror("fork master");
        return 1;
    }
    
    if (master_pid == 0) {
        execl("./master", "master", NULL);
        perror("Failed to exec master");
        exit(1);
    }
    
#if ENABLE_PRINTING
    printf("Main: Master started (PID=%d)\n", master_pid);
#endif
    
    // Wait for master to be ready
    wait_for_master_ready();
    
    // Start slaves
    for (int i = 0; i < num_slaves; i++) {
        slave_pids[i] = fork();
        if (slave_pids[i] < 0) {
            perror("fork slave");
            return 1;
        }
        
        if (slave_pids[i] == 0) {
            char slave_id_str[16];
            snprintf(slave_id_str, sizeof(slave_id_str), "%d", i);
            execl("./slave", "slave", slave_id_str, NULL);
            perror("Failed to exec slave");
            exit(1);
        }
        
#if ENABLE_PRINTING
        printf("Main: Slave %d started (PID=%d)\n", i, slave_pids[i]);
#endif
    }
    
    printf("Master-Slave IPC System running with %d slaves\n", num_slaves);
    printf("Master PID: %d (managed by main)\n", master_pid);
    printf("Press Ctrl+C to display statistics, Ctrl+\\ to stop\n");
    
    // Main loop - just wait for processes to exit or signals
    while (!should_exit) {
        // Check for child process exits
        int status;
        pid_t exited_pid = waitpid(-1, &status, WNOHANG);
        
        if (exited_pid > 0) {
            if (exited_pid == master_pid) {
#if ENABLE_PRINTING
                printf("Main: Master process exited\n");
#endif
                should_exit = 1;
                break;
            } else {
                // Mark slave as exited
                for (int i = 0; i < num_slaves; i++) {
                    if (slave_pids[i] == exited_pid) {
#if ENABLE_PRINTING
                        printf("Main: Slave %d exited\n", i);
#endif
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
#if ENABLE_PRINTING
                    printf("Main: All slaves have exited\n");
#endif
                    should_exit = 1;
                    break;
                }
            }
        }
        
        // Small delay to prevent busy waiting
        for (volatile int i = 0; i < 10000; i++);
    }
    
#if ENABLE_PRINTING
    printf("Main: Shutting down\n");
#endif
    
    return 0;
}