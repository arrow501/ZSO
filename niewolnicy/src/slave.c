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
    if (master_fd < 0) {
        fprintf(stderr, "Master fd not valid\n");
        return -1;
    }
    if (msg->slave_id != slave_id) {
        fprintf(stderr, "Message slave_id mismatch\n");
        return -1;
    }
    
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
    if (query->type != MSG_QUERY) {
        fprintf(stderr, "Expected query message\n");
        return;
    }
    if (query->slave_id != slave_id) {
        fprintf(stderr, "Query not for this slave\n");
        return;
    }
    
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
    if (slave_id < 0 || slave_id >= MAX_SLAVES) {
        fprintf(stderr, "Invalid slave ID: %d\n", slave_id);
        return 1;
    }
    
    // Setup signal handling
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);
    
    // Create slave FIFO
    snprintf(slave_fifo, sizeof(slave_fifo), "%s%d", SLAVE_FIFO_PREFIX, slave_id);
    unlink(slave_fifo);
    if (file_exists(slave_fifo)) {
        fprintf(stderr, "Failed to remove old slave FIFO\n");
        return 1;
    }
    if (mkfifo(slave_fifo, 0666) != 0) {
        perror("mkfifo");
        return 1;
    }
    
    // Connect to master
    master_fd = open(MASTER_FIFO, O_WRONLY);
    if (master_fd < 0) {
        perror("open master FIFO");
        return 1;
    }
    
    // Register with master
    if (register_with_master() != 0) {
        fprintf(stderr, "Failed to register with master\n");
        return 1;
    }
    
    // Open slave FIFO for reading
    slave_fd = open(slave_fifo, O_RDONLY);
    if (slave_fd < 0) {
        perror("open slave FIFO");
        return 1;
    }
    
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
                if (msg.slave_id != slave_id) {
                    fprintf(stderr, "Message not for this slave\n");
                    continue;
                }
                
                if (msg.type == MSG_QUERY) {
                    process_query(&msg);
                } else {
                    fprintf(stderr, "Slave should only receive queries\n");
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