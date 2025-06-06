#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include "../include/common.h"

// Global state
static pid_t master_pid = 0;
static pid_t slave_pids[MAX_SLAVES];
static int num_slaves_started = 0;
static volatile sig_atomic_t should_exit = 0;

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        should_exit = 1;
        
        // Forward termination signals to all child processes
        for (int i = 0; i < num_slaves_started; i++) {
            if (slave_pids[i] > 0) {
                kill(slave_pids[i], sig);
            }
        }
        
        if (master_pid > 0) {
            kill(master_pid, sig);
        }
    }
}

static void cleanup_processes(void) {
#if ENABLE_PRINTING
    printf("Main: Cleaning up processes...\n");
#endif
    
    // Send SIGTERM to any remaining processes
    for (int i = 0; i < num_slaves_started; i++) {
        if (slave_pids[i] > 0) {
            kill(slave_pids[i], SIGTERM);
        }
    }
    
    if (master_pid > 0) {
        kill(master_pid, SIGTERM);
    }
    
    // Wait for children to exit gracefully
    int status;
    while (wait(&status) > 0) {
        // Reap children
    }
    
#if ENABLE_PRINTING
    printf("Main: All processes terminated\n");
#endif
}

static void show_help(const char *program_name) {
    printf("Usage: %s <num_slaves>\n", program_name);
    printf("  num_slaves: 1-%d\n", MAX_SLAVES);
}

static void request_stats_from_master(void) {
    if (master_pid > 0) {
        printf("Requesting stats from master...\n");
        kill(master_pid, SIGUSR1);
    }
}

// Non-blocking input check using poll (no time dependency)
static int check_for_input(void) {
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    
    // Poll with 0 timeout = immediate return (non-blocking)
    int ret = poll(&pfd, 1, 0);
    
    if (ret > 0 && (pfd.revents & POLLIN)) {
        char input[10];
        if (fgets(input, sizeof(input), stdin) != NULL) {
            if (input[0] == 'q' || input[0] == 'Q') {
                return 2; // Quit requested
            } else {
                return 1; // Stats requested
            }
        }
    }
    
    return 0; // No input
}

int main(int argc, char *argv[]) {
    int num_slaves;
    
    if (argc != 2) {
        show_help(argv[0]);
        return 1;
    }
    
    num_slaves = atoi(argv[1]);
    if (num_slaves <= 0 || num_slaves > MAX_SLAVES) {
        fprintf(stderr, "Error: Number of slaves must be 1-%d\n", MAX_SLAVES);
        show_help(argv[0]);
        return 1;
    }
    
    for (int i = 0; i < MAX_SLAVES; i++) {
        slave_pids[i] = 0;
    }
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    // Removed SIGUSR1 - tests signal master directly
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
    
    // Simple wait for master to be ready
    for (int i = 0; i < 100; i++) {
        if (file_exists(MASTER_PID_FILE)) break;
        for (volatile int j = 0; j < 50000; j++);
    }
    
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
        num_slaves_started++;
    }
    
#if ENABLE_PRINTING
    printf("Main: All processes started\n");
    printf("Main: Press Ctrl+C to stop all processes\n");
#endif
    
    // Interactive mode - also show prompt for manual testing
    printf("Master-Slave IPC System running with %d slaves\n", num_slaves);
    printf("Press ENTER to display statistics, 'q' + ENTER to quit\n");
    
    // Main loop - handle both signals and interactive input
    while (!should_exit) {
        // Check for child process exits
        int status;
        pid_t exited_pid = waitpid(-1, &status, WNOHANG);
        
        if (exited_pid > 0) {
            if (exited_pid == master_pid) {
#if ENABLE_PRINTING
                printf("Main: Master process exited\n");
#endif
                master_pid = 0;
                should_exit = 1;
                break;
            } else {
                for (int i = 0; i < num_slaves; i++) {
                    if (slave_pids[i] == exited_pid) {
#if ENABLE_PRINTING
                        printf("Main: Slave %d exited\n", i);
#endif
                        slave_pids[i] = 0;
                        break;
                    }
                }
                
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
        
        // Check for user input (non-blocking, no time dependency)
        int input_result = check_for_input();
        if (input_result == 2) {
            // Quit requested
            should_exit = 1;
            break;
        } else if (input_result == 1) {
            // Stats requested
            request_stats_from_master();
        }
        
        // Brief CPU pause to avoid busy waiting
        for (volatile int i = 0; i < 10000; i++);
    }
    
#if ENABLE_PRINTING
    printf("Main: Shutting down\n");
#endif
    return 0;
}