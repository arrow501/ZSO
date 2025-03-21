#include "shop.h"
#include "parameters.h"

int main(){
    for(int i = 0; i < NUM_SIMULATIONS; i++){
        printf("Simulation %d/%d\n", i + 1, NUM_SIMULATIONS);
        zso();
    }
    return 0;
}