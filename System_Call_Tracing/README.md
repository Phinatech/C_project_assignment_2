# System Call Tracing — Pipeline

A process pipeline program written in C that demonstrates core POSIX system calls. It forks two child processes connected by a pipe to replicate the shell command `ps aux | grep root`, and saves the output to a file.

---

## Files

| File | Description |
|------|-------------|
| `pipeline.c` | Main source file |
| `pipeline_output.txt` | Output captured from the last run |

---

## How It Works

The program replicates `ps aux | grep root` using raw system calls:

```
Parent
  │
  ├── pipe()          → creates pipefd[0] (read) and pipefd[1] (write)
  ├── open()          → opens pipeline_output.txt for writing
  │
  ├── fork() ──► Child 1 (ps aux)
  │               dup2(pipefd[1], stdout)   → redirects stdout to pipe write-end
  │               execvp("ps", ...)         → replaces process with ps aux
  │
  ├── fork() ──► Child 2 (grep root)
  │               dup2(pipefd[0], stdin)    → redirects stdin to pipe read-end
  │               dup2(outfd,     stdout)   → redirects stdout to output file
  │               execvp("grep", ...)       → replaces process with grep root
  │
  ├── close(pipefd[0], pipefd[1], outfd)   → parent closes its copies (triggers EOF for grep)
  ├── waitpid(pid1)   → waits for ps to finish
  ├── waitpid(pid2)   → waits for grep to finish
  └── read() + print → displays first 1024 bytes of output file
```

---

## System Calls Used

| System Call | Purpose |
|-------------|---------|
| `pipe()` | Creates the inter-process communication channel |
| `open()` | Opens `pipeline_output.txt` for writing |
| `fork()` | Creates child processes (called twice) |
| `dup2()` | Redirects stdin/stdout to pipe or file |
| `close()` | Closes unused file descriptors |
| `execvp()` | Replaces child process image with `ps` or `grep` |
| `waitpid()` | Parent waits for each child to exit |
| `read()` | Reads captured output back from file |

---

## Compile

```bash
gcc -o pipeline pipeline.c
```

---

## Run

```bash
./pipeline
```

### Expected terminal output:
```
[parent] Waiting for child1 (ps aux),  PID 1234 ...
[parent] child1 exited with status 0
[parent] Waiting for child2 (grep root), PID 1235 ...
[parent] child2 exited with status 0

[parent] === First N bytes from pipeline_output.txt ===

root   1   0.0  0.0  ...  /sbin/launchd
root   337 0.0  0.1  ...  /usr/libexec/logd
...

[parent] Full output saved to: pipeline_output.txt
```

Results are also saved to `pipeline_output.txt`.

---

## Trace with strace via Docker (Linux)

```bash
cat pipeline.c | docker run --rm -i --cap-add=SYS_PTRACE gcc:latest \
  bash -c "apt-get update -q && apt-get install -y strace procps -q && \
  cat > /tmp/pipeline.c && \
  gcc -o /tmp/pipeline /tmp/pipeline.c && \
  strace -f \
  -e trace=pipe,fork,clone,execve,dup2,open,openat,close,read,write,wait4,waitpid \
  -o /tmp/strace_output.txt /tmp/pipeline && \
  cat /tmp/strace_output.txt"
```

### Key syscalls to look for in the trace:

| Syscall | What to observe |
|---------|----------------|
| `pipe2` | Returns `[pipefd[0], pipefd[1]]` — the two pipe ends |
| `openat` | Opens `pipeline_output.txt` with `O_WRONLY\|O_CREAT\|O_TRUNC` |
| `clone` | Two fork() calls — each spawns a child (shown as new PID) |
| `dup2` | Child 1 wires stdout→pipe; Child 2 wires stdin→pipe and stdout→file |
| `close` | Each process closes unneeded fds |
| `execve` | Child 1 calls `ps aux`; Child 2 calls `grep root` |
| `wait4` | Parent blocks until each child exits |
| `read` | Parent reads output file after children finish |

---

## Why the Parent Must Close Its Pipe Copies

After forking, the parent holds open copies of `pipefd[0]` and `pipefd[1]`.
If the parent does **not** close `pipefd[1]`, the write-end of the pipe stays
open even after Child 1 (ps) exits — Child 2 (grep) will then block forever
waiting for more input and never receive EOF.

```c
close(pipefd[0]);   /* parent doesn't read from pipe */
close(pipefd[1]);   /* critical: lets grep see EOF   */
close(outfd);       /* parent doesn't write to file  */
```
