////////////////////////////////////////////////////////////////////////////
// task3.c
// -------------------------------------------------------------------------
//
// Searches for prime numbers that are strictly less than an integer n
// (OpenMP version). It uses the same algorithm as the Open MPI versions:
//   - n is passed as a command-line argument, the number of threads comes
//     from OMP_NUM_THREADS.
//   - Only the odd candidates 3, 5, 7, ... are tested.
//   - The candidates are split into chunks that are dealt to the threads
//     round-robin. The chunk size selects the distribution:
//         block        one contiguous chunk per thread
//         cyclic       chunk size 1: thread t tests 3 + 2*t, 3 + 2*(t+threads), ...
//         blockcyclic  user-defined chunk size (default 1024)
//   - Every candidate is divided only by a small table of odd primes up to sqrt(n).
//   - Every thread keeps its own list of primes (no shared writes), the sorted
//     lists are merged and the primes are written to task3_output.txt.
//   - The total time includes the file write.
//
// Build: gcc -fopenmp -O2 -o task3 task3.c
// Run: OMP_NUM_THREADS=<t> ./task3 10000000 [block|cyclic|blockcyclic] [chunk]
//
// Run examples: OMP_NUM_THREADS=4 ./task3 10000000
//               OMP_NUM_THREADS=4 ./task3 10000000 cyclic
//               OMP_NUM_THREADS=4 ./task3 10000000 block
//               OMP_NUM_THREADS=4 ./task3 10000000 blockcyclic 1024
//
//////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <omp.h>

#define OUTPUT_FILE "task3_output.txt"
#define INITIAL_CAPACITY 1024
#define DEFAULT_CHUNK 1024

// Supported workload distributions
typedef enum {
    DIST_BLOCK,
    DIST_CYCLIC,
    DIST_BLOCK_CYCLIC,
    DIST_COUNT
} Distribution;

static const char *DIST_NAMES[DIST_COUNT] = {"block", "cyclic", "blockcyclic"};

// Growable array of ints holding the primes found by one thread
typedef struct {
    int *data;
    int  count;
    int  capacity;
} IntList;

static bool listPush(IntList *list, int value) {
    if (list->count == list->capacity) {
        int new_cap = list->capacity * 2;
        int *tmp = (int *)realloc(list->data, (size_t)new_cap * sizeof(int));
        if (tmp == NULL) return false;
        list->data = tmp;
        list->capacity = new_cap;
    }

    list->data[list->count++] = value;

    return true;
}

// Sieve of Eratosthenes: returns the odd primes up to limit (3, 5, 7, ...)
static int *buildSmallPrimes(int limit, int *count) {
    char *composite = (char *)calloc((size_t)limit + 1, sizeof(char));
    int *primes = (int *)malloc(((size_t)limit / 2 + 1) * sizeof(int));

    *count = 0;

    if (composite == NULL || primes == NULL) {
        free(composite);
        free(primes);

        return NULL;
    }

    for (long long i = 3; i <= limit; i += 2) {
        if (!composite[i]) {
            primes[(*count)++] = (int)i;

            for (long long j = i * i; j <= limit; j += 2 * i) {
                composite[j] = 1;
            }
        }
    }

    free(composite);

    return primes;
}

// Primality test: divide only by the odd primes in the table
// (the table must contain every odd prime up to sqrt(k))
static bool isPrime(int k, const int *small_primes, int small_count) {
    if (k <= 1) {
        return false;
    } else if (k == 2) {
        return true;
    } else if (k % 2 == 0) {
        return false;
    }

    for (int j = 0; j < small_count; j++) {
        long long p = small_primes[j];

        if (p * p > k) {
            break;
        }

        if (k % p == 0) {
            return false;
        }
    }

    return true;
}

// One thread: search its chunks and store the primes found (in ascending order)
static bool searchChunks(IntList *list, long worker, long workers, long chunk,
                         long num_candidates, const int *small_primes, int small_count) {
    if (worker == 0) {
        if (!listPush(list, 2)) {   // 2 is the only even prime
            return false;
        }
    }

    // This thread's chunks are worker, worker + workers, worker + 2*workers, ...
    for (long start = worker * chunk; start < num_candidates; start += workers * chunk) {
        long stop = start + chunk;

        if (stop > num_candidates) {
            stop = num_candidates;
        }

        for (long c = start; c < stop; c++) {
            int candidate = (int)(3 + 2 * c);

            if (isPrime(candidate, small_primes, small_count)) {
                if (!listPush(list, candidate)) {
                    return false;
                }
            }
        }
    }

    return true;
}

