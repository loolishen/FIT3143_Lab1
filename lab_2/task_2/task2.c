////////////////////////////////////////////////////////////////////////////
// task2.c
// -------------------------------------------------------------------------
//
// Searches for prime numbers that are strictly less than an integer n
// using hybrid Open MPI + OpenMP.
//
//   - Root parses n (and an optional workload distribution) from the command line and broadcasts them.
//     The number of threads per process is read from OMP_NUM_THREADS on the root and broadcast too.
//     All threads of a process then share (read only) the same n, chunk size and prime table.
//
//   - Every MPI process creates a team of OpenMP threads. Each thread is a worker
//     numbered rank * threads + thread id, so there are size * threads workers in total.
//
//   - The odd candidates 3, 5, 7, ... are split into chunks that are dealt to the workers round-robin. The chunk size selects the distribution:
//         block        one contiguous chunk per worker
//         cyclic       chunk size 1: worker w tests 3 + 2*w, 3 + 2*(w+workers), ...
//         blockcyclic  user-defined chunk size (default 1024)
//
//   - Every process divides only by a small table of odd primes up to sqrt(n).
//
//   - Each process merges the sorted lists of its threads, the lists of all processes are gathered on the root, merged into sorted order, 
//     and written to task2_output.txt by the main thread of the root.
//
// Build: mpicc -fopenmp -O2 -o task2 task2.c
// Run: OMP_NUM_THREADS=<t> mpirun -np <p> --bind-to none ./task2 10000000 [block|cyclic|blockcyclic] [chunk]
//
// Run examples: OMP_NUM_THREADS=4 mpirun -np 2 --bind-to none ./task2 10000000
//               OMP_NUM_THREADS=4 mpirun -np 2 --bind-to none ./task2 10000000 cyclic
//               OMP_NUM_THREADS=4 mpirun -np 2 --bind-to none ./task2 10000000 block
//               OMP_NUM_THREADS=4 mpirun -np 2 --bind-to none ./task2 10000000 blockcyclic 1024
//
//////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <mpi.h>
#include <omp.h>

#define OUTPUT_FILE "task2_output.txt"
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

// Positions of the values the root broadcasts to every rank
enum {
    PARAM_N,
    PARAM_DIST,
    PARAM_CHUNK,
    PARAM_THREADS,
    PARAM_COUNT
};

// Growable array of ints holding the primes found by one rank
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

