#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>
#include <poll.h>
#include <semaphore.h>
#include <pthread.h>
#include "../include/common.h"

// Slave information
typedef struct {
    int pid;
    int fd;
    int active;
} slave_info_t;

// Global state
static slave_info_t slaves[MAX_SLAVES];
static int master_fd = -1;
static stats_t *stats = NULL;
static sem_t *stats_ready_sem = NULL;
static volatile sig_atomic_t should_exit = 0;
static volatile sig_atomic_t dump_stats = 0;

// === Signal Handling ===
static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        should_exit = 1;
    } else if (sig == SIGUSR1) {
        dump_stats = 1;
    }
}

// === Shared Memory Management ===
static int setup_shared_memory(void) {
    // Clean up any existing shared memory
    shm_unlink(SHM_NAME);
    
    // Verify it's gone
    int shm_test = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_test >= 0) {
        close(shm_test);
        DEBUG_ASSERT(0, "Shared memory should not exist after unlink");
    }
    
    // Create shared memory object
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_EXCL, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        return -1;
    }
    
    DEBUG_ASSERT(shm_fd >= 0, "Shared memory fd should be valid");
    
    // Set size of shared memory
    if (ftruncate(shm_fd, sizeof(stats_t)) < 0) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }
    
    // Map shared memory
    stats = mmap(NULL, sizeof(stats_t), PROT_READ | PROT_WRITE,
                 MAP_SHARED, shm_fd, 0);
    close(shm_fd);  // Can close fd after mmap
    
    if (stats == MAP_FAILED) {
        perror("mmap");
        shm_unlink(SHM_NAME);
        return -1;
    }
    
    DEBUG_ASSERT(stats != NULL && stats != MAP_FAILED, "Stats mapping should be valid");
    
    // Initialize mutex for process-shared use
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    // PTHREAD_PROCESS_SHARED allows mutex to work across processes
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    
    int ret = pthread_mutex_init(&stats->mutex, &mutex_attr);
    DEBUG_ASSERT(ret == 0, "Mutex initialization should succeed");
    
    pthread_mutexattr_destroy(&mutex_attr);
    
    // Initialize statistics
    memset(stats->messages_sent, 0, sizeof(stats->messages_sent));
    memset(stats->messages_received, 0, sizeof(stats->messages_received));
    memset(stats->slave_pids, 0, sizeof(stats->slave_pids));
    memset(stats->active_slaves, 0, sizeof(stats->active_slaves));
    
    // Set magic number to verify initialization
    stats->magic = STATS_MAGIC;
    
    DEBUG_ASSERT(stats->magic == STATS_MAGIC, "Magic number should be set");
    
    return 0;
}

static int setup_semaphore(void) {
    // Clean up any existing semaphore
    sem_unlink(SEM_NAME);
    
    // Verify it's gone by trying to open it
    sem_t *test_sem = sem_open(SEM_NAME, 0);
    if (test_sem != SEM_FAILED) {
        sem_close(test_sem);
        DEBUG_ASSERT(0, "Semaphore should not exist after unlink");
    }
    
    // Create named semaphore for signaling stats updates
    // Initial value 0 - readers will block until we post
    stats_ready_sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 0);
    if (stats_ready_sem == SEM_FAILED) {
        perror("sem_open");
        return -1;
    }
    
    DEBUG_ASSERT(stats_ready_sem != SEM_FAILED, "Semaphore should be created successfully");
    
    return 0;
}

// === Message Processing ===
static void handle_register(const message_t *msg) {
    DEBUG_ASSERT(msg != NULL, "Message should not be NULL");
    DEBUG_ASSERT(msg->type == MSG_REGISTER, "Message type should be REGISTER");
    
    int id = msg->slave_id;
    if (id < 0 || id >= MAX_SLAVES) {
        DEBUG_ASSERT(0, "Invalid slave_id in register message");
        return;
    }
    
    DEBUG_ASSERT(msg->payload > 0, "PID in register message should be valid");
    
    // Close old connection if exists
    if (slaves[id].fd >= 0) {
        close(slaves[id].fd);
        slaves[id].fd = -1;
    }
    
    // Open slave's FIFO
    char slave_fifo[256];
    snprintf(slave_fifo, sizeof(slave_fifo), "%s%d", SLAVE_FIFO_PREFIX, id);
    
    // Verify slave FIFO exists
    DEBUG_ASSERT(file_exists(slave_fifo), "Slave FIFO should exist before opening");
    
    slaves[id].fd = open(slave_fifo, O_WRONLY);
    if (slaves[id].fd < 0) {
        perror("open slave FIFO");
        return;
    }
    
    DEBUG_ASSERT(slaves[id].fd >= 0, "Slave fd should be valid after open");
    
    slaves[id].pid = msg->payload;
    slaves[id].active = 1;
    
    // Update shared memory stats
    DEBUG_ASSERT(stats != NULL, "Stats should be initialized");
    DEBUG_ASSERT(stats->magic == STATS_MAGIC, "Stats magic should be valid");
    
    pthread_mutex_lock(&stats->mutex);  // Lock before modifying shared data
    stats->slave_pids[id] = msg->payload;
    stats->active_slaves[id] = 1;
    pthread_mutex_unlock(&stats->mutex);  // Always unlock
    
    printf("Master: Registered slave %d (PID=%d)\n", id, msg->payload);
}

