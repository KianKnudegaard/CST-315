// Code by Kian Knudegaard
// Deadlock avoidance simulation using timers for termination

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <stdarg.h>

#define NUM_PROCESSES 5
#define STARVATION_TIMEOUT 5  // seconds

pthread_mutex_t resource_lock = PTHREAD_MUTEX_INITIALIZER;
FILE *log_file;

typedef struct {
    int id;
    time_t start_wait_time;
} ProcessArgs;

void log_activity(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    fflush(log_file);
}

void* process_function(void* args) {
    ProcessArgs* p = (ProcessArgs*)args;
    int id = p->id;
    log_activity("Process %d: Started and attempting to access resource...\n", id);

    time_t start_time = time(NULL);
    p->start_wait_time = start_time;

    bool acquired = false;
    while (!acquired) {
        if (pthread_mutex_trylock(&resource_lock) == 0) {
            log_activity("Process %d: Acquired resource.\n", id);
            sleep(4);  // Simulate work
            pthread_mutex_unlock(&resource_lock);
            log_activity("Process %d: Released resource and completed.\n", id);
            acquired = true;
        } else {
            time_t current_time = time(NULL);
            if (difftime(current_time, p->start_wait_time) >= STARVATION_TIMEOUT) {
                log_activity("Process %d: Starved. Restarting...\n", id);
                p->start_wait_time = time(NULL);  // Reset wait time
                sleep(1); // Simulate restart delay
            } else {
                log_activity("Process %d: Waiting for resource...\n", id);
                sleep(1);  // Yield and retry
            }
        }
    }

    free(p);
    return NULL;
}

int main() {
    pthread_t threads[NUM_PROCESSES];
    log_file = fopen("activity_log.txt", "w");

    if (!log_file) {
        perror("Failed to open log file");
        return 1;
    }

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        ProcessArgs* p = malloc(sizeof(ProcessArgs));
        p->id = i + 1;
        pthread_create(&threads[i], NULL, process_function, p);
    }

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        pthread_join(threads[i], NULL);
    }

    fclose(log_file);
    return 0;
}
