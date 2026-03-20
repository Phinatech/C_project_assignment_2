# Multithreading and Synchronization

A multi-threaded prime number counter written in C. It divides the range `[1, 200000]` into equal segments and assigns each segment to a POSIX thread, then uses a mutex to safely accumulate the global total.

---

## Files

| File | Description |
|------|-------------|
| `prime_threads.c` | Main source file |
| `prime_threads` | Compiled executable |

---

## How It Works

1. The range `1` to `200,000` is split into 16 equal segments of `12,500` numbers each.
2. Each thread independently counts primes in its segment using trial division up to `sqrt(n)` — no lock needed during counting.
3. Once a thread finishes, it acquires a `pthread_mutex` to add its local count to the shared `total_primes` and prints the running total.
4. The mutex is locked exactly **once per thread** (not once per prime), minimising contention.
5. The main thread joins all 16 threads before printing the final result.

---

## Compile

```bash
gcc -o prime_threads prime_threads.c -lpthread -lm
```

> `-lm` is required for the `sqrt()` function from `<math.h>`.

---

## Run

```bash
./prime_threads
```

No arguments needed — the range and thread count are fixed via `#define`.

---

## Configuration

Defined at the top of `prime_threads.c`:

| Macro | Value | Description |
|-------|-------|-------------|
| `UPPER_LIMIT` | `200000` | Upper bound of the range to search |
| `NUM_THREADS` | `16` | Number of threads to spawn |
| `SEGMENT_SIZE` | `UPPER_LIMIT / NUM_THREADS` | Numbers assigned per thread (12,500) |

---

## Expected Output

```
Counting primes between 1 and 200000 using 16 threads
Segment size: 12500 numbers per thread

[Thread  0] scanning 1 – 12500
[Thread  1] scanning 12501 – 25000
...
[Thread  0] found 1461 primes in [1, 12500], running total = 1461
[Thread  3] found 1214 primes in [37501, 50000], running total = 2675
...

The synchronized total number of prime numbers between 1 and 200,000 is 17984
```

> Thread output order will vary between runs due to scheduling.

---

## Synchronization Design

| Aspect | Detail |
|--------|--------|
| Threading model | POSIX threads (`pthread`) |
| Shared state | `total_primes` (int) |
| Synchronization | `pthread_mutex_t counter_mutex` |
| Lock granularity | Locked **once per thread** after local counting finishes |
| Primality test | Trial division up to `sqrt(n)`, skips even numbers |

---

## Run with strace via Docker (Linux)

```bash
cat prime_threads.c | docker run --rm -i --cap-add=SYS_PTRACE gcc:latest \
  bash -c "apt-get update -q && apt-get install -y strace -q && \
  cat > /tmp/prime_threads.c && \
  gcc -o /tmp/prime_threads /tmp/prime_threads.c -lpthread -lm && \
  strace -f -e trace=clone,futex,exit_group \
  /tmp/prime_threads 2>&1"
```

Key syscalls to observe:
- `clone` — thread creation (equivalent to `pthread_create`)
- `futex` — mutex lock/unlock operations
- `exit_group` — process exit after all threads join
