#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

typedef struct {
    int type;           // Typ komunikatu
    int slave_id;       // ID slave'a
    int payload;        // Dane komunikatu
} message_t;

#endif /* MESSAGE_H */
