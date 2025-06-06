#ifndef COMMON_H
#define COMMON_H

#include "parameters.h"
#include <stdint.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdio.h>

// System paths with UUID
#define MASTER_FIFO "/tmp/master_fifo_" SYSTEM_UUID
#define SLAVE_FIFO_PREFIX "/tmp/slave_fifo_" SYSTEM_UUID "_"
#define SHM_NAME "/master_stats_" SYSTEM_UUID  

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
    int messages_sent[MAX_SLAVES];
    int messages_received[MAX_SLAVES];
    int active_slaves[MAX_SLAVES];
} stats_t;

// Helper function
static inline int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

#endif /* COMMON_H */