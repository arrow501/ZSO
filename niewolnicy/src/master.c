#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <poll.h>
#include <semaphore.h>
#include <pthread.h>
#include "../include/common.h"

// Global state
static int master_fd = -1;
static int slave_fds[MAX_SLAVES];
static stats_t *stats = NULL;
static sem_t *stats_sem = NULL;
static volatile sig_atomic_t should_exit = 0;
static int signal_pipe[2];  // Self-pipe for signal handling

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        should_exit = 1;
    } else if (sig == SIGUSR1) {
        // Write to pipe - one byte per signal
        char byte = 1;
        write(signal_pipe[1], &byte, 1);
    }
}

static void write_pid_file(void) {
    FILE *f = fopen(MASTER_PID_FILE, "w");
    DEBUG_ASSERT(f != NULL, "Should be able to create PID file");
    fprintf(f, "%d\n", getpid());
    fclose(f);
    
#if ENABLE_PRINTING
    printf("Master PID %d written to %s\n", getpid(), MASTER_PID_FILE);
#endif
}

static int setup_shared_memory(void) {
    shm_unlink(SHM_NAME);
    
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_EXCL, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        return -1;
    }
    
    if (ftruncate(shm_fd, sizeof(stats_t)) != 0) {
        perror("ftruncate");
        close(shm_fd);
        return -1;
    }
    
    stats = mmap(NULL, sizeof(stats_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (stats == MAP_FAILED) {
        perror("mmap");
        return -1;
    }
    
    // Simple mutex initialization
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (pthread_mutex_init(&stats->mutex, &attr) != 0) {
        perror("pthread_mutex_init");
        pthread_mutexattr_destroy(&attr);
        return -1;
    }
    pthread_mutexattr_destroy(&attr);
    
    // Initialize stats
    stats->master_pid = getpid();
    memset(stats->messages_sent, 0, sizeof(stats->messages_sent));
    memset(stats->messages_received, 0, sizeof(stats->messages_received));
    memset(stats->active_slaves, 0, sizeof(stats->active_slaves));
    stats->magic = STATS_MAGIC;
    
    return 0;
}

static int setup_semaphore(void) {
    sem_unlink(SEM_NAME);
    stats_sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 0);
    if (stats_sem == SEM_FAILED) {
        perror("sem_open");
        return -1;
    }
    return 0;
}

static void handle_register(const message_t *msg) {
    int id = msg->slave_id;
    if (id < 0 || id >= MAX_SLAVES) {
        fprintf(stderr, "Invalid slave ID: %d\n", id);
        return;
    }
    
    // Close old connection if exists
    if (slave_fds[id] >= 0) {
        close(slave_fds[id]);
    }
    
    // Open slave FIFO
    char slave_fifo[256];
    snprintf(slave_fifo, sizeof(slave_fifo), "%s%d", SLAVE_FIFO_PREFIX, id);
    if (!file_exists(slave_fifo)) {
        fprintf(stderr, "Slave FIFO does not exist: %s\n", slave_fifo);
        return;
    }
    
    slave_fds[id] = open(slave_fifo, O_WRONLY);
    if (slave_fds[id] < 0) {
        perror("open slave FIFO");
        return;
    }
    
    // Update stats
    pthread_mutex_lock(&stats->mutex);
    stats->active_slaves[id] = 1;
    pthread_mutex_unlock(&stats->mutex);
    
#if ENABLE_PRINTING
    printf("Master: Registered slave %d\n", id);
#endif
}

static void handle_unregister(const message_t *msg) {
    int id = msg->slave_id;
    if (id < 0 || id >= MAX_SLAVES) {
        fprintf(stderr, "Invalid slave ID: %d\n", id);
        return;
    }
    
    if (slave_fds[id] >= 0) {
        close(slave_fds[id]);
        slave_fds[id] = -1;
    }
    
    pthread_mutex_lock(&stats->mutex);
    stats->active_slaves[id] = 0;
    pthread_mutex_unlock(&stats->mutex);
    
#if ENABLE_PRINTING
    printf("Master: Unregistered slave %d\n", id);
#endif
}

static void handle_response(const message_t *msg) {
    int id = msg->slave_id;
    if (id < 0 || id >= MAX_SLAVES) {
        fprintf(stderr, "Invalid slave ID: %d\n", id);
        return;
    }
    
    pthread_mutex_lock(&stats->mutex);
    stats->messages_received[id]++;
    pthread_mutex_unlock(&stats->mutex);
}

static void process_message(const message_t *msg) {
    switch (msg->type) {
        case MSG_REGISTER:
            handle_register(msg);
            break;
        case MSG_UNREGISTER:
            handle_unregister(msg);
            break;
        case MSG_RESPONSE:
            handle_response(msg);
            break;
        default:
            fprintf(stderr, "Unknown message type: %d\n", msg->type);
            break;
    }
}

static void send_queries(void) {
    static int query_counter = 0;
    query_counter++;
    
    for (int i = 0; i < MAX_SLAVES; i++) {
        if (slave_fds[i] < 0) continue;
        
        message_t msg = {
            .type = MSG_QUERY,
            .slave_id = i,
            .payload = query_counter
        };
        
        if (write(slave_fds[i], &msg, sizeof(msg)) == sizeof(msg)) {
            pthread_mutex_lock(&stats->mutex);
            stats->messages_sent[i]++;
            pthread_mutex_unlock(&stats->mutex);
        } else {
            // Slave disconnected
            close(slave_fds[i]);
            slave_fds[i] = -1;
            pthread_mutex_lock(&stats->mutex);
            stats->active_slaves[i] = 0;
            pthread_mutex_unlock(&stats->mutex);
        }
    }
    
#if ENABLE_PRINTING
    printf("Master: Sent query %d to active slaves\n", query_counter);
#endif
}

