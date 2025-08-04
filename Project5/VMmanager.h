#ifndef VMMANAGER_H
#define VMMANAGER_H

#define PAGE_SIZE 4096
#define NUM_PAGES 50
#define NUM_FRAMES 25
#define MAX_PROCESSES 20
#define TLB_SIZE 8

extern int tlb_hits;
extern int tlb_misses;
extern int process_count;
extern int fifo_queue[];
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

void enqueue_fifo(int frame_index);
int dequeue_fifo();
void initialize();
void initialize_page_table(Process *process);
int create_process();
int allocate_frame();
void simulate_disk_io();
void log_page_fault(int, int, const char*);
int tlb_lookup(int, int*);
void tlb_add_entry(int, int);
void load_page(Process*, int, int);
void free_frames(Process *process);
void print_tlb_state();
void access_memory(Process*, int, int, char);
void free_process(int vm_pid);
void print_memory_state();

#endif
void access_memory(Process *process, int process_id, int virtual_address, char mode);
void free_process(int vm_pid);
void print_memory_state();

#endif
