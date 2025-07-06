// Kian Knudegaard
// CST-315: Operating Systems Lecture and Lab
// This solution works through a buffer;
// The producer "creates" numbers, (a product)
// and adds them to the buffer for the consumer
// to "consume" the numbers and print them out.
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

int theProduct = 0;
int buffer;
int hasData = 0;

int produce() {
    return theProduct++;
}
void consume(int i) {
    printf("%i", i);
}

void put(input) {
    while (hasData = 1) {
        sleep(1);
    }
    buffer = input;
    hasData = 1;
}
int get() {
    while (hasData != 1) {
        sleep(1);
    }
    hasData = 0;
    return buffer;
}

void* producer(void* input) {
    for (int i = 0; i < 5; i++) {
        int item = produce();
        put(item);
        printf("Produced: %d\n", item);
    }
    return NULL;
}
void* consumer(void* arg) {
    for (int i = 0; i < 5; i++) {
        int item = get();
        consume(item);
        sleep(1);
    }
}

int main() {

    pthread_t prodThread, consThread;

    pthread_create(&prodThread, NULL, producer, NULL);
    pthread_create(&consThread, NULL, consumer, NULL);
    pthread_join(prodThread, NULL);
    pthread_join(consThread, NULL);

    return 0;
}
