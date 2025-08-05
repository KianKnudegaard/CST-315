#ifndef ADVANCEDSCHEDULER_H
#define ADVANCEDSCHEDULER_H

#include <stdbool.h>

extern const char *state_str[];
typedef enum { NEW, READY, RUNNING, WAITING, TERMINATED } ProcessState;

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

    char **args;
    char *input_file;
    char *output_file;
    int background;
} PCB;

typedef struct {
    PCB *head;
    PCB *tail;
    pthread_mutex_t lock;
} Queue;

typedef struct {
    int current_quantum;
    int time_used;
    pthread_mutex_t lock;
} TimeManager;

extern Queue ready_queue;
extern Queue wait_queue;
extern TimeManager tm;

void setup_timer_interrupt(int);

void handle_timer_interrupt();

bool should_preempt();

void enqueue(Queue *, PCB *);

PCB* dequeue(Queue *);

PCB *sched_create_process(int, int);

void *scheduler_loop(void *);

void *io_simulator(void *arg);

void *performance_monitor(void *);

void *timer_thread_fn(void *);

void load_batch_file(const char *);

void display_procs(bool);

void start_scheduler_threads();

#endif
