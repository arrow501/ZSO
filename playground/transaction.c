#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_NUMBERS


typedef struct client_t
{
    int client_id;
    int *numbers;  // Pointer to dynamically allocated array
    int wallet;
};

typedef struct server_t
{
    int server_id;
    int total;
};

void* serve(void*){

}
void* request(void*){

}
