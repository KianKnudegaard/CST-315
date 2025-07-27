# Project 5: Preemptive Short-Term Process Scheduler

## 🔧 Overview

This project extends a custom Unix-like shell by integrating a **preemptive thread-based short-term scheduler**. The scheduler simulates process management using `PCB` structures and supports realistic state transitions (`READY`, `RUNNING`, `WAITING`, `TERMINATED`) based on I/O, time slicing, and termination conditions.

Processes can be created interactively or loaded from a batch file. The scheduler operates in both interactive and batch modes, leveraging simulated time slices and asynchronous I/O operations.

---

## 🧠 Features Implemented

- Preemptive round-robin scheduling with priority-based tie-breaking
- Realistic process states and transitions:
  - `RUNNING → WAITING` (I/O request)
  - `WAITING → READY` (I/O complete)
  - `RUNNING → READY` (quantum expiration)
  - `RUNNING → TERMINATED` (completion)
- Separate `ready_queue` and `wait_queue`
- Batch process loading from a file
- Real-time system monitoring
- `procs` command with minimal and detailed views

---

## 🛠️ Scheduler Behavior Details

| Condition                                                        | Description  |
|------------------------------------------------------------------|----------------------------------------------|
| Process requests I/O → RUNNING → WAITING                         |  Triggered randomly, state and queue updated |
| I/O completes → WAITING → READY                                  |  Handled in background I/O thread            |
| Process can be RUNNING again after I/O completion                |  Re-enqueued and evaluated by scheduler      |
| Process termination → removed from system                        |  `free()` is called and memory cleared       |
| Quantum expiration → RUNNING → READY                             |  `should_preempt()` triggers state change    |

---

## 📂 File Structure

