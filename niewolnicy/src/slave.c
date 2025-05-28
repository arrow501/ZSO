#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <poll.h>
#include "../include/common.h"

// Global state for signal handlers
static volatile sig_atomic_t should_exit = 0;
static volatile sig_atomic_t notify_master = 0;
static int slave_id = -1;
static int master_fd = -1;

// === Signal Handling ===
static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        notify_master = 1;  // Tell master we're leaving
        should_exit = 1;    // Then exit gracefully
    }
}

// === Communication Functions ===
static int send_message_to_master(const message_t *msg) {
    DEBUG_ASSERT(master_fd >= 0, "Master fd should be valid when sending message");
    DEBUG_ASSERT(msg != NULL, "Message pointer should not be NULL");
    DEBUG_ASSERT(msg->slave_id == slave_id, "Message slave_id should match our ID");
    
    if (master_fd < 0) return -1;
    
    ssize_t written = write(master_fd, msg, sizeof(message_t));
    if (written != sizeof(message_t)) {
        if (errno != EPIPE) {  // EPIPE means master is gone
            perror("write to master");
        }
        return -1;
    }
    
    DEBUG_ASSERT(written == sizeof(message_t), "Should write complete message");
    return 0;
}

static int register_with_master(void) {
    DEBUG_ASSERT(slave_id >= 0 && slave_id < MAX_SLAVES, "Slave ID should be valid");
    DEBUG_ASSERT(getpid() > 0, "PID should be valid");
    
    message_t msg = {
        .type = MSG_REGISTER,
        .slave_id = slave_id,
        .payload = getpid()
    };
    
    return send_message_to_master(&msg);
}

static int unregister_from_master(void) {
    message_t msg = {
        .type = MSG_UNREGISTER,
        .slave_id = slave_id,
        .payload = 0
    };
    
    return send_message_to_master(&msg);
}

static void process_query(const message_t *query) {
    DEBUG_ASSERT(query != NULL, "Query should not be NULL");
    DEBUG_ASSERT(query->type == MSG_QUERY, "Message type should be QUERY");
    DEBUG_ASSERT(query->slave_id == slave_id, "Query should be addressed to us");
    
    // Simple processing: double the value
    message_t response = {
        .type = MSG_RESPONSE,
        .slave_id = slave_id,
        .payload = query->payload * 2
    };
    
    send_message_to_master(&response);
}

// === Main Loop Functions ===
static int setup_slave_fifo(char *path, size_t path_size) {
    DEBUG_ASSERT(path != NULL, "Path buffer should not be NULL");
    DEBUG_ASSERT(path_size >= 256, "Path buffer should be large enough");
    DEBUG_ASSERT(slave_id >= 0 && slave_id < MAX_SLAVES, "Slave ID should be valid");
    
    snprintf(path, path_size, "%s%d", SLAVE_FIFO_PREFIX, slave_id);
    
    // Check if old FIFO exists and remove it
    int existed = file_exists(path);
    if (existed) {
        if (unlink(path) < 0) {
            perror("unlink old slave FIFO");
            return -1;
        }
    }
    
    // Verify it's gone
    DEBUG_ASSERT(!file_exists(path), "Old FIFO should be deleted before creating new one");
    
    if (mkfifo(path, 0666) < 0) {
        perror("mkfifo slave");
        return -1;
    }
    
    // Verify it was created
    DEBUG_ASSERT(file_exists(path), "FIFO should exist after creation");
    
    return 0;
}

static void cleanup_resources(const char *fifo_path) {
    if (master_fd >= 0) {
        close(master_fd);
        master_fd = -1;
    }
    
    if (fifo_path[0] != '\0') {
        unlink(fifo_path);
        DEBUG_ASSERT(!file_exists(fifo_path), "Slave FIFO should be deleted after cleanup");
    }
}

static int slave_main_loop(int slave_read_fd) {
    DEBUG_ASSERT(slave_read_fd >= 0, "Slave read fd should be valid");
    
    struct pollfd pfd = {
        .fd = slave_read_fd,
        .events = POLLIN
    };
    
    while (!should_exit) {
        // Check if we need to notify master (from signal)
        if (notify_master) {
            unregister_from_master();
            notify_master = 0;
        }
        
        // Wait for messages with timeout
        int ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
        
        if (ret < 0) {
            if (errno == EINTR) continue;  // Signal interrupted
            perror("poll");
            return -1;
        }
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            message_t msg;
            ssize_t bytes = read(slave_read_fd, &msg, sizeof(msg));
            
            if (bytes == sizeof(msg)) {
                DEBUG_ASSERT(msg.slave_id == slave_id, "Message should be for this slave");
                
                if (msg.type == MSG_QUERY) {
                    process_query(&msg);
                } else {
                    DEBUG_ASSERT(0, "Slave should only receive QUERY messages");
                }
            } else if (bytes == 0) {
                // Master closed connection
                printf("Slave %d: Master disconnected\n", slave_id);
                break;
            } else if (bytes < 0 && errno != EINTR) {
                perror("read");
                return -1;
            }
        }
    }
    
    return 0;
}

// === Main Function ===
int main(int argc, char *argv[]) {
    char slave_fifo_path[256] = "";
    int slave_read_fd = -1;
    int exit_code = 1;
    
    // Parse arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <slave_id>\n", argv[0]);
        return 1;
    }
    
    slave_id = atoi(argv[1]);
    if (slave_id < 0 || slave_id >= MAX_SLAVES) {
        fprintf(stderr, "Invalid slave_id: %d (must be 0-%d)\n", 
                slave_id, MAX_SLAVES - 1);
        return 1;
    }
    
    // Setup signal handling
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe
    
    // Create slave FIFO
    if (setup_slave_fifo(slave_fifo_path, sizeof(slave_fifo_path)) < 0) {
        goto cleanup;
    }
    
    // Connect to master
    master_fd = open(MASTER_FIFO, O_WRONLY);
    if (master_fd < 0) {
        perror("open master FIFO");
        goto cleanup;
    }
    
    // Register with master
    if (register_with_master() < 0) {
        fprintf(stderr, "Failed to register with master\n");
        goto cleanup;
    }
    
    // Open slave FIFO for reading
    slave_read_fd = open(slave_fifo_path, O_RDONLY);
    if (slave_read_fd < 0) {
        perror("open slave FIFO");
        goto cleanup;
    }
    
    printf("Slave %d: Started (PID=%d)\n", slave_id, getpid());
    
    // Run main loop
    if (slave_main_loop(slave_read_fd) == 0) {
        exit_code = 0;
    }
    
    // Send final unregister if not already done
    if (!notify_master && !should_exit) {
        unregister_from_master();
    }
    
cleanup:
    if (slave_read_fd >= 0) close(slave_read_fd);
    cleanup_resources(slave_fifo_path);
    
    printf("Slave %d: Exiting\n", slave_id);
    return exit_code;
}