static void handle_unregister(const message_t *msg) {
    DEBUG_ASSERT(msg != NULL, "Message should not be NULL");
    DEBUG_ASSERT(msg->type == MSG_UNREGISTER, "Message type should be UNREGISTER");
    
    int id = msg->slave_id;
    if (id < 0 || id >= MAX_SLAVES) {
        DEBUG_ASSERT(0, "Invalid slave_id in unregister message");
        return;
    }
    
    if (slaves[id].fd >= 0) {
        close(slaves[id].fd);
        slaves[id].fd = -1;
    }
    
    slaves[id].active = 0;
    slaves[id].pid = 0;
    
    // Update shared memory stats
    DEBUG_ASSERT(stats != NULL, "Stats should be initialized");
    DEBUG_ASSERT(stats->magic == STATS_MAGIC, "Stats magic should be valid");
    
    pthread_mutex_lock(&stats->mutex);
    stats->slave_pids[id] = 0;
    stats->active_slaves[id] = 0;
    pthread_mutex_unlock(&stats->mutex);
    
    printf("Master: Unregistered slave %d\n", id);
}

static void handle_response(const message_t *msg) {
    DEBUG_ASSERT(msg != NULL, "Message should not be NULL");
    DEBUG_ASSERT(msg->type == MSG_RESPONSE, "Message type should be RESPONSE");
    
    int id = msg->slave_id;
    if (id < 0 || id >= MAX_SLAVES) {
        DEBUG_ASSERT(0, "Invalid slave_id in response message");
        return;
    }
    
    // Verify response is from active slave
    DEBUG_ASSERT(slaves[id].active, "Response should come from active slave");
    
    // Update statistics
    DEBUG_ASSERT(stats != NULL, "Stats should be initialized");
    DEBUG_ASSERT(stats->magic == STATS_MAGIC, "Stats magic should be valid");
    
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
    }
}

// === Query Management ===
static void send_queries_to_slaves(int query_value) {
    DEBUG_ASSERT(query_value > 0, "Query value should be positive");
    DEBUG_ASSERT(stats != NULL, "Stats should be initialized");
    
    int sent_count = 0;
    
    for (int i = 0; i < MAX_SLAVES; i++) {
        if (!slaves[i].active || slaves[i].fd < 0) continue;
        
        DEBUG_ASSERT(slaves[i].pid > 0, "Active slave should have valid PID");
        
        message_t msg = {
            .type = MSG_QUERY,
            .slave_id = i,
            .payload = query_value
        };
        
        ssize_t written = write(slaves[i].fd, &msg, sizeof(msg));
        if (written == sizeof(msg)) {
            // Update statistics
            pthread_mutex_lock(&stats->mutex);
            stats->messages_sent[i]++;
            pthread_mutex_unlock(&stats->mutex);
            sent_count++;
        } else {
            // Slave disconnected
            slaves[i].active = 0;
            close(slaves[i].fd);
            slaves[i].fd = -1;
            
            // Update shared memory
            pthread_mutex_lock(&stats->mutex);
            stats->active_slaves[i] = 0;
            stats->slave_pids[i] = 0;
            pthread_mutex_unlock(&stats->mutex);
        }
    }
    
    if (sent_count > 0) {
        printf("Master: Sent queries to %d slaves (value=%d)\n", 
               sent_count, query_value);
    }
}

// === Stats Handling ===
static void signal_stats_ready(void) {
    DEBUG_ASSERT(stats_ready_sem != NULL && stats_ready_sem != SEM_FAILED, 
                 "Semaphore should be initialized");
    
    // Post to semaphore to wake up any waiting readers
    int ret = sem_post(stats_ready_sem);
    DEBUG_ASSERT(ret == 0, "Semaphore post should succeed");
    
    printf("Master: Stats updated in shared memory\n");
}

