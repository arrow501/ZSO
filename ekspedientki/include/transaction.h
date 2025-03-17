#ifndef TRANSACTION_H
#define TRANSACTION_H

typedef struct transaction_t {
    int total;
    int paid;
    int* items;
    int items_size;
} transaction_t;

#endif /* TRANSACTION_H */
