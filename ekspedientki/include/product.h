/* product.h - Product management for shop simulation */
#ifndef PRODUCT_H
#define PRODUCT_H

#include <stdbool.h>
#include <pthread.h>

#define MAX_PRODUCTS 50
#define MAX_PRODUCT_NAME 32

/* Product structure */
typedef struct {
    int id;                      /* Unique product identifier */
    char name[MAX_PRODUCT_NAME]; /* Product name */
    int price;                   /* Price in cents */
    int stock;                   /* Available quantity */
    bool needs_assistant;        /* Whether product needs assistant preparation */
} product_t;

/* Initialize product system and inventory */
void product_system_init(void);

/* Clean up product system resources */
void product_system_cleanup(void);

/* Initialize the product inventory with default items */
void initialize_products(void);

/**
 * Attempt to get a product from inventory
 * 
 * @param product_id ID of the product to retrieve
 * @return true if product was available and retrieved, false otherwise
 */
bool try_get_product(int product_id);

/**
 * Return a product to inventory (e.g., if transaction failed)
 * 
 * @param product_id ID of the product to return
 */
void return_product(int product_id);

/**
 * Check if a product requires assistant preparation
 * 
 * @param product_id ID of the product to check
 * @return true if product needs assistant, false otherwise
 */
bool product_needs_assistant(int product_id);

/**
 * Get product information
 * 
 * @param product_id ID of the product
 * @return Pointer to the product, or NULL if invalid ID
 */
const product_t* get_product_info(int product_id);

/* Get the total number of products in inventory */
int get_product_count(void);

/* External declarations for product data */
extern product_t products[MAX_PRODUCTS];
extern pthread_mutex_t inventory_mutex;

#endif /* PRODUCT_H */