#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/common.h"

// Global state - keep it minimal
static volatile sig_atomic_t should_exit = 0;
static int slave_id = -1;
static int master_fd = -1;
static int slave_fd = -1;
static int messages_processed = 0;
static char slave_fifo[256];

static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGQUIT) {  // Only handle termination signals
        should_exit = 1;
    } else if (sig == SIGUSR1) {
        // Signal to slave causes it to notify master it's finishing
        should_exit = 1;
    }
}

static void send_to_master(const message_t *msg) {
    if (master_fd >= 0) {
        write(master_fd, msg, sizeof(message_t));
    }
}

static void register_with_master(void) {
    message_t msg = {
        .type = MSG_REGISTER,
        .slave_id = slave_id,
        .payload = getpid()
    };
    send_to_master(&msg);
}

static void unregister_from_master(void) {
    message_t msg = {
        .type = MSG_UNREGISTER,
        .slave_id = slave_id,
        .payload = 0
    };
    send_to_master(&msg);
}

static void process_query(const message_t *query) {
    if (query->type != MSG_QUERY || query->slave_id != slave_id) {
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
    }
}

static void cleanup(void) {
    if (should_exit) {
        unregister_from_master();
    }
    
    if (slave_fd >= 0) {
        close(slave_fd);
    }
    if (master_fd >= 0) {
        close(master_fd);
    }
    unlink(slave_fifo);
}

int main(int argc, char *argv[]) {
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
    
    // Setup signal handling - SIGINT is ignored (set by main before exec)
    signal(SIGTERM, handle_signal);
    signal(SIGQUIT, handle_signal);
    signal(SIGUSR1, handle_signal);  // Signal to slave → notify master finishing
    signal(SIGPIPE, SIG_IGN);
    atexit(cleanup);
    
    // Create slave FIFO FIRST
    snprintf(slave_fifo, sizeof(slave_fifo), "%s%d", SLAVE_FIFO_PREFIX, slave_id);
    unlink(slave_fifo);
    if (mkfifo(slave_fifo, 0666) != 0) {
        perror("mkfifo");
        return 1;
    }
    
    // Wait for master FIFO to exist
    for (int i = 0; i < 100; i++) {
        if (file_exists(MASTER_FIFO)) break;
        for (volatile int j = 0; j < 10000; j++);
    }
    
    // Connect to master
    master_fd = open(MASTER_FIFO, O_WRONLY);
    if (master_fd < 0) {
        perror("open master FIFO");
        return 1;
    }
    
    // Register with master BEFORE opening our FIFO for reading
    register_with_master();
    
    // Now open slave FIFO for reading (this will block until master opens for writing)
    slave_fd = open(slave_fifo, O_RDONLY);
    if (slave_fd < 0) {
        perror("open slave FIFO");
        return 1;
    }
    
#if ENABLE_PRINTING
    printf("Slave %d: Started (PID=%d)\n", slave_id, getpid());
#endif
    
    // Simple main loop
    while (!should_exit) {
        message_t msg;
        ssize_t bytes = read(slave_fd, &msg, sizeof(msg));
        
        if (bytes == sizeof(msg)) {
            process_query(&msg);
        } else if (bytes == 0) {
            // Master disconnected
            break;
        } else if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // Real error
            break;
        }
        
        // Small delay to prevent busy waiting
        for (volatile int i = 0; i < 1000; i++);
    }
    
#if ENABLE_PRINTING
    printf("Slave %d: Exiting (processed %d messages)\n", slave_id, messages_processed);
#endif
    
    return 0;
}