// One worker (one thread of one process): search its chunks and store the primes found
static bool searchChunks(IntList *list, long worker, long workers, long chunk,
                         long num_candidates, const int *small_primes, int small_count) {
    if (worker == 0) {
        if (!listPush(list, 2)) {   // 2 is the only even prime
            return false;
        }
    }

    // This worker's chunks are worker, worker + workers, worker + 2*workers, ...
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
static void mergeSegments(const int *in, int *out, const int *counts,
                          const int *displs, int p, int total) {
    int *pos = (int *)calloc((size_t)p, sizeof(int));   // elements used per segment

    if (pos == NULL) {
        fprintf(stderr, "Merge: memory allocation failed\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
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
}

// Root only: write the sorted primes to the output file
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

// Root only: parse the command line into params (params[PARAM_N] is -1 on error)
static void parseArguments(int argc, char *argv[], long params[PARAM_COUNT]) {
    char *end = NULL;

    params[PARAM_N] = -1;
    params[PARAM_DIST] = DIST_CYCLIC;
    params[PARAM_CHUNK] = DEFAULT_CHUNK;
    params[PARAM_THREADS] = omp_get_max_threads();   // from OMP_NUM_THREADS

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: mpirun -np <p> %s <n> [block|cyclic|blockcyclic] [chunk]\n",
                argv[0]);
        return;
    }

    long n = strtol(argv[1], &end, 10);

    if (end == argv[1] || *end != '\0' || n < 0 || n > INT_MAX) {
        fprintf(stderr, "Error: n must be an integer in [0, %d].\n", INT_MAX);
        return;
    }

    if (argc >= 3) {
        int dist = -1;

        for (int d = 0; d < DIST_COUNT; d++) {
            if (strcmp(argv[2], DIST_NAMES[d]) == 0) {
                dist = d;
            }
        }

        if (dist < 0) {
            fprintf(stderr, "Error: distribution must be block, cyclic or blockcyclic.\n");
            return;
        }

        params[PARAM_DIST] = dist;
    }

    if (argc == 4) {
        long chunk = strtol(argv[3], &end, 10);

        if (params[PARAM_DIST] != DIST_BLOCK_CYCLIC) {
            fprintf(stderr, "Error: a chunk size can only be given with blockcyclic.\n");
            return;
        }

        if (end == argv[3] || *end != '\0' || chunk < 1 || chunk > INT_MAX) {
            fprintf(stderr, "Error: chunk must be an integer in [1, %d].\n", INT_MAX);
            return;
        }

        params[PARAM_CHUNK] = chunk;
    }

    params[PARAM_N] = n;
}

int main(int argc, char *argv[]) {
    int rank, size, provided;

    // Only the main thread of each process makes MPI calls
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    if (provided < MPI_THREAD_FUNNELED) {
        fprintf(stderr, "Error: this MPI library does not support MPI_THREAD_FUNNELED.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Root reads n, the workload distribution and the thread count
    long params[PARAM_COUNT] = {-1, DIST_CYCLIC, DEFAULT_CHUNK, 1};

    if (rank == 0) {
        parseArguments(argc, argv, params);
    }

    // Broadcast n, the distribution and the thread count to every process
    MPI_Bcast(params, PARAM_COUNT, MPI_LONG, 0, MPI_COMM_WORLD);

    long n = params[PARAM_N];
    Distribution dist = (Distribution)params[PARAM_DIST];
    int threads = (int)params[PARAM_THREADS];

    if (n < 0) {
        MPI_Finalize();
        return 1;
    }

    if (n <= 2) {
        if (rank == 0) {
            printf("There are no prime numbers less than %ld.\n", n);
        }

        MPI_Finalize();

        return 0;
    }

    // Odd candidates are 3, 5, 7, ...: candidate c is the number 3 + 2*c
    long num_candidates = (n - 2) / 2;
    long workers = (long)size * threads;   // every thread of every process is a worker
    long chunk;

    // Every distribution is "deal chunks of candidates to the workers round-robin"
    if (dist == DIST_BLOCK) {
        chunk = (num_candidates + workers - 1) / workers;
    } else if (dist == DIST_CYCLIC) {
        chunk = 1;
    } else {
        chunk = params[PARAM_CHUNK];
    }

    if (chunk < 1) {
        chunk = 1;
    }

    // One list of primes per thread of this process
    IntList *thread_lists = (IntList *)calloc((size_t)threads, sizeof(IntList));

    if (thread_lists == NULL) {
        fprintf(stderr, "Rank %d: memory allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Line up all ranks so the timer starts together
    MPI_Barrier(MPI_COMM_WORLD);
    double t_start = MPI_Wtime();

    // Every rank builds the small prime table (odd primes up to sqrt(n - 1))
    long limit = 1;

    while ((limit + 1) * (limit + 1) < n) {
        limit++;
    }

    int small_count = 0;
    int *small_primes = buildSmallPrimes((int)limit, &small_count);

    if (small_primes == NULL) {
        fprintf(stderr, "Rank %d: memory allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Every thread runs the same search on its own worker number.
    // n, chunk, workers and the prime table are shared and only read.
    int search_failed = 0;
    int actual_threads = 0;

    omp_set_dynamic(0);   // ask for exactly "threads" threads

    #pragma omp parallel num_threads(threads)
    {
        int tid = omp_get_thread_num();

        #pragma omp single
        actual_threads = omp_get_num_threads();

        IntList mine;
        mine.count = 0;
        mine.capacity = INITIAL_CAPACITY;
        mine.data = (int *)malloc((size_t)mine.capacity * sizeof(int));

        if (mine.data == NULL || !searchChunks(&mine, (long)rank * threads + tid, workers, chunk, num_candidates, small_primes, small_count)) {
            #pragma omp atomic write
            search_failed = 1;
        }

        thread_lists[tid] = mine;
    }

    if (search_failed || actual_threads != threads) {
        fprintf(stderr, "Rank %d: thread search failed (%d of %d threads started)\n",
                rank, actual_threads, threads);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Merge the sorted lists of this process's threads into one sorted list
    int *thread_counts = (int *)malloc((size_t)threads * sizeof(int));
    int *thread_displs = (int *)malloc((size_t)threads * sizeof(int));
    int local_total = 0;

    if (thread_counts == NULL || thread_displs == NULL) {
        fprintf(stderr, "Rank %d: memory allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    for (int t = 0; t < threads; t++) {
        thread_counts[t] = thread_lists[t].count;
        thread_displs[t] = local_total;
        local_total += thread_counts[t];
    }

    // +1 so a process that found no primes never asks malloc for zero bytes
    int *thread_data = (int *)malloc(((size_t)local_total + 1) * sizeof(int));
    IntList local;
    local.count = local_total;
    local.capacity = local_total + 1;
    local.data = (int *)malloc((size_t)local.capacity * sizeof(int));

    if (thread_data == NULL || local.data == NULL) {
        fprintf(stderr, "Rank %d: memory allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    for (int t = 0; t < threads; t++) {
        memcpy(thread_data + thread_displs[t], thread_lists[t].data,
               (size_t)thread_counts[t] * sizeof(int));
        free(thread_lists[t].data);
    }

    mergeSegments(thread_data, local.data, thread_counts, thread_displs, threads, local_total);

    double t_compute = MPI_Wtime() - t_start;   // this rank's compute time (table + threaded search + thread merge)

    // Gather all local lists on the root
    int *counts = NULL, *displs = NULL, *all_primes = NULL, *sorted_primes = NULL;
    int total = 0;

    if (rank == 0) {
        counts = (int *)malloc((size_t)size * sizeof(int));
        displs = (int *)malloc((size_t)size * sizeof(int));

        if (counts == NULL || displs == NULL) {
            fprintf(stderr, "Rank 0: memory allocation failed\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Gather(&local.count, 1, MPI_INT, counts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        for (int r = 0; r < size; r++) {
            displs[r] = total;
            total += counts[r];
        }

        all_primes = (int *)malloc((size_t)total * sizeof(int));
        sorted_primes = (int *)malloc((size_t)total * sizeof(int));

        if (all_primes == NULL || sorted_primes == NULL) {
            fprintf(stderr, "Rank 0: memory allocation failed\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Gatherv(local.data, local.count, MPI_INT,
                all_primes, counts, displs, MPI_INT, 0, MPI_COMM_WORLD);

    // Main thread of the root merges the sorted lists and writes the file (both are timed)
    double t_merge = 0.0, t_write = 0.0;
    bool written = false;

    if (rank == 0) {
        double t_phase = MPI_Wtime();
        mergeSegments(all_primes, sorted_primes, counts, displs, size, total);
        t_merge = MPI_Wtime() - t_phase;

        t_phase = MPI_Wtime();
        written = writePrimes(sorted_primes, total);
        t_write = MPI_Wtime() - t_phase;
    }

    double t_total = MPI_Wtime() - t_start;     // compute + gather + merge + file write

    // Timing statistics: slowest and fastest rank
    double t_compute_max, t_compute_min, t_total_max;
    MPI_Reduce(&t_compute, &t_compute_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_compute, &t_compute_min, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_total,   &t_total_max,   1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // Root reports
    if (rank == 0) {
        printf("Processes: %d, threads per process: %d, n = %ld, primes found: %d\n", size, threads, n, total);
        printf("Distribution: %s (chunk size %ld, %ld workers)\n", DIST_NAMES[dist], chunk, workers);
        printf("Compute time  (slowest rank): %lf s\n", t_compute_max);
        printf("Compute time  (fastest rank): %lf s\n", t_compute_min);
        printf("Merge time    (root): %lf s\n", t_merge);
        printf("File write time (root): %lf s\n", t_write);
        printf("Total time (compute + gather + merge + file write): %lf s\n", t_total_max);

        if (written) {
            printf("File '%s' written successfully.\n", OUTPUT_FILE);
        } else {
            printf("Error: Could not create output file.\n");
        }
    }

    free(thread_lists);
    free(thread_counts);
    free(thread_displs);
    free(thread_data);
    free(local.data);
    free(small_primes);
    free(counts);
    free(displs);
    free(all_primes);
    free(sorted_primes);

    MPI_Finalize();
    return 0;
}
