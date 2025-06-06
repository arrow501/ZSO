#ifndef PARAMETERS_H
#define PARAMETERS_H

/**
 * Parameters Module
 * 
 * Global configuration parameters for the IPC system.
 */

/** Number of slave processes */
#ifndef NUM_SLAVES
#define NUM_SLAVES 3
#endif

/** Number of messages each slave should process before exiting */
#ifndef NUM_MESSAGES_PER_SLAVE
#define NUM_MESSAGES_PER_SLAVE 10
#endif

/** Controls debug output (1 = enabled, 0 = disabled) */
#ifndef ENABLE_PRINTING
#define ENABLE_PRINTING 1
#endif

/** Controls assertions (1 = enabled, 0 = disabled) */
#ifndef ENABLE_ASSERTS
#define ENABLE_ASSERTS 1
#endif

/** Poll timeout in milliseconds for responsiveness */
#ifndef POLL_TIMEOUT_MS
#define POLL_TIMEOUT_MS 100
#endif

/** Query interval in milliseconds */
#ifndef QUERY_INTERVAL_MS
#define QUERY_INTERVAL_MS 1000
#endif

#endif /* PARAMETERS_H */