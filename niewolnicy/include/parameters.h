#ifndef PARAMETERS_H
#define PARAMETERS_H

/**
 * Parameters Module
 * 
 * Global configuration parameters for the IPC system.
 */

/** Maximum number of slave processes */
#ifndef MAX_SLAVES
#define MAX_SLAVES 10
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

/** System UUID for unique IPC resource names */
#ifndef SYSTEM_UUID
#define SYSTEM_UUID "2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f"
#endif

#endif /* PARAMETERS_H */