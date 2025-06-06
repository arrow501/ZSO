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
#include <fcntl.h>
#include <poll.h>
#include "../include/common.h"


// Global state
static int master_fd = -1;
static int slave_fds[NUM_SLAVES];
static stats_t *stats = NULL;
static sem_t *stats_sem = NULL;
static sem_t *stats_init_sem = NULL;
static volatile sig_atomic_t should_exit = 0;
static volatile sig_atomic_t stats_signals_pending = 0;

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        should_exit = 1;
    } else if (sig == SIGUSR1) {
        stats_signals_pending++;  // Count each signal atomically
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
    // Clean up any existing shared memory
    shm_unlink(SHM_NAME);
    
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_EXCL, 0666);
    DEBUG_ASSERT(shm_fd >= 0, "Should create shared memory");
    
    DEBUG_ASSERT(ftruncate(shm_fd, sizeof(stats_t)) == 0, "Should set shm size");
    
    stats = mmap(NULL, sizeof(stats_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    DEBUG_ASSERT(stats != MAP_FAILED, "Should map shared memory");
    
    // Initialize process-shared mutex
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    DEBUG_ASSERT(pthread_mutex_init(&stats->mutex, &attr) == 0, "Should init mutex");
    pthread_mutexattr_destroy(&attr);
    
    // Initialize stats with mutex protection
    pthread_mutex_lock(&stats->mutex);
    stats->master_pid = getpid();
    memset(stats->messages_sent, 0, sizeof(stats->messages_sent));
    memset(stats->messages_received, 0, sizeof(stats->messages_received));
    memset(stats->active_slaves, 0, sizeof(stats->active_slaves));
    stats->magic = STATS_MAGIC;
    pthread_mutex_unlock(&stats->mutex);
    
    return 0;
}

static int setup_semaphore(void) {
    // Clean up old semaphores
    sem_unlink(SEM_NAME);
    sem_unlink(SEM_INIT_NAME);
    
    // Create stats ready semaphore
    stats_sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 0);
    DEBUG_ASSERT(stats_sem != SEM_FAILED, "Should create stats semaphore");
    
    // Create initialization semaphore (starts at 0 - blocks until we signal)
    stats_init_sem = sem_open(SEM_INIT_NAME, O_CREAT | O_EXCL, 0666, 0);
    DEBUG_ASSERT(stats_init_sem != SEM_FAILED, "Should create init semaphore");
    
    return 0;
}

static void handle_register(const message_t *msg) {
    int id = msg->slave_id;
    DEBUG_ASSERT(id >= 0 && id < NUM_SLAVES, "Valid slave ID");
    
    // Close old connection if exists
    if (slave_fds[id] >= 0) {
        close(slave_fds[id]);
    }
    
    // Open slave FIFO
    char slave_fifo[256];
    snprintf(slave_fifo, sizeof(slave_fifo), "%s%d", SLAVE_FIFO_PREFIX, id);
    DEBUG_ASSERT(file_exists(slave_fifo), "Slave FIFO should exist");
    
    slave_fds[id] = open(slave_fifo, O_WRONLY);
    DEBUG_ASSERT(slave_fds[id] >= 0, "Should open slave FIFO");
    
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
    DEBUG_ASSERT(id >= 0 && id < NUM_SLAVES, "Valid slave ID");
    
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
    DEBUG_ASSERT(id >= 0 && id < NUM_SLAVES, "Valid slave ID");
    
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
            DEBUG_ASSERT(0, "Unknown message type");
    }
}

static void send_queries(void) {
    static int query_counter = 0;
    query_counter++;
    
    for (int i = 0; i < NUM_SLAVES; i++) {
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
    DEBUG_ASSERT(sem_post(stats_sem) == 0, "Should signal stats ready");
    
#if ENABLE_PRINTING
    printf("Master: Stats updated in shared memory\n");
#endif
}

static void cleanup(void) {
    // Close slave connections
    for (int i = 0; i < NUM_SLAVES; i++) {
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
    
    // Cleanup semaphore
    if (stats_sem != NULL) {
        sem_close(stats_sem);
    }
    sem_unlink(SEM_NAME);
    
    if (stats_init_sem != NULL) {
        sem_close(stats_init_sem);
    }
    sem_unlink(SEM_INIT_NAME);
}

int main(void) {
    // Initialize slave FDs
    for (int i = 0; i < NUM_SLAVES; i++) {
        slave_fds[i] = -1;
    }
    
    // Setup signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);
    signal(SIGPIPE, SIG_IGN);
    
    atexit(cleanup);
    
    // Write PID file first
    write_pid_file();
    
    // Setup IPC
    DEBUG_ASSERT(setup_shared_memory() == 0, "Should setup shared memory");
    DEBUG_ASSERT(setup_semaphore() == 0, "Should setup semaphore");
    
    // Signal that shared memory is fully initialized and ready
    DEBUG_ASSERT(sem_post(stats_init_sem) == 0, "Should signal initialization complete");
    
    // Create master FIFO
    unlink(MASTER_FIFO);
    DEBUG_ASSERT(!file_exists(MASTER_FIFO), "No leftover FIFO");
    DEBUG_ASSERT(mkfifo(MASTER_FIFO, 0666) == 0, "Should create master FIFO");
    
    master_fd = open(MASTER_FIFO, O_RDONLY | O_NONBLOCK);
    DEBUG_ASSERT(master_fd >= 0, "Should open master FIFO");
    
#if ENABLE_PRINTING
    printf("Master: Started (PID=%d)\n", getpid());
    printf("Master: Send SIGUSR1 to display stats\n");
#endif
    
    // Main loop
    struct pollfd pfd = { .fd = master_fd, .events = POLLIN };
    
    while (!should_exit) {
        // Handle all pending stats requests
        while (stats_signals_pending > 0) {
            stats_signals_pending--;  // Process one signal
            signal_stats_ready();
        }
        
        // Poll for messages
        int ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            message_t msg;
            while (read(master_fd, &msg, sizeof(msg)) == sizeof(msg)) {
                process_message(&msg);
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