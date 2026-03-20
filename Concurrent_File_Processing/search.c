#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
 
/* ── Shared state ────────────────────────────────────────────────────────── */
static pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE           *output_fp    = NULL;
static int             total_count  = 0;   /* global occurrence tally */
 
/* ── Thread argument ─────────────────────────────────────────────────────── */
typedef struct {
    int         thread_id;
    const char *keyword;
    const char *filename;
} thread_args_t;
 
/* ── Count occurrences of keyword inside text (substring search) ─────────── */
static int count_occurrences(const char *text, const char *keyword)
{
    int   count   = 0;
    int   klen    = (int)strlen(keyword);
    const char *p = text;
 
    if (klen == 0) return 0;
 
    while ((p = strstr(p, keyword)) != NULL) {
        count++;
        p += klen;   /* advance past this match to avoid infinite loop */
    }
    return count;
}
 
/* ── Thread worker ───────────────────────────────────────────────────────── */
static void *search_file(void *arg)
{
    thread_args_t *a = (thread_args_t *)arg;
 
    /* ── Open and read the entire file into a buffer ── */
    FILE *fp = fopen(a->filename, "r");
    if (!fp) {
        fprintf(stderr, "[Thread %d] Cannot open file: %s\n",
                a->thread_id, a->filename);
        return NULL;
    }
 
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);
 
    char *buffer = malloc((size_t)file_size + 1);
    if (!buffer) {
        fprintf(stderr, "[Thread %d] malloc failed\n", a->thread_id);
        fclose(fp);
        return NULL;
    }
 
    size_t bytes_read = fread(buffer, 1, (size_t)file_size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);
 
    /* ── Count occurrences locally (no lock needed) ── */
    int local_count = count_occurrences(buffer, a->keyword);
    free(buffer);
 
    /* ── Critical section: write result and update shared total ── */
    pthread_mutex_lock(&output_mutex);
 
    total_count += local_count;
 
    fprintf(output_fp, "Thread %2d | File: %-30s | Occurrences: %d\n",
            a->thread_id, a->filename, local_count);
    fflush(output_fp);
 
    printf("[Thread %2d] \"%s\" found %d time(s) in %s\n",
           a->thread_id, a->keyword, local_count, a->filename);
 
    pthread_mutex_unlock(&output_mutex);
 
    return NULL;
}
 
/* ── Main ────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    /*
     * argv layout:
     *   argv[1]          = keyword
     *   argv[2]          = output file
     *   argv[3..argc-2]  = input files
     *   argv[argc-1]     = number of threads
     */
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s <keyword> <output.txt> <file1> [file2 ...] <num_threads>\n",
            argv[0]);
        return 1;
    }
 
    const char *keyword     = argv[1];
    const char *output_file = argv[2];
    int         num_threads = atoi(argv[argc - 1]);
    int         num_files   = argc - 4;          /* files between argv[3] and argv[argc-2] */
 
    if (num_threads < 1) {
        fprintf(stderr, "num_threads must be >= 1\n");
        return 1;
    }
    if (num_files < 1) {
        fprintf(stderr, "At least one input file required\n");
        return 1;
    }
 
    /* Cap threads to number of files — excess threads would have no work */
    if (num_threads > num_files) {
        printf("Note: capping threads to number of files (%d)\n", num_files);
        num_threads = num_files;
    }
 
    /* ── Open shared output file ── */
    output_fp = fopen(output_file, "w");
    if (!output_fp) {
        perror("fopen output");
        return 1;
    }
 
    fprintf(output_fp, "=== Keyword Search Results ===\n");
    fprintf(output_fp, "Keyword    : \"%s\"\n", keyword);
    fprintf(output_fp, "Threads    : %d\n", num_threads);
    fprintf(output_fp, "Files      : %d\n\n", num_files);
 
    printf("\nSearching for \"%s\" across %d file(s) using %d thread(s)\n\n",
           keyword, num_files, num_threads);
 
    /* ── Timing start ── */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
 
    /* ── Launch threads in batches (files / threads assignment) ── */
    pthread_t     *threads = malloc((size_t)num_threads * sizeof(pthread_t));
    thread_args_t *args    = malloc((size_t)num_files   * sizeof(thread_args_t));
 
    if (!threads || !args) { perror("malloc"); return 1; }
 
    /*
     * File-to-thread assignment:
     * We process files in batches of num_threads. Within each batch,
     * each thread handles one file. After each batch all threads are
     * joined before the next batch starts.
     */
    int files_done = 0;
 
    while (files_done < num_files) {
        int batch_size = num_threads;
        if (files_done + batch_size > num_files)
            batch_size = num_files - files_done;
 
        /* Create batch */
        for (int i = 0; i < batch_size; i++) {
            int file_idx = files_done + i;
            args[file_idx].thread_id = file_idx;
            args[file_idx].keyword   = keyword;
            args[file_idx].filename  = argv[3 + file_idx];
 
            int rc = pthread_create(&threads[i], NULL,
                                    search_file, &args[file_idx]);
            if (rc != 0) {
                fprintf(stderr, "pthread_create failed: %d\n", rc);
                return 1;
            }
        }
 
        /* Join batch */
        for (int i = 0; i < batch_size; i++)
            pthread_join(threads[i], NULL);
 
        files_done += batch_size;
    }
 
    /* ── Timing end ── */
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed_ms = (t_end.tv_sec  - t_start.tv_sec)  * 1000.0
                      + (t_end.tv_nsec - t_start.tv_nsec) / 1.0e6;
 
    /* ── Write summary to output file ── */
    fprintf(output_fp, "\n-------------------------------\n");
    fprintf(output_fp, "Total occurrences : %d\n", total_count);
    fprintf(output_fp, "Elapsed time      : %.3f ms\n", elapsed_ms);
    fprintf(output_fp, "-------------------------------\n");
    fclose(output_fp);
 
    printf("\n------------------------------------------\n");
    printf("Total occurrences of \"%s\" : %d\n", keyword, total_count);
    printf("Elapsed time               : %.3f ms\n", elapsed_ms);
    printf("Results written to         : %s\n", output_file);
    printf("------------------------------------------\n");
 
    free(threads);
    free(args);
    pthread_mutex_destroy(&output_mutex);
 
    return 0;
}
 