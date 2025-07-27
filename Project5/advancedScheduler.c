#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

typedef enum { NEW, READY, RUNNING, WAITING, TERMINATED } ProcessState;

const char *state_str[] = { "NEW", "READY", "RUNNING", "WAITING", "TERMINATED" };

typedef struct time_manager {
    int current_quantum;
    int time_used;
    pthread_mutex_t lock;
} time_manager;

time_manager tm = { .current_quantum = 2, .time_used = 0, .lock = PTHREAD_MUTEX_INITIALIZER };

typedef struct PCB {
    int pid;
    ProcessState state;
    int priority;
    int cpu_time_used;
    int time_limit;
    int io_requested;

    struct PCB *parent;
    struct PCB **children;
    int child_count;

    pthread_t thread;
    struct PCB *next;
} PCB;

typedef struct {
    PCB *head;
    PCB *tail;
    pthread_mutex_t lock;
} Queue;

Queue ready_queue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};
Queue wait_queue  = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER};

int next_pid = 1;

void setup_timer_interrupt(int quantum) {
    pthread_mutex_lock(&tm.lock);
    tm.current_quantum = quantum;
    tm.time_used = 0;
    pthread_mutex_unlock(&tm.lock);
}

void handle_timer_interrupt() {
    pthread_mutex_lock(&tm.lock);
    tm.time_used += 1;
    pthread_mutex_unlock(&tm.lock);
}

bool should_preempt() {
    pthread_mutex_lock(&tm.lock);
    bool preempt = tm.time_used >= tm.current_quantum;
    pthread_mutex_unlock(&tm.lock);
    return preempt;
}

void enqueue(Queue *q, PCB *p) {
    pthread_mutex_lock(&q->lock);
    p->next = NULL;
    if (q->tail) {
        q->tail->next = p;
    } else {
        q->head = p;
    }
    q->tail = p;
    pthread_mutex_unlock(&q->lock);
}

PCB* dequeue(Queue *q) {
    pthread_mutex_lock(&q->lock);

    if (!q->head) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    PCB *prev = NULL;
    PCB *curr = q->head;

    PCB *best_prev = NULL;
    PCB *best_node = curr;
    int best_priority = curr->priority;
    int best_time_limit = curr->time_limit;

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
        if (q->tail == best_node) {
            q->tail = NULL;
        }
    } else if (best_prev) {
        best_prev->next = best_node->next;
        if (q->tail == best_node) {
            q->tail = best_prev;
        }
    }

    best_node->next = NULL;

    pthread_mutex_unlock(&q->lock);
    return best_node;
}

void *io_simulator(void *arg) {
    while (1) {
        sleep(3);
        pthread_mutex_lock(&wait_queue.lock);
        PCB *prev = NULL;
        PCB *curr = wait_queue.head;

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

PCB *create_process(int priority, int time_limit) {
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

void *scheduler_loop(void *arg) {
    while (1) {
        PCB *p = dequeue(&ready_queue);
        if (!p) {
            printf("[Scheduler] No ready processes. Idling...\n");
            sleep(1);
            continue;
        }

        printf("[Scheduler] Scheduling PID %d\n", p->pid);
        setup_timer_interrupt(5);
        p->state = RUNNING;

        for (int i = 0; i < p->time_limit; i++) {
            if (p->state != RUNNING) break;
            printf("[CPU] PID %d running... (used: %d)\n", p->pid, p->cpu_time_used);
            sleep(1);
            p->cpu_time_used++;

            if (rand() % 5 == 0 && !p->io_requested) {
                printf("[CPU] PID %d requesting I/O\n", p->pid);
                p->state = WAITING;
                p->io_requested = 1;
                enqueue(&wait_queue, p);
                goto next;
            }

            if (should_preempt()) {
                printf("[Preempt] PID %d quantum expired, moving back to ready queue\n", p->pid);
                p->state = READY;
                enqueue(&ready_queue, p);
                goto next;
            }

            if (p->cpu_time_used >= p->time_limit) {
                printf("[CPU] PID %d completed execution.\n", p->pid);
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

void load_batch_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Batch file error");
        exit(1);
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
            PCB *p = create_process(priority, time_limit);
            enqueue(&ready_queue, p);
            printf("[Batch] Loaded PID %d with priority %d and time limit %d\n", p->pid, priority, time_limit);
        }
    }

    fclose(file);
}

void *timer_thread_fn(void *arg) {
    while (1) {
        sleep(1);
        handle_timer_interrupt();
    }
    return NULL;
}

void *performance_monitor(void *arg) {
    while (1) {
        sleep(5);
        int ready = 0, waiting = 0, running = 0;
        pthread_mutex_lock(&ready_queue.lock);
        for (PCB *p = ready_queue.head; p; p = p->next) ready++;
        pthread_mutex_unlock(&ready_queue.lock);

        pthread_mutex_lock(&wait_queue.lock);
        for (PCB *p = wait_queue.head; p; p = p->next) waiting++;
        pthread_mutex_unlock(&wait_queue.lock);

        printf("[Monitor] Ready: %d, Waiting: %d\n", ready, waiting);
    }
    return NULL;
}

void display_procs(bool detailed) {
    pthread_mutex_lock(&ready_queue.lock);
    PCB *curr = ready_queue.head;
    printf("[procs] Ready Queue:\n");
    printf(detailed ? "PID STATE PRIORITY USED LIMIT IO CHILDREN\n" : "PID STATE PRIORITY\n");
    while (curr) {
        if (detailed)
            printf("%3d %6s %8d %4d %5d %2d %3d\n", curr->pid, state_str[curr->state], curr->priority, curr->cpu_time_used, curr->time_limit, curr->io_requested, curr->child_count);
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
            printf("%3d %6s %8d %4d %5d %2d %3d\n", curr->pid, state_str[curr->state], curr->priority, curr->cpu_time_used, curr->time_limit, curr->io_requested, curr->child_count);
        else
            printf("%3d %6s %8d\n", curr->pid, state_str[curr->state], curr->priority);
        curr = curr->next;
    }
    pthread_mutex_unlock(&wait_queue.lock);
}
void start_scheduler_threads() {
    pthread_t io_thread, timer_thread, scheduler_thread, monitor_thread;

    pthread_create(&io_thread, NULL, io_simulator, NULL);
    pthread_create(&timer_thread, NULL, timer_thread_fn, NULL);
    pthread_create(&scheduler_thread, NULL, scheduler_loop, NULL);
    pthread_create(&monitor_thread, NULL, performance_monitor, NULL);
}
