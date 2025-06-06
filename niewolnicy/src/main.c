#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include "../include/common.h"

// Global state
static pid_t master_pid = 0;
static pid_t slave_pids[MAX_SLAVES];
static int num_slaves_started = 0;
static volatile sig_atomic_t should_exit = 0;
static volatile sig_atomic_t stats_requested = 0;

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        should_exit = 1;
    } else if (sig == SIGUSR1) {
        stats_requested = 1;
    }
}

static void cleanup_processes(void) {
#if ENABLE_PRINTING
    printf("Main: Cleaning up processes...\n");
#endif
    
    for (int i = 0; i < num_slaves_started; i++) {
        if (slave_pids[i] > 0) {
            kill(slave_pids[i], SIGTERM);
        }
    }
    
    if (master_pid > 0) {
        kill(master_pid, SIGTERM);
    }
    
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
    signal(SIGUSR1, handle_signal);
    atexit(cleanup_processes);
    
#if ENABLE_PRINTING
    printf("Main: Starting IPC system with %d slaves\n", num_slaves);
#endif
    
    // Start master
    master_pid = fork();
    DEBUG_ASSERT(master_pid >= 0, "Should fork master successfully");
    
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
        DEBUG_ASSERT(slave_pids[i] >= 0, "Should fork slave successfully");
        
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
    printf("Main: Send SIGUSR1 to this process (PID=%d) to display stats\n", getpid());
    printf("Main: Press Ctrl+C to stop all processes\n");
#else
    printf("IPC system running with %d slaves (PID=%d)\n", num_slaves, getpid());
    printf("Press Enter to display stats, Ctrl+C to exit\n");
#endif
    
    // Main monitoring loop
    while (!should_exit) {
        // Handle stats request
        if (stats_requested) {
            stats_requested = 0;
            if (master_pid > 0) {
                kill(master_pid, SIGUSR1);
            }
        }
        
#if !ENABLE_PRINTING
        // In release mode, wait for user input (Enter key)
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms timeout
        
        int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), stdin)) {
                // User pressed Enter - trigger stats
                if (master_pid > 0) {
                    kill(master_pid, SIGUSR1);
                }
            }
        }
#endif
        
        // Check for exited children
        int status;
        pid_t exited_pid = waitpid(-1, &status, WNOHANG);
        
        if (exited_pid > 0) {
            if (exited_pid == master_pid) {
#if ENABLE_PRINTING
                printf("Main: Master process exited\n");
#endif
                master_pid = 0;
                should_exit = 1;
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
                }
            }
        } else if (exited_pid < 0 && errno != ECHILD) {
            perror("waitpid");
            break;
        } else {
            // Brief pause
            for (volatile int i = 0; i < 10000; i++);
        }
    }
    
#if ENABLE_PRINTING
    printf("Main: Shutting down\n");
#endif
    return 0;
}