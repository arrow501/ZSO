#ifndef PARAMETERS_H
#define PARAMETERS_H

// Parametryzacja programu
#ifndef MAX_SLAVES
#define MAX_SLAVES 10
#endif

#ifndef MAX_MESSAGES
#define MAX_MESSAGES 100
#endif

// Kontrola debug output
#ifndef ENABLE_PRINTING
#define ENABLE_PRINTING 1
#endif

// Stałe systemowe
#define MASTER_FIFO "/tmp/master_fifo"
#define SLAVE_FIFO_PREFIX "/tmp/slave_fifo_"
#define SHM_NAME "/master_stats"
#define SEM_NAME "/master_sem"

// Typy komunikatów  
#define MSG_REGISTER 1
#define MSG_UNREGISTER 2
#define MSG_QUERY 3
#define MSG_RESPONSE 4

#endif /* PARAMETERS_H */
