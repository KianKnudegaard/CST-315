#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "VMmanager.c"

int tlb_hits = 0;
int tlb_misses = 0;
int process_count = 0;
int fifo_queue[NUM_FRAMES];
int fifo_front = 0;
int fifo_rear = 0;

Frame frames[NUM_FRAMES];
Process processes[MAX_PROCESSES];
TLBEntry tlb[TLB_SIZE];

void enqueue_fifo(int frame_index) {
    fifo_queue[fifo_rear] = frame_index;
    fifo_rear = (fifo_rear + 1) % NUM_FRAMES;
}

int dequeue_fifo() {
    int frame_index = fifo_queue[fifo_front];
    fifo_front = (fifo_front + 1) % NUM_FRAMES;
    return frame_index;
}

void initialize() {
    for (int i = 0; i < NUM_FRAMES; i++) {
        frames[i] = (Frame){i, 0, -1, -1};
    }
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = 0;
        tlb[i].use_counter = 0;
    }
    fifo_front = fifo_rear = 0;
}

void initialize_page_table(Process *process) {
    for (int i = 0; i < NUM_PAGES; i++) {
        process->page_table[i] = (PageTableEntry){-1, 0, 0, 1, 1};
    }
}

int create_process() {
    if (process_count >= MAX_PROCESSES) return -1;
    Process *process = &processes[process_count];
    process->process_id = process_count + 1;
    initialize_page_table(process);
    process_count++;
    return process->process_id;
}

int allocate_frame() {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!frames[i].occupied) {
            frames[i].occupied = 1;
            enqueue_fifo(i);
            return i;
        }
    }
    int victim = dequeue_fifo();
    for (int i = 0; i < process_count; i++) {
        Process *p = &processes[i];
        for (int j = 0; j < NUM_PAGES; j++) {
            if (p->page_table[j].frame_number == victim) {
                p->page_table[j].valid = 0;
                p->page_table[j].frame_number = -1;
            }
        }
    }
    enqueue_fifo(victim);
    return victim;
}

void simulate_disk_io() {
    sleep(1);
}

void log_page_fault(int process_id, int page_number, const char *type) {
    printf("Page Fault (%s): Process %d, Page %d\n", type, process_id, page_number);
}

int tlb_lookup(int page_number, int *frame_number) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].page_number == page_number) {
            *frame_number = tlb[i].frame_number;
            tlb[i].use_counter++;
            tlb_hits++;
            return 1;
        }
    }
    tlb_misses++;
    return 0;
}

void tlb_add_entry(int page_number, int frame_number) {
    int lru_index = 0, min_use = tlb[0].use_counter;
    for (int i = 1; i < TLB_SIZE; i++) {
        if (!tlb[i].valid) {
            lru_index = i;
            break;
        }
        if (tlb[i].use_counter < min_use) {
            min_use = tlb[i].use_counter;
            lru_index = i;
        }
    }
    tlb[lru_index] = (TLBEntry){page_number, frame_number, 1, 1};
}

void load_page(Process *process, int page_number, int is_hard_fault) {
    int frame_number = allocate_frame();
    process->page_table[page_number].frame_number = frame_number;
    process->page_table[page_number].valid = 1;
    frames[frame_number] = (Frame){frame_number, 1, process->process_id, page_number};
    if (is_hard_fault) simulate_disk_io();
    tlb_add_entry(page_number, frame_number);
    log_page_fault(process->process_id, page_number, is_hard_fault ? "Hard" : "Soft");
}

void access_memory(Process *process, int page_number, int offset, char mode) {
    int frame_number;
    if (tlb_lookup(page_number, &frame_number)) {
        printf("TLB HIT: Frame %d for Process %d, Page %d\n", frame_number, process->process_id, page_number);
    } else {
        if (!process->page_table[page_number].valid) {
            load_page(process, page_number, 1);
            frame_number = process->page_table[page_number].frame_number;
        } else {
            load_page(process, page_number, 0);
            frame_number = process->page_table[page_number].frame_number;
        }
    }
    if ((mode == 'r' && !process->page_table[page_number].read_permission) ||
        (mode == 'w' && !process->page_table[page_number].write_permission)) {
        printf("Access violation: Process %d, Page %d, Offset %d, Mode %c\n",
               process->process_id, page_number, offset, mode);
        return;
    }
    printf("Accessed memory at Frame %d, Offset %d for Process %d, Mode %c\n",
           frame_number, offset, process->process_id, mode);
}

void free_frames(Process *process) {
    for (int i = 0; i < NUM_PAGES; i++) {
        if (process->page_table[i].valid) {
            int f = process->page_table[i].frame_number;
            frames[f] = (Frame){f, 0, -1, -1};
            process->page_table[i].valid = 0;
        }
    }
}

void print_tlb_state() {
    printf("\nTLB State:\n");
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid) {
            printf("Index %d: Page %d -> Frame %d (Use: %d)\n",
                   i, tlb[i].page_number, tlb[i].frame_number, tlb[i].use_counter);
        }
    }
    printf("TLB Hits: %d, Misses: %d\n\n", tlb_hits, tlb_misses);
}

void print_memory_state() {
    printf("\nMemory State:\n");
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frames[i].occupied)
            printf("Frame %d: Process %d, Page %d\n",
                   i, frames[i].process_id, frames[i].page_number);
        else
            printf("Frame %d: Free\n", i);
    }
    print_tlb_state();
}

void free_process(int vm_pid) {
    if (vm_pid <= 0 || vm_pid > process_count) {
        printf("Invalid VM process ID: %d\n", vm_pid);
        return;
    }
    Process *process = &processes[vm_pid - 1];
    free_frames(process);
}
