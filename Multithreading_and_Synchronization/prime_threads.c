#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
 
/* ── Configuration ────────────────────────────────────────────────────── */
#define UPPER_LIMIT   200000
#define NUM_THREADS   16
#define SEGMENT_SIZE  (UPPER_LIMIT / NUM_THREADS)   /* 12,500 per thread */
 
/* ── Shared state ─────────────────────────────────────────────────────── */
static int             total_primes = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
 
/* ── Thread argument ──────────────────────────────────────────────────── */
typedef struct {
    int thread_id;
    int range_start;   /* inclusive */
    int range_end;     /* inclusive */
} thread_args_t;
 
/* ── Primality test ───────────────────────────────────────────────────── */
/*
 * Returns 1 if n is prime, 0 otherwise.
 * Trial division up to sqrt(n); handles edge cases for n <= 3.
 */
static int is_prime(int n)
{
    if (n < 2)  return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
 
    int limit = (int)sqrt((double)n);
    for (int i = 3; i <= limit; i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}
 
/* ── Thread worker ────────────────────────────────────────────────────── */
/*
 * Each thread:
 *   1. Counts primes in its assigned segment locally (no lock needed here)
 *   2. Acquires the mutex once to add the local count to the shared total
 *   3. Releases the mutex and exits
 *
 * Using a local counter minimises mutex contention — the lock is acquired
 * exactly once per thread rather than once per prime found.
 */
static void *count_primes_in_segment(void *arg)
{
    thread_args_t *args  = (thread_args_t *)arg;
    int local_count = 0;
 
    printf("[Thread %2d] scanning %d – %d\n",
           args->thread_id, args->range_start, args->range_end);
 
    for (int n = args->range_start; n <= args->range_end; n++) {
        if (is_prime(n)) {
            local_count++;
        }
    }
 
    /* ── Critical section: update shared counter ── */
    pthread_mutex_lock(&counter_mutex);
    total_primes += local_count;
    printf("[Thread %2d] found %d primes in [%d, %d], running total = %d\n",
           args->thread_id, local_count,
           args->range_start, args->range_end, total_primes);
    pthread_mutex_unlock(&counter_mutex);
 
    return NULL;
}
 
/* ── Main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    pthread_t     threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];
 
    printf("Counting primes between 1 and %d using %d threads\n",
           UPPER_LIMIT, NUM_THREADS);
    printf("Segment size: %d numbers per thread\n\n", SEGMENT_SIZE);
 
    /* ── Create threads ── */
    for (int t = 0; t < NUM_THREADS; t++) {
        args[t].thread_id   = t;
        args[t].range_start = t * SEGMENT_SIZE + 1;
        args[t].range_end   = (t == NUM_THREADS - 1)
                              ? UPPER_LIMIT               /* last thread takes remainder */
                              : (t + 1) * SEGMENT_SIZE;
 
        int rc = pthread_create(&threads[t], NULL,
                                count_primes_in_segment, &args[t]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed for thread %d: %d\n", t, rc);
            exit(EXIT_FAILURE);
        }
    }
 
    /* ── Join all threads ── */
    for (int t = 0; t < NUM_THREADS; t++) {
        int rc = pthread_join(threads[t], NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_join failed for thread %d: %d\n", t, rc);
            exit(EXIT_FAILURE);
        }
    }
 
    /* ── Destroy mutex ── */
    pthread_mutex_destroy(&counter_mutex);
 
    /* ── Final result ── */
    printf("\nThe synchronized total number of prime numbers"
           " between 1 and 200,000 is %d\n", total_primes);
 
    return 0;
}
 