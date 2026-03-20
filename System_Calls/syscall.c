#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
 
#define DEFAULT_BUF_SIZE 4096   /* one 4 KiB page — matches kernel default */
 
static double elapsed_ms(struct timespec *start, struct timespec *end)
{
    return (end->tv_sec  - start->tv_sec)  * 1000.0
         + (end->tv_nsec - start->tv_nsec) / 1.0e6;
}
 
int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <src> <dst> [buf_bytes]\n", argv[0]);
        return 1;
    }
 
    size_t buf_size = (argc >= 4) ? (size_t)atol(argv[3]) : DEFAULT_BUF_SIZE;
    if (buf_size == 0) { fprintf(stderr, "buf_size must be > 0\n"); return 1; }
 
    /* ── open source ──────────────────────────────────────────────────── */
    int src_fd = open(argv[1], O_RDONLY);
    if (src_fd == -1) { perror("open src"); return 1; }
 
    /* ── get file size ────────────────────────────────────────────────── */
    struct stat st;
    if (fstat(src_fd, &st) == -1) { perror("fstat"); return 1; }
    off_t file_size = st.st_size;
 
    /* ── open destination ─────────────────────────────────────────────── */
    int dst_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd == -1) { perror("open dst"); return 1; }
 
    /* ── allocate buffer ──────────────────────────────────────────────── */
    char *buf = malloc(buf_size);
    if (!buf) { perror("malloc"); return 1; }
 
    /* ── timed copy loop ──────────────────────────────────────────────── */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
 
    ssize_t     n_read;
    long long   total_bytes  = 0;
    long long   read_calls   = 0;
    long long   write_calls  = 0;
 
    while ((n_read = read(src_fd, buf, buf_size)) > 0) {
        read_calls++;
        ssize_t written = 0;
        while (written < n_read) {              /* handle partial writes */
            ssize_t w = write(dst_fd, buf + written, (size_t)(n_read - written));
            if (w == -1) { perror("write"); free(buf); return 1; }
            write_calls++;
            written += w;
        }
        total_bytes += n_read;
    }
 
    if (n_read == -1) { perror("read"); free(buf); return 1; }
 
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    free(buf);
 
    /* ── close ────────────────────────────────────────────────────────── */
    close(src_fd);
    close(dst_fd);
 
    double ms   = elapsed_ms(&t_start, &t_end);
    double mb   = (double)total_bytes / (1024.0 * 1024.0);
    double mbps = mb / (ms / 1000.0);
 
    printf("=== copy_syscall (Version 1: read/write) ===\n");
    printf("Source file size : %.2f MiB (%lld bytes)\n", mb, (long long)file_size);
    printf("Buffer size      : %zu bytes\n", buf_size);
    printf("read() calls     : %lld\n",  read_calls);
    printf("write() calls    : %lld\n",  write_calls);
    printf("Total I/O calls  : %lld\n",  read_calls + write_calls);
    printf("Elapsed time     : %.3f ms\n", ms);
    printf("Throughput       : %.2f MiB/s\n", mbps);
 
    return 0;
}