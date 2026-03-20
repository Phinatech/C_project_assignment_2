# System Calls — File Copy Comparison

Two versions of a file copy program written in C that compare low-level system calls (`read`/`write`) against buffered stdio (`fread`/`fwrite`). Both measure throughput and I/O call counts to highlight the performance difference.

---

## Files

| File | Description |
|------|-------------|
| `syscall.c` | Version 1 — raw POSIX system calls (`read`/`write`) |
| `stidio.c` | Version 2 — buffered stdio (`fread`/`fwrite`) |
| `copy_syscall` | Compiled executable for Version 1 |
| `copy_stdio` | Compiled executable for Version 2 |
| `testfile.bin` | Sample input file for testing |
| `output1.bin` | Output from Version 1 |
| `output2.bin` | Output from Version 2 |

---

## How It Works

### Version 1 — `syscall.c` (raw system calls)
- Opens source with `open()`, destination with `open(O_WRONLY|O_CREAT|O_TRUNC)`
- Reads chunks directly into a buffer with `read()` and writes with `write()`
- Every `read()`/`write()` call crosses the user–kernel boundary
- More system calls = more context switches

### Version 2 — `stidio.c` (buffered stdio)
- Opens files with `fopen()`, reads with `fread()`, writes with `fwrite()`
- The C standard library maintains an internal buffer (`setvbuf` sets it to match the app buffer size)
- Fewer actual `read()`/`write()` kernel calls — the stdio layer batches them
- Reduces context switches compared to Version 1

---

## Compile

```bash
# Version 1
gcc -o copy_syscall syscall.c

# Version 2
gcc -o copy_stdio stidio.c
```

---

## Run

**Step 1 — Create a test file:**
```bash
dd if=/dev/urandom of=testfile.bin bs=1M count=10
```

**Step 2 — Run Version 1 (raw syscalls):**
```bash
./copy_syscall testfile.bin output1.bin
```

**Step 3 — Run Version 2 (stdio buffered):**
```bash
./copy_stdio testfile.bin output2.bin
```

**Optional — Custom buffer size (default is 4096 bytes):**
```bash
./copy_syscall testfile.bin output1.bin 8192
./copy_stdio   testfile.bin output2.bin 8192
```

### Expected output (Version 1):
```
=== copy_syscall (Version 1: read/write) ===
Source file size : 10.00 MiB (10485760 bytes)
Buffer size      : 4096 bytes
read() calls     : 2560
write() calls    : 2560
Total I/O calls  : 5120
Elapsed time     : 12.345 ms
Throughput       : 810.23 MiB/s
```

### Expected output (Version 2):
```
=== copy_stdio (Version 2: fread/fwrite) ===
Source file size : 10.00 MiB (10485760 bytes)
Buffer size      : 4096 bytes
fread() calls    : 2560
fwrite() calls   : 2560
Total I/O calls  : 5120
Elapsed time     : 8.123 ms
Throughput       : 1231.45 MiB/s
```

> Version 2 is typically faster because the stdio layer reduces actual kernel crossings.

---

## Trace with strace via Docker (Linux)

### Version 1:
```bash
cat syscall.c | docker run --rm -i --cap-add=SYS_PTRACE gcc:latest \
  bash -c "apt-get update -q && apt-get install -y strace -q && \
  cat > /tmp/syscall.c && \
  gcc -o /tmp/copy_syscall /tmp/syscall.c && \
  dd if=/dev/urandom of=/tmp/test.bin bs=1M count=5 2>/dev/null && \
  strace -e trace=open,openat,read,write,close,fstat \
  -o /tmp/strace_v1.txt /tmp/copy_syscall /tmp/test.bin /tmp/out1.bin && \
  cat /tmp/strace_v1.txt"
```

### Version 2:
```bash
cat stidio.c | docker run --rm -i --cap-add=SYS_PTRACE gcc:latest \
  bash -c "apt-get update -q && apt-get install -y strace -q && \
  cat > /tmp/stidio.c && \
  gcc -o /tmp/copy_stdio /tmp/stidio.c && \
  dd if=/dev/urandom of=/tmp/test.bin bs=1M count=5 2>/dev/null && \
  strace -e trace=open,openat,read,write,close,fstat \
  -o /tmp/strace_v2.txt /tmp/copy_stdio /tmp/test.bin /tmp/out2.bin && \
  cat /tmp/strace_v2.txt"
```

### Key difference to observe in the trace:

| Syscall | Version 1 (`syscall.c`) | Version 2 (`stidio.c`) |
|---------|------------------------|------------------------|
| `read()` calls | Many (one per buffer chunk) | Few (stdio batches internally) |
| `write()` calls | Many (one per buffer chunk) | Few (stdio batches internally) |
| `openat()` | Direct `open()` flags visible | Wrapped by `fopen()` |

---

## Key Difference Summary

| Aspect | Version 1 (`syscall.c`) | Version 2 (`stidio.c`) |
|--------|------------------------|------------------------|
| API | `open`, `read`, `write`, `close` | `fopen`, `fread`, `fwrite`, `fclose` |
| Buffering | Application buffer only | Application + stdio internal buffer |
| Kernel crossings | One per `read`/`write` call | Reduced by stdio batching |
| Syscall count | Higher | Lower |
| Typical speed | Baseline | Faster |
