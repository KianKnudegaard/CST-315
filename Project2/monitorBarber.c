#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_CUSTOMERS 10
#define NUM_CHAIRS 5

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t customerReady = PTHREAD_COND_INITIALIZER;
pthread_cond_t barberReady = PTHREAD_COND_INITIALIZER;

int waiting = 0;

void* barber(void* arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        while (waiting == 0) {
            printf("[Barber] Sleeping. No customers.\n");
            pthread_cond_wait(&customerReady, &lock);
        }

        waiting--;
        pthread_cond_signal(&barberReady);
        pthread_mutex_unlock(&lock);

        printf("[Barber] Cutting hair...\n");
        sleep(2); // Simulate haircut
        printf("[Barber] Haircut done.\n");
    }
    return NULL;
}

void* customer(void* arg) {
    int id = *(int*)arg;
    sleep(rand() % 3);

    pthread_mutex_lock(&lock);
    if (waiting < NUM_CHAIRS) {
        waiting++;
        printf("[Customer %d] Waiting. (%d in queue)\n", id, waiting);
        pthread_cond_signal(&customerReady);      // Wake barber
        pthread_cond_wait(&barberReady, &lock);   // Wait for barber chair
        pthread_mutex_unlock(&lock);

        printf("[Customer %d] Getting haircut.\n", id);
        // Simulate haircut time (barber does this)
        printf("[Customer %d] Haircut done. Leaving.\n", id);
    } else {
        pthread_mutex_unlock(&lock);
        printf("[Customer %d] No chairs. Leaving.\n", id);
    }

    free(arg);
    return NULL;
}

int main() {
    pthread_t barberThread;
    pthread_t customerThreads[NUM_CUSTOMERS];

    pthread_create(&barberThread, NULL, barber, NULL);

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        int* id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&customerThreads[i], NULL, customer, id);
    }

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_join(customerThreads[i], NULL);
    }

    pthread_cancel(barberThread);
    printf("All customers processed. Shop closed.\n");
    return 0;
}
