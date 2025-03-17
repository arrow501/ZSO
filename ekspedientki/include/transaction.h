#ifndef TRANSACTION_H
#define TRANSACTION_H

/**
 * Transaction Module
 * 
 * This module defines the transaction structure used to represent
 * customer purchases and payment information in the shop system.
 */

/**
 * Represents a sales transaction between a clerk and a customer.
 * Contains the total amount, payment status, and purchased items.
 */
typedef struct transaction_t {
    int total;      // Total cost of all items
    int paid;       // Amount paid by customer
    int* items;     // Array of purchased product IDs
    int items_size; // Number of items purchased
} transaction_t;

#endif /* TRANSACTION_H */
