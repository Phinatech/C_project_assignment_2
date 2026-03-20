# Concurrent File Processing

A multi-threaded keyword search program written in C. It spawns POSIX threads to search multiple files simultaneously and reports the number of occurrences of a keyword in each file.

---

## Files

| File | Description |
|------|-------------|
| `search.c` | Main source file |
| `file1.txt` | Sample input file 1 |
| `file2.txt` | Sample input file 2 |
| `file3.txt` | Sample input file 3 |
| `output.txt` | Search results output |

---

## How It Works

1. The program accepts a keyword, an output file, one or more input files, and a thread count.
2. It spawns up to `num_threads` POSIX threads, each assigned one file to search.
3. Files are processed in batches — each thread reads its file into memory and counts keyword occurrences locally (no lock needed for reading).
4. A `pthread_mutex` protects the shared output file and global counter when threads write their results.
5. Timing is measured with `clock_gettime(CLOCK_MONOTONIC)`.

---

## Compile

```bash
gcc -o search search.c -lpthread
```

---

## Usage

```bash
./search <keyword> <output.txt> <file1> [file2 ...] <num_threads>

```

### Arguments

| Argument | Description |
|----------|-------------|
| `keyword` | The word/substring to search for |
| `output.txt` | File where results are written |
| `file1 ...` | One or more input files to search |
| `num_threads` | Number of threads to use |

> Note: if `num_threads` exceeds the number of input files, it is automatically capped to the file count.

---

## Example

```bash
./search the output.txt file1.txt file2.txt file3.txt 3
```

### Terminal output:
```
Searching for "the" across 3 file(s) using 3 thread(s)

[Thread  0] "the" found 3 time(s) in file1.txt
[Thread  1] "the" found 3 time(s) in file2.txt
[Thread  2] "the" found 3 time(s) in file3.txt

------------------------------------------
Total occurrences of "the" : 9
Elapsed time               : 1.304 ms
Results written to         : output.txt
------------------------------------------
```

### output.txt:
```
=== Keyword Search Results ===
Keyword    : "the"
Threads    : 3
Files      : 3

Thread  0 | File: file1.txt  | Occurrences: 3
Thread  1 | File: file2.txt  | Occurrences: 3
Thread  2 | File: file3.txt  | Occurrences: 3

-------------------------------
Total occurrences : 9
Elapsed time      : 1.304 ms
-------------------------------
```

---

## Run with strace via Docker (Linux)

```bash
cat search.c | docker run --rm -i --cap-add=SYS_PTRACE gcc:latest \
  bash -c "apt-get update -q && apt-get install -y strace -q && \
  cat > /tmp/search.c && \
  gcc -o /tmp/search /tmp/search.c -lpthread && \
  echo 'the quick brown fox' > /tmp/f1.txt && \
  echo 'searching the keyword' > /tmp/f2.txt && \
  echo 'the thread reads the file' > /tmp/f3.txt && \
  strace -f -e trace=open,openat,read,write,close,clone,futex \
  /tmp/search the /tmp/out.txt /tmp/f1.txt /tmp/f2.txt /tmp/f3.txt 3 2>&1"
```

---

## Concurrency Design

| Aspect | Detail |
|--------|--------|
| Threading model | POSIX threads (`pthread`) |
| Synchronization | `pthread_mutex_t` on shared output file and `total_count` |
| Batch processing | Threads join after each batch before the next starts |
| Local counting | Each thread counts independently before locking |
