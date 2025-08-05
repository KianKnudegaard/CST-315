# Custom Unix/Linux Shell Project

## Overview

This project is a multi-phase implementation of a custom Unix/Linux shell designed to simulate core operating system concepts. The shell supports basic command interpretation, process management, virtual memory, scheduling, and an internal file system. It was developed in C and emphasizes modular, extendable design.

---

## Features by Part

### **Part 1: Basic Shell**
- Parses and executes user commands.
- Supports foreground and background process execution (`&`).
- Handles input/output redirection (`<`, `>`).
- Executes piped commands (`|`).
- Command chaining with semicolons (`;`).

### **Part 2: Process Table and Management**
- Maintains a process table with minimal and detailed modes.
- Supports `jobs`, `end`, and `exit` commands.
- Implements `memaccess` to simulate memory operations on managed processes.

### **Part 3: Scheduler**
- Custom process scheduler using a queue.
- Supports creation of scheduled processes (`sched_create_process()`).
- Timer-based preemption and CPU usage tracking.
- Uses pthreads to simulate I/O, timer, and scheduler threads.

### **Part 4: Virtual Memory Manager**
- Implements page tables and frame allocation.
- Simulates TLB lookups, page faults, and disk I/O.
- Tracks TLB hit/miss rates.
- Supports `memaccess` for read/write operations.

### **Part 5: Batch Mode**
- Accepts a batch file as input via command-line argument.
- Executes batch commands line by line.
- Falls back to interactive mode when batch input ends.

---

## Part 6: Internal File System (Final Integration)
- Implements a hierarchical in-memory file system.
- Supports:
  - `mkdir`, `rmdir`, `createf`, `deletef`
  - `tree`, `info`, `cd`, `pwd`, `search`, `move`, `copy`, `copydir`, `editfile`, `rename`
- Every file/directory operation updates the internal structure in real-time.
- Separates shell file operations (e.g., `create`, `delete`) from internal file system commands.
- The directory structure acts as a mini database of file descriptors.
- Maintains memory isolation from the virtual memory system (Part 4) to avoid data corruption.

---

## How to Run

```bash
make
./my_shell            # Launch interactive mode
./my_shell batch.txt  # Run commands from a batch file
