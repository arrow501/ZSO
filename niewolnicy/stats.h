#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#include <pthread.h>
#include "parameters.h"

// Struktura w pamięci dzielonej
typedef struct {
    pthread_mutex_t mutex;  // Mutex dla synchronizacji
    int messages_sent[MAX_SLAVES];
    int messages_received[MAX_SLAVES];
    int total_sent;
    int total_received;
} stats_t;

#endif /* STATS_H */
