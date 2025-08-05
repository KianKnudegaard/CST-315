#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include "advancedScheduler.h"

// Global instances
Queue ready_queue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
Queue wait_queue  = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
TimeManager tm = { .current_quantum = 2, .time_used = 0, .lock = PTHREAD_MUTEX_INITIALIZER };
const char *state_str[] = { "NEW", "READY", "RUNNING", "WAITING", "TERMINATED" };
int next_pid = 1;

// Timer management
void setup_timer_interrupt(int quantum) {
    pthread_mutex_lock(&tm.lock);
    tm.current_quantum = quantum;
    tm.time_used = 0;
    pthread_mutex_unlock(&tm.lock);
}

void handle_timer_interrupt() {
    pthread_mutex_lock(&tm.lock);
    tm.time_used++;
    pthread_mutex_unlock(&tm.lock);
}

bool should_preempt() {
    pthread_mutex_lock(&tm.lock);
    bool preempt = tm.time_used >= tm.current_quantum;
    pthread_mutex_unlock(&tm.lock);
    return preempt;
}

// Queue utilities
void enqueue(Queue *q, PCB *p) {
    pthread_mutex_lock(&q->lock);
    p->next = NULL;
    if (q->tail) q->tail->next = p;
    else q->head = p;
    q->tail = p;
    pthread_mutex_unlock(&q->lock);
}

PCB* dequeue(Queue *q) {
    pthread_mutex_lock(&q->lock);
    if (!q->head) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    PCB *prev = NULL, *curr = q->head;
    PCB *best_node = curr, *best_prev = NULL;
    int best_priority = curr->priority, best_time_limit = curr->time_limit;

    while (curr) {
        if (curr->priority < best_priority ||
           (curr->priority == best_priority && curr->time_limit < best_time_limit)) {
            best_priority = curr->priority;
            best_time_limit = curr->time_limit;
            best_node = curr;
            best_prev = prev;
        }
        prev = curr;
        curr = curr->next;
    }

    if (best_node == q->head) {
        q->head = best_node->next;
        if (q->tail == best_node) q->tail = NULL;
    } else if (best_prev) {
        best_prev->next = best_node->next;
        if (q->tail == best_node) q->tail = best_prev;
    }

    best_node->next = NULL;
    pthread_mutex_unlock(&q->lock);
    return best_node;
}

// Process creation
PCB *sched_create_process(int priority, int time_limit) {
    PCB *p = malloc(sizeof(PCB));
    p->pid = next_pid++;
    p->state = READY;
    p->priority = priority;
    p->cpu_time_used = 0;
    p->time_limit = time_limit;
    p->io_requested = 0;
    p->parent = NULL;
    p->children = NULL;
    p->child_count = 0;
    p->next = NULL;
    return p;
}

// Scheduler
void *scheduler_loop(void *arg) {
    while (1) {
        PCB *p = dequeue(&ready_queue);
        static int idle_printed = 0;
        if (!p) {
            // Only print idle once unless something changes
            if (!idle_printed) {
                fprintf(stderr, "[Scheduler] No ready processes. Idling...\nmy_shell v\n");
                idle_printed = 1;
            }
            sleep(1);
            continue;
        }
        idle_printed = 0;

        // Reset idle flag
        fprintf(stderr, "[Scheduler] Scheduling PID %d\n", p->pid);
        setup_timer_interrupt(5);
        p->state = RUNNING;

        for (int i = 0; i < p->time_limit; i++) {
            if (p->state != RUNNING) break;

            fprintf(stderr, "[CPU] PID %d running... (used: %d)\n", p->pid, p->cpu_time_used);
            sleep(1);
            p->cpu_time_used++;

            if (rand() % 5 == 0 && !p->io_requested) {
                fprintf(stderr, "[CPU] PID %d requesting I/O\n", p->pid);
                p->state = WAITING;
                p->io_requested = 1;
                enqueue(&wait_queue, p);
                goto next;
            }

            if (should_preempt()) {
                fprintf(stderr, "[Preempt] PID %d quantum expired, moving back to ready queue\n", p->pid);
                p->state = READY;
                enqueue(&ready_queue, p);
                goto next;
            }

            if (p->cpu_time_used >= p->time_limit) {
                fprintf(stderr, "[CPU] PID %d completed execution.\n", p->pid);
                p->state = TERMINATED;
                free(p);
                goto next;
            }
        }

        next:
            continue;
    }
    return NULL;
}


