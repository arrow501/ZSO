#include <stdio.h>
#include <pthread.h>

#define MAX_COUNTER 1000000

int counter = 0;

pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t t1, t2;


void* increment(void* arg){
    while(counter < MAX_COUNTER){
        pthread_mutex_lock(&counter_mutex);
        if(counter >= MAX_COUNTER){
            pthread_exit(0);
        }
        counter++;
        if(counter%100==0)
            printf("I'm thread %d and i incremented the counter to %d\n", (int)arg, counter);
        pthread_mutex_unlock(&counter_mutex);
    }
    return 0;
}

int main(){
    printf("Initial counter: %d\n", counter);

    pthread_create(&t2, NULL, increment, (void*)(2));
    pthread_create(&t1, NULL, increment, (void*)(1));
    
    printf("Halfway counter: %d\n", counter);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Final counter: %d\n", counter);
    return 0;
}