// Merge p sorted segments (stored back to back in "in") into "out"
static bool mergeSegments(const int *in, int *out, const int *counts,
                          const int *displs, int p, int total) {
    int *pos = (int *)calloc((size_t)p, sizeof(int));   // elements used per segment

    if (pos == NULL) {
        return false;
    }

    for (int k = 0; k < total; k++) {
        int best = -1;

        // Pick the smallest unused element among the segment heads
        for (int r = 0; r < p; r++) {
            if (pos[r] < counts[r] &&
                (best < 0 || in[displs[r] + pos[r]] < in[displs[best] + pos[best]])) {
                best = r;
            }
        }

        out[k] = in[displs[best] + pos[best]];
        pos[best]++;
    }

    free(pos);

    return true;
}

// Write the primes (already in ascending order) to the output file
static bool writePrimes(const int *primes, int total) {
    FILE *fptr = fopen(OUTPUT_FILE, "w");

    if (fptr == NULL) {
        return false;
    }

    for (int i = 0; i < total; i++) {
        if (i > 0) fprintf(fptr, ", ");
        fprintf(fptr, "%d", primes[i]);
    }

    fprintf(fptr, "\n");
    fclose(fptr);

    return true;
}

// Parse the command line: n, an optional distribution and an optional chunk size
static bool parseArguments(int argc, char *argv[], long *n, Distribution *dist, long *chunk) {
    char *end = NULL;

    *dist = DIST_CYCLIC;
    *chunk = DEFAULT_CHUNK;

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s <n> [block|cyclic|blockcyclic] [chunk]\n", argv[0]);
        return false;
    }

    *n = strtol(argv[1], &end, 10);

    if (end == argv[1] || *end != '\0' || *n < 0 || *n > INT_MAX) {
        fprintf(stderr, "Error: n must be an integer in [0, %d].\n", INT_MAX);
        return false;
    }

    if (argc >= 3) {
        int found = -1;

        for (int d = 0; d < DIST_COUNT; d++) {
            if (strcmp(argv[2], DIST_NAMES[d]) == 0) {
                found = d;
            }
        }

        if (found < 0) {
            fprintf(stderr, "Error: distribution must be block, cyclic or blockcyclic.\n");
            return false;
        }

        *dist = (Distribution)found;
    }

    if (argc == 4) {
        *chunk = strtol(argv[3], &end, 10);

        if (*dist != DIST_BLOCK_CYCLIC) {
            fprintf(stderr, "Error: a chunk size can only be given with blockcyclic.\n");
            return false;
        }

        if (end == argv[3] || *end != '\0' || *chunk < 1 || *chunk > INT_MAX) {
            fprintf(stderr, "Error: chunk must be an integer in [1, %d].\n", INT_MAX);
            return false;
        }
    }

    return true;
}

// Seconds between two clock readings
static double elapsedSeconds(struct timespec from, struct timespec to) {
    return (to.tv_sec - from.tv_sec) + (to.tv_nsec - from.tv_nsec) * 1e-9;
}

