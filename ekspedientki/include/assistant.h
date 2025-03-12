/* assistant.h - Assistant thread interface */
#ifndef ASSISTANT_H
#define ASSISTANT_H

#include "shop.h"

/**
 * Initialize an assistant
 * 
 * @param assistant Pointer to assistant structure to initialize
 * @return 0 on success, non-zero on failure
 */
int assistant_init(assistant_t* assistant);

/**
 * Start an assistant thread
 * 
 * @param assistant Pointer to assistant structure
 * @param shop Pointer to shop structure
 * @return 0 on success, non-zero on failure
 */
int assistant_start(assistant_t* assistant, shop_t* shop);

/**
 * Clean up assistant resources
 * 
 * @param assistant Pointer to assistant structure to clean up
 */
void assistant_cleanup(assistant_t* assistant);

/**
 * Stop an assistant's processing
 * 
 * @param assistant Pointer to assistant structure to stop
 */
void assistant_stop(assistant_t* assistant);

/**
 * Create a new assistant task
 * 
 * @param type Task type
 * @param product_id Product ID for preparation
 * @param clerk_id Requesting clerk ID
 * @return Pointer to new task structure, or NULL on failure
 */
assistant_task_t* assistant_create_task(assistant_task_type_t type, int product_id, int clerk_id);

/**
 * Free an assistant task
 * 
 * @param task Pointer to task structure to free
 */
void assistant_free_task(void* task);

/**
 * Add a task to assistant's queue
 * 
 * @param shop Pointer to shop structure
 * @param task Pointer to task to add
 * @return 0 on success, non-zero on failure
 */
int assistant_add_task(shop_t* shop, assistant_task_t* task);

/**
 * Wait for a task to complete
 * 
 * @param task Pointer to task to wait for
 * @return true if task completed successfully, false otherwise
 */
bool assistant_wait_for_task(assistant_task_t* task);

/* Assistant thread entry point */
void* assistant_thread(void* arg);

#endif /* ASSISTANT_H */