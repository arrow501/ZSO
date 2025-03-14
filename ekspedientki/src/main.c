/* main.c - Simulation control */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "shop.h"
#include "product.h"
#include "clerk.h"
#include "assistant.h"
#include "customer.h"
#include <linux/time.h>

/* Constants */

/* Shop instance */
static shop_t shop;

/* Customer threads */
#define NUM_CUSTOMERS 20
static customer_t customers[NUM_CUSTOMERS];

/* Helper structure for thread arguments */
typedef struct {
    union {
        clerk_t* clerk;
        assistant_t* assistant;
        customer_t* customer;
    };
    shop_t* shop;
} thread_arg_t;

/* Initialize simulation */
static int init_simulation(void) {
    int i;
    
    /* Seed random number generator */
    srand(time(NULL));
    
    /* Initialize shop state */
    memset(&shop, 0, sizeof(shop));
    pthread_mutex_init(&shop.mutex, NULL);
    pthread_cond_init(&shop.shutdown_complete, NULL);
    
    /* Initialize product system */
    product_system_init();
    
    /* Initialize clerk threads */
    for (i = 0; i < NUM_CLERKS; i++) {
        if (clerk_init(&shop.clerks[i], i) != 0) {
            fprintf(stderr, "Failed to initialize clerk %d\n", i);
            return -1;
        }
    }
    
    /* Initialize assistant thread */
    if (assistant_init(&shop.assistant) != 0) {
        fprintf(stderr, "Failed to initialize assistant\n");
        return -1;
    }
    
    /* Initialize customer threads */
    for (i = 0; i < NUM_CUSTOMERS; i++) {
        if (customer_init(&customers[i], i) != 0) {
            fprintf(stderr, "Failed to initialize customer %d\n", i);
            return -1;
        }
        
        /* Generate random shopping list */
        customer_generate_shopping_list(&customers[i], 3, 8);
    }
    
    return 0;
}

/* Start simulation threads */
static int start_threads(void) {
    int i;
    
    /* Start clerk threads */
    for (i = 0; i < NUM_CLERKS; i++) {
        if (clerk_start(&shop.clerks[i], &shop) != 0) {
            fprintf(stderr, "Failed to start clerk %d\n", i);
            return -1;
        }
    }
    
    /* Start assistant thread */
    if (assistant_start(&shop.assistant, &shop) != 0) {
        fprintf(stderr, "Failed to start assistant\n");
        return -1;
    }
    
    /* Start customer threads */
    for (i = 0; i < NUM_CUSTOMERS; i++) {
        if (customer_start(&customers[i], &shop) != 0) {
            fprintf(stderr, "Failed to start customer %d\n", i);
            return -1;
        }
    }
    
    return 0;
}

/* Wait for customers to finish */
static void wait_for_customers(void) {
    int i;
    
    /* Join all customer threads */
    for (i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_join(customers[i].thread, NULL);
    }
}

/* Stop and cleanup simulation */
static void cleanup_simulation(void) {
    int i;
    
    /* Set shutdown flag */
    pthread_mutex_lock(&shop.mutex);
    shop.shutdown = true;
    pthread_mutex_unlock(&shop.mutex);
    
    /* Stop clerk threads */
    for (i = 0; i < NUM_CLERKS; i++) {
        clerk_stop(&shop.clerks[i]);
        clerk_cleanup(&shop.clerks[i]);
    }
    
    /* Stop assistant thread */
    assistant_stop(&shop.assistant);
    assistant_cleanup(&shop.assistant);
    
    /* Cleanup customer resources */
    for (i = 0; i < NUM_CUSTOMERS; i++) {
        customer_cleanup(&customers[i]);
    }
    
    /* Cleanup shop resources */
    pthread_mutex_destroy(&shop.mutex);
    pthread_cond_destroy(&shop.shutdown_complete);
    
    /* Cleanup product system */
    product_system_cleanup();
}

/* Print simulation results */
static void print_results(void) {
    int i;
    int total_items = 0;
    int total_sales = 0;
    
    printf("\n==== Simulation Results ====\n");
    printf("Customers processed: %d\n", shop.customers_processed);
    
    for (i = 0; i < NUM_CLERKS; i++) {
        total_items += shop.clerks[i].items_processed;
        total_sales += shop.clerks[i].total_sales;
        
        printf("Clerk %d: processed %d items, total sales: $%d.%02d\n",
               i, shop.clerks[i].items_processed,
               shop.clerks[i].total_sales / 100, shop.clerks[i].total_sales % 100);
    }
    
    printf("Total items sold: %d\n", total_items);
    printf("Total sales: $%d.%02d\n", total_sales / 100, total_sales % 100);
    printf("============================\n");
}

/* Main simulation function */
int projekt_zso(void) {
    /* Initialize simulation */
    if (init_simulation() != 0) {
        fprintf(stderr, "Failed to initialize simulation\n");
        cleanup_simulation();
        return -1;
    }
    
    /* Start simulation threads */
    if (start_threads() != 0) {
        fprintf(stderr, "Failed to start simulation threads\n");
        cleanup_simulation();
        return -1;
    }
    
    /* Wait for customers to finish (with timeout) */
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += SIMULATION_TIME_SEC;
    
    /* Run for specified time */
    sleep(SIMULATION_TIME_SEC);
    
    /* Clean up and print results */
    cleanup_simulation();
    print_results();
    
    return 0;
}

/* Test main function */
#ifdef TEST_MAIN
int main(void) {
    int i;
    
    printf("Starting shop simulation...\n");
    
    /* Run simulation multiple times */
    for (i = 0; i < 10; i++) {
        printf("\n=== Run %d ===\n", i + 1);
        projekt_zso();
    }
    
    return 0;
}
#endif