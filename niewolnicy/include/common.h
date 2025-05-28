#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <pthread.h>
#include <assert.h>
#include <sys/stat.h>

// Debug mode configuration
#ifndef DEBUG
#define DEBUG 1  // Set to 0 to disable debug assertions
#endif

// Debug assertion macro - only active in debug builds
#if DEBUG
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

// System configuration
#define MAX_SLAVES 10
#define POLL_TIMEOUT_MS 100
#define QUERY_INTERVAL_MS 1000

// IPC paths
#define MASTER_FIFO "/tmp/master_fifo"
#define SLAVE_FIFO_PREFIX "/tmp/slave_fifo_"
#define SHM_NAME "/master_stats"
#define SEM_NAME "/stats_ready"

// Message types
typedef enum {
    MSG_REGISTER = 1,
    MSG_UNREGISTER = 2,
    MSG_QUERY = 3,
    MSG_RESPONSE = 4
} message_type_t;

// Message structure for IPC
typedef struct {
    message_type_t type;
    int slave_id;
    int payload;  // PID for register, value for query/response
} message_t;

// Statistics structure in shared memory
typedef struct {
    pthread_mutex_t mutex;  // Mutex must be first for alignment
    int messages_sent[MAX_SLAVES];
    int messages_received[MAX_SLAVES];
    int slave_pids[MAX_SLAVES];
    int active_slaves[MAX_SLAVES];
    int magic;  // Magic number to verify initialization
} stats_t;

#define STATS_MAGIC ((int)0xDEADBEEF)

// Helper function to check if file exists
static inline int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

#endif /* COMMON_H */
