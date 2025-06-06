#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include "../include/common.h"

// Global state - keep it minimal
static int master_fd = -1;
static int slave_fds[MAX_SLAVES];
static stats_t *stats = NULL;
static volatile sig_atomic_t should_exit = 0;
static volatile sig_atomic_t stats_requests = 0;  // Simple atomic counter

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        should_exit = 1;
    } else if (sig == SIGUSR1) {
        // Simply increment counter - atomic on most platforms
        stats_requests++;
    }
}

static void write_pid_file(void) {
    FILE *f = fopen(MASTER_PID_FILE, "w");
    if (!f) {
        perror("fopen PID file");
        exit(1);
    }
    fprintf(f, "%d\n", getpid());
    fclose(f);
}

static void setup_shared_memory(void) {
    // Clean up any old shared memory
    shm_unlink(SHM_NAME);
    
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_EXCL, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        exit(1);
    }
    
    if (ftruncate(shm_fd, sizeof(stats_t)) != 0) {
        perror("ftruncate");
        exit(1);
    }
    
    stats = mmap(NULL, sizeof(stats_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (stats == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    
    // Initialize mutex for process sharing
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&stats->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    
    // Initialize stats
    stats->master_pid = getpid();
    memset(stats->messages_sent, 0, sizeof(stats->messages_sent));
    memset(stats->messages_received, 0, sizeof(stats->messages_received));
    memset(stats->active_slaves, 0, sizeof(stats->active_slaves));
    stats->magic = STATS_MAGIC;
}

static void display_stats(void) {
    pthread_mutex_lock(&stats->mutex);
    
    printf("\n=== Master Statistics ===\n");
    printf("Master PID: %d\n", getpid());
    printf("\nSlave Status:\n");
    
    int total_sent = 0, total_received = 0, active_count = 0;
    
    for (int i = 0; i < MAX_SLAVES; i++) {
        if (stats->active_slaves[i]) {
            printf("  Slave %d: ACTIVE, sent=%d, received=%d\n", 
                   i, stats->messages_sent[i], stats->messages_received[i]);
            active_count++;
        }
        total_sent += stats->messages_sent[i];
        total_received += stats->messages_received[i];
    }
    
    if (active_count == 0) {
        printf("  No active slaves\n");
    }
    
    printf("\nTotals: %d active slaves, %d sent, %d received\n", 
           active_count, total_sent, total_received);
    printf("========================\n");
    
    pthread_mutex_unlock(&stats->mutex);
}

static void handle_register(const message_t *msg) {
    int id = msg->slave_id;
    if (id < 0 || id >= MAX_SLAVES) return;
    
    // Close old connection if exists
    if (slave_fds[id] >= 0) {
        close(slave_fds[id]);
    }
    
    // Open slave FIFO - wait for it to exist
    char slave_fifo[256];
    snprintf(slave_fifo, sizeof(slave_fifo), "%s%d", SLAVE_FIFO_PREFIX, id);
    
    // Wait for slave FIFO to be created
    for (int i = 0; i < 100; i++) {
        if (file_exists(slave_fifo)) break;
        for (volatile int j = 0; j < 10000; j++);
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
    if (id < 0 || id >= MAX_SLAVES) return;
    
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
    if (id < 0 || id >= MAX_SLAVES) return;
    
    pthread_mutex_lock(&stats->mutex);
    stats->messages_received[id]++;
    pthread_mutex_unlock(&stats->mutex);
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
        
        ssize_t written = write(slave_fds[i], &msg, sizeof(msg));
        if (written == sizeof(msg)) {
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
}

static void cleanup(void) {
    // Close all slave connections
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
    
    // Cleanup shared memory
    if (stats != NULL) {
        pthread_mutex_destroy(&stats->mutex);
        munmap(stats, sizeof(stats_t));
    }
    shm_unlink(SHM_NAME);
}

int main(void) {
    // Initialize
    for (int i = 0; i < MAX_SLAVES; i++) {
        slave_fds[i] = -1;
    }
    
    // Setup signals
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);
    signal(SIGPIPE, SIG_IGN);
    atexit(cleanup);
    
    // Write PID file FIRST so main can find it
    write_pid_file();
    
    // Setup IPC
    setup_shared_memory();
    
    // Create master FIFO
    unlink(MASTER_FIFO);
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
#endif    
    // Main loop - much simpler!
    while (!should_exit) {
        // Check for pending stats requests
        sig_atomic_t pending_requests = stats_requests;
        if (pending_requests > 0) {
            // Display stats for each pending request
            for (sig_atomic_t i = 0; i < pending_requests; i++) {
                display_stats();
            }
            // Atomically subtract the requests we just processed
            stats_requests -= pending_requests;
        }
          // Check for messages from slaves
        message_t msg;
        while (read(master_fd, &msg, sizeof(msg)) == sizeof(msg)) {
            switch (msg.type) {
                case MSG_REGISTER:
                    handle_register(&msg);
                    break;
                case MSG_UNREGISTER:
                    handle_unregister(&msg);
                    break;
                case MSG_RESPONSE:
                    handle_response(&msg);
                    break;
                default:
                    break;
            }
        }
        
        // Send queries every iteration for active IPC communication
        send_queries();
        
        // Small delay to prevent busy waiting
        for (volatile int i = 0; i < 1000; i++);
    }
    
    return 0;
}