static void signal_stats_ready(void) {
    if (sem_post(stats_sem) != 0) {
        perror("sem_post");
        return;
    }
    
    // Also display stats directly in master (simplified)
    printf("\n=== Master Statistics ===\n");
    printf("Master PID: %d\n", getpid());
    
    int total_sent = 0, total_received = 0, active_count = 0;
    
    printf("\nSlave Status:\n");
    for (int i = 0; i < MAX_SLAVES; i++) {
        if (stats->active_slaves[i]) {
            printf("  Slave %d: ACTIVE, sent=%d, received=%d\n", 
                   i, stats->messages_sent[i], stats->messages_received[i]);
            active_count++;
        }
        // Only count totals, don't print inactive ones
        total_sent += stats->messages_sent[i];
        total_received += stats->messages_received[i];
    }
    
    if (active_count == 0) {
        printf("  No active slaves\n");
    }
    
    printf("\nTotals: %d active slaves, %d sent, %d received\n", 
           active_count, total_sent, total_received);
    printf("========================\n");
}

static void cleanup(void) {
    // Close slave connections
    for (int i = 0; i < MAX_SLAVES; i++) {
        if (slave_fds[i] >= 0) {
            close(slave_fds[i]);
        }
    }
    
    // Close master FIFO
    if (master_fd >= 0) {
        close(master_fd);
    }
    unlink(MASTER_FIFO);
    unlink(MASTER_PID_FILE);
    
    // Close signal pipe
    if (signal_pipe[0] >= 0) close(signal_pipe[0]);
    if (signal_pipe[1] >= 0) close(signal_pipe[1]);
    
    // Cleanup shared memory
    if (stats != NULL) {
        pthread_mutex_destroy(&stats->mutex);
        munmap(stats, sizeof(stats_t));
    }
    shm_unlink(SHM_NAME);
    
    // Cleanup semaphore
    if (stats_sem != NULL) {
        sem_close(stats_sem);
    }
    sem_unlink(SEM_NAME);
}

int main(void) {
    // Initialize slave FDs
    for (int i = 0; i < MAX_SLAVES; i++) {
        slave_fds[i] = -1;
    }
    
    // Initialize signal pipe
    signal_pipe[0] = -1;
    signal_pipe[1] = -1;
    
    // Create self-pipe for signal handling
    if (pipe(signal_pipe) != 0) {
        perror("pipe");
        exit(1);
    }
    fcntl(signal_pipe[1], F_SETFL, O_NONBLOCK);
    
    // Setup signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);
    signal(SIGPIPE, SIG_IGN);
    
    atexit(cleanup);
    
    // Write PID file first
    write_pid_file();
    
    // Setup IPC
    if (setup_shared_memory() != 0) {
        fprintf(stderr, "Failed to setup shared memory\n");
        exit(1);
    }
    if (setup_semaphore() != 0) {
        fprintf(stderr, "Failed to setup semaphore\n");  
        exit(1);
    }
    
    // Create master FIFO
    unlink(MASTER_FIFO);
    if (file_exists(MASTER_FIFO)) {
        fprintf(stderr, "Failed to remove old FIFO\n");
        exit(1);
    }
    if (mkfifo(MASTER_FIFO, 0666) != 0) {
        perror("mkfifo");
        exit(1);
    }
    
    master_fd = open(MASTER_FIFO, O_RDONLY | O_NONBLOCK);
    if (master_fd < 0) {
        perror("open master FIFO");
        exit(1);
    }
    
#if ENABLE_PRINTING
    printf("Master: Started (PID=%d)\n", getpid());
    printf("Master: Send SIGUSR1 to display stats\n");
#endif
    
    // Main loop
    struct pollfd pfds[2] = {
        { .fd = master_fd, .events = POLLIN },
        { .fd = signal_pipe[0], .events = POLLIN }
    };
    
    while (!should_exit) {
        // Poll for both messages and signals
        int ret = poll(pfds, 2, POLL_TIMEOUT_MS);
        
        // Handle IPC messages
        if (ret > 0 && (pfds[0].revents & POLLIN)) {
            message_t msg;
            while (read(master_fd, &msg, sizeof(msg)) == sizeof(msg)) {
                process_message(&msg);
            }
        }
        
        // Handle signal pipe - process ALL pending signals
        if (ret > 0 && (pfds[1].revents & POLLIN)) {
            char buffer[256];
            ssize_t bytes_read = read(signal_pipe[0], buffer, sizeof(buffer));
            if (bytes_read > 0) {
                // Each byte = one signal, display stats for each
                for (ssize_t i = 0; i < bytes_read; i++) {
                    signal_stats_ready();
                }
            }
        }
        
        // Send periodic queries (poll-count based, no time logic)
        static int poll_count = 0;
        if (++poll_count >= 10) { // Every 10 poll cycles
            send_queries();
            poll_count = 0;
        }
    }
    
#if ENABLE_PRINTING
    printf("Master: Shutting down\n");
#endif
    
    return 0;
}