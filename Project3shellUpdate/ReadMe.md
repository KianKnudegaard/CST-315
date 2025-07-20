# Project 3: Improved Unix/Linux Command Line Interpreter

## Overview
This custom shell builds upon previous foundations to introduce critical features found in modern Unix/Linux terminals. Inspired by Ubuntu's Bash behavior, this shell now supports input/output redirection and piping between commands.

## 🔥 New Features in Project 3

### 1. Input/Output Redirection
You can redirect output or input using `>` and `<`, just like in Bash.

Examples:
```bash
ls > files.txt
cat < input.txt
```

### 2. Command Piping (`|`)
The shell now supports piping between commands using the `|` operator, allowing the output of one command to become the input of another. This mirrors one of the most powerful features in Unix/Linux environments like Ubuntu. Piping makes it possible to chain together simple commands to perform complex tasks efficiently.

Examples:
```bash
ps aux | grep bash
cat file.txt | sort | uniq
```