// === Main Loop ===
static int master_main_loop(void) {
    DEBUG_ASSERT(master_fd >= 0, "Master fd should be valid");
    DEBUG_ASSERT(stats != NULL, "Stats should be initialized");
    DEBUG_ASSERT(stats->magic == STATS_MAGIC, "Stats magic should be valid");
    
    struct pollfd pfd = {
        .fd = master_fd,
        .events = POLLIN
    };
    
    int query_counter = 0;
    struct timeval last_query_time, current_time;
    gettimeofday(&last_query_time, NULL);
    
    while (!should_exit) {
        // Handle stats dump request
        if (dump_stats) {
            dump_stats = 0;
            signal_stats_ready();
        }
        
        // Poll for incoming messages
        int ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            return -1;
        }
        
        // Process all available messages
        if (ret > 0 && (pfd.revents & POLLIN)) {
            message_t msg;
            ssize_t bytes;
            
            while ((bytes = read(master_fd, &msg, sizeof(msg))) == sizeof(msg)) {
                DEBUG_ASSERT(msg.type >= MSG_REGISTER && msg.type <= MSG_RESPONSE,
                           "Message type should be valid");
                DEBUG_ASSERT(msg.slave_id >= 0 && msg.slave_id < MAX_SLAVES,
                           "Slave ID in message should be valid");
                
                process_message(&msg);
            }
            
            if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("read");
                return -1;
            }
        }
        
        // Check if it's time to send queries
        gettimeofday(&current_time, NULL);
        long elapsed_ms = (current_time.tv_sec - last_query_time.tv_sec) * 1000 +
                         (current_time.tv_usec - last_query_time.tv_usec) / 1000;
        
        if (elapsed_ms >= QUERY_INTERVAL_MS) {
            send_queries_to_slaves(++query_counter);
            last_query_time = current_time;
        }
    }
    
    return 0;
}

// === Cleanup ===
static void cleanup_resources(void) {
    // Close all slave connections
    for (int i = 0; i < MAX_SLAVES; i++) {
        if (slaves[i].fd >= 0) {
            close(slaves[i].fd);
        }
    }
    
    // Close master FIFO
    if (master_fd >= 0) {
        close(master_fd);
        master_fd = -1;
    }
    unlink(MASTER_FIFO);
    
    // Clean up shared memory
    if (stats != NULL) {
        pthread_mutex_destroy(&stats->mutex);  // Destroy mutex before unmapping
        munmap(stats, sizeof(stats_t));
        stats = NULL;
    }
    shm_unlink(SHM_NAME);
    
    // Clean up semaphore
    if (stats_ready_sem != NULL) {
        sem_close(stats_ready_sem);
        stats_ready_sem = NULL;
    }
    sem_unlink(SEM_NAME);
}

// === Main Function ===
int main(void) {
    // Initialize slave array
    memset(slaves, 0, sizeof(slaves));
    for (int i = 0; i < MAX_SLAVES; i++) {
        slaves[i].fd = -1;
    }
    
    // Set up signal handling
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);
    signal(SIGPIPE, SIG_IGN);
    
    // Register cleanup function
    atexit(cleanup_resources);
    
    // Initialize shared memory
    if (setup_shared_memory() < 0) {
        return 1;
    }
    
    DEBUG_ASSERT(stats != NULL, "Stats should be set up");
    DEBUG_ASSERT(stats->magic == STATS_MAGIC, "Stats should be properly initialized");
    
    // Initialize semaphore
    if (setup_semaphore() < 0) {
        return 1;
    }
    
    DEBUG_ASSERT(stats_ready_sem != SEM_FAILED, "Semaphore should be set up");
    
    // Create master FIFO
    unlink(MASTER_FIFO);
    DEBUG_ASSERT(!file_exists(MASTER_FIFO), "Master FIFO should not exist after unlink");
    
    if (mkfifo(MASTER_FIFO, 0666) < 0) {
        perror("mkfifo master");
        return 1;
    }
    
    DEBUG_ASSERT(file_exists(MASTER_FIFO), "Master FIFO should exist after creation");
    
    // Open FIFO for reading (non-blocking)
    master_fd = open(MASTER_FIFO, O_RDONLY | O_NONBLOCK);
    if (master_fd < 0) {
        perror("open master FIFO");
        return 1;
    }
    
    DEBUG_ASSERT(master_fd >= 0, "Master fd should be valid");
    
    printf("Master: Started (PID=%d)\n", getpid());
    printf("Master: Send SIGUSR1 to display stats\n");
    
    // Run main loop
    int ret = master_main_loop();
    
    printf("Master: Shutting down...\n");
    
    // Final stats update
    signal_stats_ready();
    
    return ret;
}
