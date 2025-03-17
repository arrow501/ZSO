#ifndef PRODUCT_H
#define PRODUCT_H

/* Parameters */
#define MAX_PRODUCTS 50

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>  
#include <string.h>   
#include <pthread.h>  

/**
 * Product Module
 * 
 * This module handles the shop's inventory system, providing
 * functionality to track, access, and modify product information.
 */

/**
 * Represents a product in the shop inventory.
 */
typedef struct {
    int id;                // Unique product identifier
    char name[50];         // Product name
    int price;             // Product price in cents
    int stock;             // Current inventory quantity
    bool needs_assistant;  // Whether product requires assistant help
} product_t;

/**
 * Initializes the product inventory with default products.
 * Must be called before any other product functions.
 */
void initialize_products();

/**
 * Attempts to retrieve a product from inventory (decrements stock).
 * 
 * @param product_id ID of the product to retrieve
 * @return true if product was successfully retrieved, false otherwise
 */
bool try_get_product(int product_id);

/**
 * Gets the price of a product.
 * 
 * @param product_id ID of the product
 * @return Price of the product in cents
 */
int get_product_price(int product_id);

/**
 * Cleans up product module resources.
 * Should be called before program exit.
 */
void destroy_products();

/**
 * Checks if a product requires assistant preparation.
 * 
 * @param product_id ID of the product to check
 * @return true if product needs assistant, false otherwise
 */
bool product_needs_assistant(int product_id);

#endif /* PRODUCT_H */