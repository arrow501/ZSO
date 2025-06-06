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

// Global state
static volatile sig_atomic_t should_exit = 0;
static volatile sig_atomic_t notify_master = 0;
static int slave_id = -1;
static int master_fd = -1;
static int messages_processed = 0;

static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        notify_master = 1;
        should_exit = 1;
    }
}

static int send_to_master(const message_t *msg) {
    DEBUG_ASSERT(master_fd >= 0, "Master fd should be valid");
    DEBUG_ASSERT(msg->slave_id == slave_id, "Message should be from this slave");
    
    ssize_t written = write(master_fd, msg, sizeof(message_t));
    if (written != sizeof(message_t)) {
        if (errno != EPIPE) {
            perror("write to master");
        }
        return -1;
    }
    return 0;
}

static int register_with_master(void) {
    message_t msg = {
        .type = MSG_REGISTER,
        .slave_id = slave_id,
        .payload = getpid()
    };
    return send_to_master(&msg);
}

static int unregister_from_master(void) {
    message_t msg = {
        .type = MSG_UNREGISTER,
        .slave_id = slave_id,
        .payload = 0
    };
    return send_to_master(&msg);
}

static void process_query(const message_t *query) {
    DEBUG_ASSERT(query->type == MSG_QUERY, "Should be query message");
    DEBUG_ASSERT(query->slave_id == slave_id, "Query should be for this slave");
    
    // Simple processing: double the value
    message_t response = {
        .type = MSG_RESPONSE,
        .slave_id = slave_id,
        .payload = query->payload * 2
    };
    
    send_to_master(&response);
    messages_processed++;
    
#if ENABLE_PRINTING
    printf("Slave %d: Processed query %d -> response %d (total: %d)\n", 
           slave_id, query->payload, response.payload, messages_processed);
#endif
    
    // Exit after processing enough messages
    if (messages_processed >= NUM_MESSAGES_PER_SLAVE) {
        should_exit = 1;
        notify_master = 1;
    }
}

static void cleanup(const char *fifo_path) {
    if (master_fd >= 0) {
        close(master_fd);
    }
    
    if (fifo_path && fifo_path[0]) {
        unlink(fifo_path);
    }
}

int main(int argc, char *argv[]) {
    char slave_fifo[256];
    int slave_fd = -1;
    
    // Parse arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <slave_id>\n", argv[0]);
        return 1;
    }
    
    slave_id = atoi(argv[1]);
    DEBUG_ASSERT(slave_id >= 0 && slave_id < MAX_SLAVES, "Valid slave ID");
    
    // Setup signal handling
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);
    
    // Create slave FIFO
    snprintf(slave_fifo, sizeof(slave_fifo), "%s%d", SLAVE_FIFO_PREFIX, slave_id);
    unlink(slave_fifo);
    DEBUG_ASSERT(!file_exists(slave_fifo), "No leftover slave FIFO");
    DEBUG_ASSERT(mkfifo(slave_fifo, 0666) == 0, "Should create slave FIFO");
    
    // Connect to master
    master_fd = open(MASTER_FIFO, O_WRONLY);
    DEBUG_ASSERT(master_fd >= 0, "Should connect to master");
    
    // Register with master
    DEBUG_ASSERT(register_with_master() == 0, "Should register with master");
    
    // Open slave FIFO for reading
    slave_fd = open(slave_fifo, O_RDONLY);
    DEBUG_ASSERT(slave_fd >= 0, "Should open slave FIFO");
    
#if ENABLE_PRINTING
    printf("Slave %d: Started (PID=%d)\n", slave_id, getpid());
#endif
    
    // Main loop
    struct pollfd pfd = { .fd = slave_fd, .events = POLLIN };
    
    while (!should_exit) {
        // Handle unregister signal
        if (notify_master) {
            unregister_from_master();
            notify_master = 0;
        }
        
        // Poll for messages
        int ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            message_t msg;
            ssize_t bytes = read(slave_fd, &msg, sizeof(msg));
            
            if (bytes == sizeof(msg)) {
                DEBUG_ASSERT(msg.slave_id == slave_id, "Message should be for this slave");
                
                if (msg.type == MSG_QUERY) {
                    process_query(&msg);
                } else {
                    DEBUG_ASSERT(0, "Slave should only receive queries");
                }
            } else if (bytes == 0) {
                // Master disconnected
#if ENABLE_PRINTING
                printf("Slave %d: Master disconnected\n", slave_id);
#endif
                break;
            }
        }
    }
    
    // Final cleanup
    if (slave_fd >= 0) {
        close(slave_fd);
    }
    cleanup(slave_fifo);
    
#if ENABLE_PRINTING
    printf("Slave %d: Exiting (processed %d messages)\n", slave_id, messages_processed);
#endif
    
    return 0;
}