#ifndef PRODUCT_H
#define PRODUCT_H

/* Parameters */
#define MAX_PRODUCTS 50


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>  
#include <string.h>   
#include <pthread.h>  

// Structure definition
typedef struct {
    int id;
    char name[50];
    int price;
    int stock;
    bool needs_assistant;
} product_t;



void initialize_products();

bool try_get_product(int product_id);

int get_product_price(int product_id);

void destroy_products();

bool product_needs_assistant(int product_id);

#endif