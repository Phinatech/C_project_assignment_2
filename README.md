# C Programming Assignment 2 — Year 3 Trimester 1

A collection of C programs exploring operating system concepts including system calls, process management, inter-process communication, and multithreading with synchronization.

---

## Project Structure

```
C_project_assignment_2/
├── System_Calls/                        # File copy using raw vs buffered I/O
│   ├── syscall.c                        # Version 1: read()/write() system calls
│   ├── stidio.c                         # Version 2: fread()/fwrite() stdio
│   └── README.md
│
├── System_Call_Tracing/                 # Process pipeline with system call tracing
│   ├── pipeline.c                       # ps aux | grep root via fork/pipe/execvp
│   ├── pipeline_output.txt              # Captured output from last run
│   └── README.md
│
├── Multithreading_and_Synchronization/  # Parallel prime number counter
│   ├── prime_threads.c                  # 16 threads, mutex-protected total
│   └── README.md
│
└── Concurrent_File_Processing/          # Multi-threaded keyword search
    ├── search.c                         # Searches multiple files in parallel
    ├── file1.txt / file2.txt / file3.txt
    ├── output.txt                       # Search results from last run
    └── README.md
```

---

## Projects Overview

### 1. System Calls — File Copy Comparison
**Directory:** [System_Calls/](System_Calls/)

Compares two methods of copying a file:

| Version | File | API Used | Description |
|---------|------|----------|-------------|
| V1 | `syscall.c` | `open`, `read`, `write`, `close` | Raw POSIX system calls |
| V2 | `stidio.c` | `fopen`, `fread`, `fwrite`, `fclose` | Buffered C stdio library |

Measures and reports read/write call counts, elapsed time, and throughput for each approach.

**Quick start:**
```bash
cd System_Calls
gcc -o copy_syscall syscall.c
gcc -o copy_stdio stidio.c
dd if=/dev/urandom of=testfile.bin bs=1M count=10
./copy_syscall testfile.bin output1.bin
./copy_stdio   testfile.bin output2.bin
```

---

### 2. System Call Tracing — Pipeline
**Directory:** [System_Call_Tracing/](System_Call_Tracing/)

Implements the shell pipeline `ps aux | grep root` using raw system calls, demonstrating how a shell works internally.

**Key system calls used:** `pipe`, `fork`, `dup2`, `execvp`, `waitpid`, `open`, `read`, `close`

**Quick start:**
```bash
cd System_Call_Tracing
gcc -o pipeline pipeline.c
./pipeline
```

**Run with strace via Docker:**
```bash
cat pipeline.c | docker run --rm -i --cap-add=SYS_PTRACE gcc:latest \
  bash -c "apt-get update -q && apt-get install -y strace procps -q && \
  cat > /tmp/pipeline.c && gcc -o /tmp/pipeline /tmp/pipeline.c && \
  strace -f -e trace=pipe,fork,clone,execve,dup2,open,openat,close,read,write,wait4,waitpid \
  /tmp/pipeline 2>&1"
```

---

### 3. Multithreading and Synchronization — Prime Counter
**Directory:** [Multithreading_and_Synchronization/](Multithreading_and_Synchronization/)

Counts all prime numbers between 1 and 200,000 using 16 POSIX threads. Each thread independently scans its segment and uses a mutex to safely update the shared total.

**Key concepts:** `pthread_create`, `pthread_join`, `pthread_mutex_lock`, local counting to reduce contention

**Quick start:**
```bash
cd Multithreading_and_Synchronization
gcc -o prime_threads prime_threads.c -lpthread -lm
./prime_threads
```

---

### 4. Concurrent File Processing — Keyword Search
**Directory:** [Concurrent_File_Processing/](Concurrent_File_Processing/)

Searches multiple text files simultaneously for a keyword using POSIX threads. Each thread handles one file, counts occurrences locally, then writes results to a shared output file protected by a mutex.

**Key concepts:** `pthread_create`, `pthread_join`, `pthread_mutex_t`, batch thread processing

**Quick start:**
```bash
cd Concurrent_File_Processing
gcc -o search search.c -lpthread
./search <keyword> output.txt file1.txt file2.txt file3.txt 3
```

**Example:**
```bash
./search the output.txt file1.txt file2.txt file3.txt 3
```

---

## Prerequisites

| Tool | Required For |
|------|-------------|
| `gcc` | Compiling all programs |
| `-lpthread` | Projects 3 and 4 (threading) |
| `-lm` | Project 3 (`sqrt()` in prime check) |
| `Docker Desktop` | Running strace on macOS |

**Install GCC on macOS:**
```bash
xcode-select --install
```

---

## Running strace on macOS

`strace` is a Linux-only tool. On macOS, use Docker to run it:

```bash
# Make sure Docker Desktop is running, then use:
cat <source_file>.c | docker run --rm -i --cap-add=SYS_PTRACE gcc:latest \
  bash -c "apt-get update -q && apt-get install -y strace -q && \
  cat > /tmp/prog.c && gcc -o /tmp/prog /tmp/prog.c && \
  strace -f /tmp/prog 2>&1"
```

Each project's README contains the exact Docker strace command for that program.

---

## Concepts Covered

| Concept | Project |
|---------|---------|
| POSIX system calls (`read`, `write`, `open`) | System Calls |
| Buffered I/O vs raw I/O | System Calls |
| `fork`, `exec`, `pipe`, `dup2` | System Call Tracing |
| Inter-process communication (IPC) | System Call Tracing |
| POSIX threads (`pthread`) | Multithreading, Concurrent File Processing |
| Mutex synchronization | Multithreading, Concurrent File Processing |
| System call tracing with `strace` | All projects |
