/* product.c - Product management for shop simulation */
#include <stdio.h>
#include <string.h>
#include "product.h"

/* Macro for initializing products */
#define INIT_PRODUCT(pid, pname, pprice, pstock, passist) \
do { \
    products[pid].id = pid; \
    strcpy(products[pid].name, pname); \
    products[pid].price = pprice; \
    products[pid].stock = pstock; \
    products[pid].needs_assistant = passist; \
} while (0)

/* Global product array definition */
product_t products[MAX_PRODUCTS];
pthread_mutex_t inventory_mutex;

void product_system_init(void) {
    /* Initialize the inventory mutex */
    pthread_mutex_init(&inventory_mutex, NULL);
    
    /* Setup the product inventory */
    initialize_products();
    
    /* For debug purposes */
    #ifdef DEBUG
    printf("Product system initialized with %d products\n", MAX_PRODUCTS);
    #endif
}

void product_system_cleanup(void) {
    /* Destroy the inventory mutex */
    pthread_mutex_destroy(&inventory_mutex);
    
    #ifdef DEBUG
    printf("Product system cleaned up\n");
    #endif
}

void initialize_products(void) {
    /* Initialize all products */
    INIT_PRODUCT(0, "Banana", 129, 45, false);
    INIT_PRODUCT(1, "Apple", 159, 50, false);
    INIT_PRODUCT(2, "Bread", 349, 32, false);
    INIT_PRODUCT(3, "Milk", 399, 40, false);
    INIT_PRODUCT(4, "Eggs", 599, 30, false);
    INIT_PRODUCT(5, "Pasta", 259, 35, false);
    INIT_PRODUCT(6, "Rice", 329, 48, false);
    INIT_PRODUCT(7, "Salt", 159, 60, false);
    INIT_PRODUCT(8, "Sugar", 289, 55, false);
    INIT_PRODUCT(9, "Chocolate", 499, 40, false);
    INIT_PRODUCT(10, "Cheese", 899, 25, false);
    INIT_PRODUCT(11, "Yogurt", 449, 30, false);
    INIT_PRODUCT(12, "Butter", 599, 28, false);
    INIT_PRODUCT(13, "Coffee", 999, 35, false);
    INIT_PRODUCT(14, "Tea", 599, 40, false);
    INIT_PRODUCT(15, "Juice", 449, 38, false);
    INIT_PRODUCT(16, "Water", 149, 70, false);
    INIT_PRODUCT(17, "Soda", 249, 60, false);
    INIT_PRODUCT(18, "Chips", 349, 45, false);
    INIT_PRODUCT(19, "Cookies", 399, 35, false);
    INIT_PRODUCT(20, "Cereal", 459, 30, false);
    INIT_PRODUCT(21, "Jam", 399, 25, false);
    INIT_PRODUCT(22, "Honey", 799, 20, false);
    INIT_PRODUCT(23, "Nuts", 699, 30, false);
    INIT_PRODUCT(24, "Peanuts", 499, 35, false);
    INIT_PRODUCT(25, "Candy", 299, 50, false);
    INIT_PRODUCT(26, "Pepper", 199, 40, false);
    INIT_PRODUCT(27, "Oil", 599, 30, false);
    INIT_PRODUCT(28, "Flour", 349, 35, false);
    INIT_PRODUCT(29, "Tuna", 599, 30, false);
    INIT_PRODUCT(30, "Soup", 399, 25, false);
    INIT_PRODUCT(31, "Beans", 299, 40, false);
    INIT_PRODUCT(32, "Tomato", 179, 60, false);
    INIT_PRODUCT(33, "Potato", 199, 55, false);
    INIT_PRODUCT(34, "Onion", 129, 65, false);
    INIT_PRODUCT(35, "Garlic", 159, 45, false);
    INIT_PRODUCT(36, "Lemon", 129, 40, false);
    INIT_PRODUCT(37, "Orange", 179, 50, false);
    INIT_PRODUCT(38, "Beef", 1299, 20, false);
    INIT_PRODUCT(39, "Chicken", 999, 25, false);
    INIT_PRODUCT(40, "Cake", 899, 15, true);
    INIT_PRODUCT(41, "Deli Meat", 799, 25, true);
    INIT_PRODUCT(42, "Fresh Fish", 1299, 20, true);
    INIT_PRODUCT(43, "Sliced Bread", 399, 30, true);
    INIT_PRODUCT(44, "Cheese Wheel", 1599, 10, true);
    INIT_PRODUCT(45, "Custom Coffee", 699, 35, true);
    INIT_PRODUCT(46, "Watermelon", 599, 20, true);
    INIT_PRODUCT(47, "Fresh Meat", 1099, 15, true);
    INIT_PRODUCT(48, "Salad Mix", 349, 30, true);
    INIT_PRODUCT(49, "Fresh Juice", 899, 25, true);
    
    #ifdef DEBUG
    printf("Products initialized\n");
    #endif
}

bool try_get_product(int product_id) {
    bool success = false;
    
    /* Ensure thread-safe access to inventory */
    pthread_mutex_lock(&inventory_mutex);
    
    /* Check if product is valid and in stock */
    if (product_id >= 0 && product_id < MAX_PRODUCTS && products[product_id].stock > 0) {
        /* Decrement stock */
        products[product_id].stock--;
        success = true;
        
        #ifdef DEBUG
        printf("Product %d (%s) taken from inventory, %d remaining\n", 
               product_id, products[product_id].name, products[product_id].stock);
        #endif
    } else {
        #ifdef DEBUG
        if (product_id >= 0 && product_id < MAX_PRODUCTS) {
            printf("Product %d (%s) out of stock\n", product_id, products[product_id].name);
        } else {
            printf("Invalid product ID: %d\n", product_id);
        }
        #endif
    }
    
    pthread_mutex_unlock(&inventory_mutex);
    return success;
}

void return_product(int product_id) {
    /* Ensure thread-safe access to inventory */
    pthread_mutex_lock(&inventory_mutex);
    
    /* Check if product is valid */
    if (product_id >= 0 && product_id < MAX_PRODUCTS) {
        /* Increment stock */
        products[product_id].stock++;
        
        #ifdef DEBUG
        printf("Product %d (%s) returned to inventory, now %d in stock\n", 
               product_id, products[product_id].name, products[product_id].stock);
        #endif
    } else {
        #ifdef DEBUG
        printf("Invalid product ID for return: %d\n", product_id);
        #endif
    }
    
    pthread_mutex_unlock(&inventory_mutex);
}

bool product_needs_assistant(int product_id) {
    if (product_id >= 0 && product_id < MAX_PRODUCTS) {
        return products[product_id].needs_assistant;
    }
    return false;
}

const product_t* get_product_info(int product_id) {
    if (product_id >= 0 && product_id < MAX_PRODUCTS) {
        return &products[product_id];
    }
    return NULL;
}

int get_product_count(void) {
    return MAX_PRODUCTS;
}