#ifndef COMMON_H
#define COMMON_H

#include "parameters.h"
#include <stdint.h>
#include <pthread.h>
#include <assert.h>
#include <sys/stat.h>
#include <stdio.h>

// Assertion macro - only active when enabled
#if ENABLE_ASSERTS
    #define DEBUG_ASSERT(cond, msg) \
        do { \
            if (!(cond)) { \
                fprintf(stderr, "ASSERTION FAILED: %s\n", msg); \
                fprintf(stderr, "  at %s:%d in %s\n", __FILE__, __LINE__, __func__); \
                assert(cond); \
            } \
        } while(0)
#else
    #define DEBUG_ASSERT(cond, msg) ((void)0)
#endif

// System paths
#define MASTER_FIFO "/tmp/master_fifo"
#define SLAVE_FIFO_PREFIX "/tmp/slave_fifo_"
#define MASTER_PID_FILE "/tmp/master_pid"
#define SHM_NAME "/master_stats"
#define SEM_NAME "/stats_ready"

// Message types
typedef enum {
    MSG_REGISTER = 1,
    MSG_UNREGISTER = 2,
    MSG_QUERY = 3,
    MSG_RESPONSE = 4
} message_type_t;

// Message structure
typedef struct {
    message_type_t type;
    int slave_id;
    int payload;
} message_t;

// Statistics in shared memory
typedef struct {
    pthread_mutex_t mutex;
    pid_t master_pid;
    int messages_sent[NUM_SLAVES];
    int messages_received[NUM_SLAVES];
    int active_slaves[NUM_SLAVES];
    int magic;
} stats_t;

#define STATS_MAGIC 0xDEADBEEF

// Helper function
static inline int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

#endif /* COMMON_H */