#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


int simple_test() {
    queue* q = queue_create();
    int* data = (int*)malloc(sizeof(int));
    *data = 42;
    queue_push(q, data);
    int* popped = (int*)queue_pop(q);
    printf("Popped: %d\n", *popped);
    free(popped);
    queue_destroy(q);
    return 0;
}

void* producer_thread(void* arg) {
    queue* q = (queue*)arg;
    for (int i = 0; i < 10; i++) {
        int* data = (int*)malloc(sizeof(int));
        *data = i;
        queue_push(q, data);
        printf("Produced: %d\n", i);
    }
    return NULL;
}

void* consumer_thread(void* arg) {
    queue* q = (queue*)arg;
    for (int i = 0; i < 10; i++) {
        int* popped = (int*)queue_pop(q);
        printf("Consumed: %d\n", *popped);
        free(popped);
    }
    return NULL;
}



int locking_test() {
    pthread_t producer, consumer;
    queue* q = queue_create();
    pthread_create(&consumer, NULL, consumer_thread, q);
    sleep(1);
    pthread_create(&producer, NULL, producer_thread, q);
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    queue_destroy(q);
    return 0;
}

int main() {
    simple_test();
    locking_test();
    return 0;
}