// Main function to execute the program
int main(int argc, char *argv[]) {
    struct timespec start, compute_end, end;
    long n = 0;
    long chunk = DEFAULT_CHUNK;
    Distribution dist = DIST_CYCLIC;

    // Read n and the workload distribution from the command line
    if (!parseArguments(argc, argv, &n, &dist, &chunk)) {
        return 1;
    }

    if (n <= 2) {
        printf("There are no prime numbers less than %ld.\n", n);
        return 0;
    }

    // Odd candidates are 3, 5, 7, ...: candidate c is the number 3 + 2*c
    int threads = omp_get_max_threads();
    long num_candidates = (n - 2) / 2;

    // Every distribution is "deal chunks of candidates to the threads round-robin"
    if (dist == DIST_BLOCK) {
        chunk = (num_candidates + threads - 1) / threads;
    } else if (dist == DIST_CYCLIC) {
        chunk = 1;
    }

    if (chunk < 1) {
        chunk = 1;
    }

    // One list of primes and one search time per thread
    IntList *thread_lists = (IntList *)calloc((size_t)threads, sizeof(IntList));
    double *thread_times = (double *)calloc((size_t)threads, sizeof(double));

    if (thread_lists == NULL || thread_times == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    // Get current clock time.
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Build the small prime table (odd primes up to sqrt(n - 1))
    long limit = 1;

    while ((limit + 1) * (limit + 1) < n) {
        limit++;
    }

    int small_count = 0;
    int *small_primes = buildSmallPrimes((int)limit, &small_count);

    if (small_primes == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    // Every thread runs the same search on its own thread number.
    // n, chunk and the prime table are shared and only read.
    int search_failed = 0;
    int actual_threads = 0;

    omp_set_dynamic(0);   // ask for exactly "threads" threads

    #pragma omp parallel num_threads(threads)
    {
        int tid = omp_get_thread_num();

        #pragma omp single
        actual_threads = omp_get_num_threads();

        double t_thread = omp_get_wtime();

        IntList mine;
        mine.count = 0;
        mine.capacity = INITIAL_CAPACITY;
        mine.data = (int *)malloc((size_t)mine.capacity * sizeof(int));

        if (mine.data == NULL ||
            !searchChunks(&mine, tid, threads, chunk, num_candidates,
                          small_primes, small_count)) {
            #pragma omp atomic write
            search_failed = 1;
        }

        thread_times[tid] = omp_get_wtime() - t_thread;
        thread_lists[tid] = mine;
    }

    if (search_failed || actual_threads != threads) {
        fprintf(stderr, "Thread search failed (%d of %d threads started)\n",
                actual_threads, threads);
        return 1;
    }

    // Merge the sorted lists of all threads into one sorted list
    int *thread_counts = (int *)malloc((size_t)threads * sizeof(int));
    int *thread_displs = (int *)malloc((size_t)threads * sizeof(int));
    int total = 0;

    if (thread_counts == NULL || thread_displs == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    for (int t = 0; t < threads; t++) {
        thread_counts[t] = thread_lists[t].count;
        thread_displs[t] = total;
        total += thread_counts[t];
    }

    int *thread_data = (int *)malloc(((size_t)total + 1) * sizeof(int));
    int *sorted_primes = (int *)malloc(((size_t)total + 1) * sizeof(int));

    if (thread_data == NULL || sorted_primes == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    for (int t = 0; t < threads; t++) {
        memcpy(thread_data + thread_displs[t], thread_lists[t].data,
               (size_t)thread_counts[t] * sizeof(int));
        free(thread_lists[t].data);
    }

    if (!mergeSegments(thread_data, sorted_primes, thread_counts, thread_displs, threads, total)) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    // Time used by the table build, the threaded search and the merge
    clock_gettime(CLOCK_MONOTONIC, &compute_end);

    // Write the primes to the output file (included in the total time)
    bool written = writePrimes(sorted_primes, total);

    // Get the clock current time again
    clock_gettime(CLOCK_MONOTONIC, &end);

    // Search time of the slowest and the fastest thread
    double t_thread_max = thread_times[0];
    double t_thread_min = thread_times[0];

    for (int t = 1; t < threads; t++) {
        if (thread_times[t] > t_thread_max) t_thread_max = thread_times[t];
        if (thread_times[t] < t_thread_min) t_thread_min = thread_times[t];
    }

    printf("OpenMP prime search: n = %ld, threads: %d, primes found: %d\n", n, threads, total);
    printf("Distribution: %s (chunk size %ld)\n", DIST_NAMES[dist], chunk);
    printf("Search time  (slowest thread): %lf s\n", t_thread_max);
    printf("Search time  (fastest thread): %lf s\n", t_thread_min);
    printf("Compute time (table + search + merge): %lf s\n", elapsedSeconds(start, compute_end));
    printf("File write time: %lf s\n", elapsedSeconds(compute_end, end));
    printf("Total time (compute + file write): %lf s\n", elapsedSeconds(start, end));

    if (written) {
        printf("File '%s' written successfully.\n", OUTPUT_FILE);
    } else {
        printf("Error: Could not create output file.\n");
    }

    free(thread_lists);
    free(thread_times);
    free(thread_counts);
    free(thread_displs);
    free(thread_data);
    free(sorted_primes);
    free(small_primes);

    return 0;
}