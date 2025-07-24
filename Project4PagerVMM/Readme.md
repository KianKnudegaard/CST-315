# Project 4: Pager – A Virtual Memory Manager

## Overview

This project extends a Unix-like shell by integrating a virtual memory management (VMM) system. It simulates paging, page faults, frame allocation, and TLB behavior for multiple processes.

---

## Key Features

- **Memory Model**: 50 virtual pages/process, 25 physical frames, 4KB pages.
- **Page Tables**: Each process has a page table tracking validity, permissions, and frame mapping.
- **Demand Paging**: Pages are loaded on access; hard faults simulate disk I/O with `sleep(1)`.
- **FIFO Replacement**: When memory is full, the oldest page is evicted.
- **TLB Cache**: A small 8-entry cache with LRU-like policy tracks page-frame translations.
- **Access Control**: Read/write permissions enforced with access violation handling.
- **Process Cleanup**: When processes terminate, all memory and TLB entries are released.

---

## How to Run

### Requirements

- GCC (Linux or WSL recommended)
- Makefile to compile (or compile manually)

### Compile and Execute

```bash
gcc shellCompiler.c VMmanager.c -o shell -Wall
./shell