// I/O simulation
void *io_simulator(void *arg) {
    while (1) {
        sleep(3);
        pthread_mutex_lock(&wait_queue.lock);
        PCB *prev = NULL, *curr = wait_queue.head;

        while (curr) {
            PCB *next = curr->next;
            if (curr->io_requested) {
                printf("[IO] Completing I/O for PID %d\n", curr->pid);
                curr->state = READY;
                curr->io_requested = 0;

                if (prev) prev->next = curr->next;
                else wait_queue.head = curr->next;
                if (wait_queue.tail == curr) wait_queue.tail = prev;

                curr->next = NULL;
                enqueue(&ready_queue, curr);
            } else {
                prev = curr;
            }
            curr = next;
        }
        pthread_mutex_unlock(&wait_queue.lock);
    }
    return NULL;
}

// Performance monitoring
void *performance_monitor(void *arg) {
    int last_ready = -1, last_waiting = -1;

    while (1) {
        sleep(1); // Lower frequency or change to event-driven if needed
        int ready = 0, waiting = 0;

        pthread_mutex_lock(&ready_queue.lock);
        for (PCB *p = ready_queue.head; p; p = p->next) ready++;
        pthread_mutex_unlock(&ready_queue.lock);

        pthread_mutex_lock(&wait_queue.lock);
        for (PCB *p = wait_queue.head; p; p = p->next) waiting++;
        pthread_mutex_unlock(&wait_queue.lock);

        if (ready != last_ready || waiting != last_waiting) {
            printf("[Monitor] Ready: %d, Waiting: %d\n", ready, waiting);
            if (!ready && !waiting) { printf("my_shell v\n"); fflush(stdout); }
            last_ready = ready;
            last_waiting = waiting;
        }
    }
    return NULL;
}

// Timer thread
void *timer_thread_fn(void *arg) {
    while (1) {
        sleep(1);
        handle_timer_interrupt();
    }
    return NULL;
}

// Batch file loader
void load_batch_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Batch file error");
        exit(EXIT_FAILURE);
    }

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || strlen(line) < 2) continue;

        if (strncmp(line, "SLEEP", 5) == 0) {
            int seconds = atoi(&line[6]);
            printf("[Batch] Sleeping for %d seconds before next process\n", seconds);
            sleep(seconds);
            continue;
        }

        int priority, time_limit;
        if (sscanf(line, "%d %d", &priority, &time_limit) == 2) {
            PCB *p = sched_create_process(priority, time_limit);
            enqueue(&ready_queue, p);
            printf("[Batch] Loaded PID %d with priority %d and time limit %d\n",
                   p->pid, priority, time_limit);
        }
    }

    fclose(file);
}

// Process display
void display_procs(bool detailed) {
    pthread_mutex_lock(&ready_queue.lock);
    PCB *curr = ready_queue.head;
    printf("[procs] Ready Queue:\n");
    printf(detailed ? "PID STATE PRIORITY USED LIMIT IO CHILDREN\n" : "PID STATE PRIORITY\n");
    while (curr) {
        if (detailed)
            printf("%3d %6s %8d %4d %5d %2d %3d\n",
                   curr->pid, state_str[curr->state], curr->priority,
                   curr->cpu_time_used, curr->time_limit, curr->io_requested, curr->child_count);
        else
            printf("%3d %6s %8d\n", curr->pid, state_str[curr->state], curr->priority);
        curr = curr->next;
    }
    pthread_mutex_unlock(&ready_queue.lock);

    pthread_mutex_lock(&wait_queue.lock);
    curr = wait_queue.head;
    printf("[procs] Wait Queue:\n");
    while (curr) {
        if (detailed)
            printf("%3d %6s %8d %4d %5d %2d %3d\n",
                   curr->pid, state_str[curr->state], curr->priority,
                   curr->cpu_time_used, curr->time_limit, curr->io_requested, curr->child_count);
        else
            printf("%3d %6s %8d\n", curr->pid, state_str[curr->state], curr->priority);
        curr = curr->next;
    }
    pthread_mutex_unlock(&wait_queue.lock);
}

// Thread launcher
void start_scheduler_threads() {
    pthread_t io_thread;
    pthread_t timer_thread;
    pthread_t scheduler_thread;
    pthread_t monitor_thread;

    pthread_create(&io_thread, NULL, io_simulator, NULL);
    pthread_create(&timer_thread, NULL, timer_thread_fn, NULL);
    pthread_create(&scheduler_thread, NULL, scheduler_loop, NULL);
    pthread_create(&monitor_thread, NULL, performance_monitor, NULL);
}

