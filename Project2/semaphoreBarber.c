#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define NUM_CUSTOMERS 10
#define NUM_CHAIRS 5

sem_t waitingRoom;     // Controls access to chairs
sem_t barberChair;     // Ensures one customer at a time in barber chair
sem_t barberSleep;     // Barber waits here if no customers
sem_t haircutDone;     // Customer waits here for haircut to finish

void* barber(void* arg) {
    while (1) {
        sem_wait(&barberSleep); // Wait for a customer
        sem_wait(&waitingRoom); // Take a waiting customer
        sem_post(&barberChair); // Let them sit in the barber chair

        printf("[Barber] Cutting hair...\n");
        sleep(2); // Simulate haircut

        sem_post(&haircutDone); // Haircut finished
    }
    return NULL;
}

void* customer(void* arg) {
    int id = *(int*)arg;
    sleep(rand() % 3); // Arrive at different times

    if (sem_trywait(&waitingRoom) == 0) {
        printf("[Customer %d] Sitting in waiting room.\n", id);
        sem_post(&barberSleep);    // Wake up barber if sleeping
        sem_wait(&barberChair);    // Wait for barber chair
        printf("[Customer %d] Getting haircut.\n", id);
        sem_wait(&haircutDone);    // Wait for haircut to finish
        sem_post(&waitingRoom);    // Leave waiting room
        printf("[Customer %d] Haircut done. Leaving.\n", id);
    } else {
        printf("[Customer %d] No chairs. Leaving.\n", id);
    }
    free(arg);
    return NULL;
}

int main() {
    pthread_t barberThread;
    pthread_t customerThreads[NUM_CUSTOMERS];

    sem_init(&waitingRoom, 0, NUM_CHAIRS);
    sem_init(&barberChair, 0, 0);
    sem_init(&barberSleep, 0, 0);
    sem_init(&haircutDone, 0, 0);

    pthread_create(&barberThread, NULL, barber, NULL);

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        int* id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&customerThreads[i], NULL, customer, id);
    }

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_join(customerThreads[i], NULL);
    }

    // Optionally cancel barber thread if you want the program to end
    pthread_cancel(barberThread);
    printf("All customers processed. Shop closed.\n");

    return 0;
}
