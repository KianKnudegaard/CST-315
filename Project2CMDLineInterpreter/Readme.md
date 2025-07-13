# Custom Shell Interpreter — Project 2

## 🧾 Description
This project is a Unix/Linux command-line interpreter (shell), written in C. It supports basic command execution, background jobs, and batch file processing.

---

## ⚙️ Features
- Custom prompt: `my_shell>`
- Executes Unix commands using `fork()` and `execvp()`
- Supports semicolon-separated commands (runs concurrently)
- Handles background jobs using `&`
- Built-in commands:
  - `cd`
  - `exit`
  - `jobs`
  - `end` (terminate background jobs)
- Batch mode: run commands from a file
- Signal handler to exit shell via `SIGUSR1`

---

## 🚀 How to Build

```bash
gcc -o my_shell shellCompiler.c
