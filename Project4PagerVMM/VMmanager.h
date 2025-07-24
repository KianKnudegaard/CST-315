#ifndef VMMANAGER_H
#define VMANAGER_H

#define PAGE_SIZE 4096
#define NUM_PAGES 50
#define NUM_FRAMES 25
#define MAX_PROCESSES 20
#define TLB_SIZE 8

extern int tlb_hits ;
extern int tlb_misses;
extern int process_count;
extern int fifo_queue[NUM_FRAMES];
extern int fifo_front;
extern int fifo_rear;

typedef struct {
    int frame_number;
    int valid;
    int modified;
    int read_permission;
    int write_permission;
} PageTableEntry;

typedef struct {
    int process_id;
    PageTableEntry page_table[NUM_PAGES];
} Process;

typedef struct {
    int frame_number;
    int occupied;
    int process_id;
    int page_number;
} Frame;

typedef struct {
    int page_number;
    int frame_number;
    int valid;
    int use_counter; // for LRU
} TLBEntry;

extern Frame frames[NUM_FRAMES];
extern Process processes[MAX_PROCESSES];
extern TLBEntry tlb[TLB_SIZE];

void initialize();
int create_process();
void access_memory(Process *process, int process_id, int virtual_address, char mode);
void free_process(int vm_pid);
void print_memory_state();

#endif