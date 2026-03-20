#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
 
#define DEFAULT_BUF_SIZE 4096
 
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
    FILE *src = fopen(argv[1], "rb");
    if (!src) { perror("fopen src"); return 1; }
 
    /* ── get file size ────────────────────────────────────────────────── */
    struct stat st;
    if (stat(argv[1], &st) == -1) { perror("stat"); return 1; }
    off_t file_size = st.st_size;
 
    /* ── open destination ─────────────────────────────────────────────── */
    FILE *dst = fopen(argv[2], "wb");
    if (!dst) { perror("fopen dst"); return 1; }
 
    /*
     * setvbuf: configure the stdio layer's internal buffer to the same
     * size as our fread/fwrite buffer.  This gives the stdio layer enough
     * room to batch kernel calls.  Without this, glibc defaults to 8 KiB.
     */
    if (setvbuf(src, NULL, _IOFBF, buf_size) != 0)
        fprintf(stderr, "Warning: setvbuf src failed\n");
    if (setvbuf(dst, NULL, _IOFBF, buf_size) != 0)
        fprintf(stderr, "Warning: setvbuf dst failed\n");
 
    /* ── allocate buffer ──────────────────────────────────────────────── */
    char *buf = malloc(buf_size);
    if (!buf) { perror("malloc"); return 1; }
 
    /* ── timed copy loop ──────────────────────────────────────────────── */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
 
    size_t      n_read;
    long long   total_bytes  = 0;
    long long   fread_calls  = 0;
    long long   fwrite_calls = 0;
 
    while ((n_read = fread(buf, 1, buf_size, src)) > 0) {
        fread_calls++;
        size_t written = 0;
        while (written < n_read) {
            size_t w = fwrite(buf + written, 1, n_read - written, dst);
            if (w == 0 && ferror(dst)) {
                perror("fwrite"); free(buf); return 1;
            }
            fwrite_calls++;
            written += w;
        }
        total_bytes += (long long)n_read;
    }
 
    if (ferror(src)) { perror("fread"); free(buf); return 1; }
 
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    free(buf);
 
    /* ── close ────────────────────────────────────────────────────────── */
    fclose(src);
    fclose(dst);
 
    double ms   = elapsed_ms(&t_start, &t_end);
    double mb   = (double)total_bytes / (1024.0 * 1024.0);
    double mbps = mb / (ms / 1000.0);
 
    printf("=== copy_stdio (Version 2: fread/fwrite) ===\n");
    printf("Source file size : %.2f MiB (%lld bytes)\n", mb, (long long)file_size);
    printf("Buffer size      : %zu bytes\n", buf_size);
    printf("fread() calls    : %lld\n",  fread_calls);
    printf("fwrite() calls   : %lld\n",  fwrite_calls);
    printf("Total I/O calls  : %lld\n",  fread_calls + fwrite_calls);
    printf("Elapsed time     : %.3f ms\n", ms);
    printf("Throughput       : %.2f MiB/s\n", mbps);
 
    return 0;
}
 