////////////////////////////////////////////////////////////////////////////
// task1.c
// -------------------------------------------------------------------------
//
// Searches for prime numbers that are strictly less than an integer n
// (serial version). It uses the same algorithm as the Open MPI version:
//   - n is passed as a command-line argument.
//   - Only the odd candidates 3, 5, 7, ... are tested.
//   - Every candidate is divided only by a small table of odd primes up to sqrt(n).
//   - The primes are written in ascending order to task1_output.txt.
//   - The total time includes the file write.
//
// Build: gcc -O2 -o task1 task1.c
// Run: ./task1 10000000
//
//////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>

#define OUTPUT_FILE "task1_output.txt"
#define INITIAL_CAPACITY 1024

// Growable array of ints holding the primes found
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

// Seconds between two clock readings
static double elapsedSeconds(struct timespec from, struct timespec to) {
    return (to.tv_sec - from.tv_sec) + (to.tv_nsec - from.tv_nsec) * 1e-9;
}

// Main function to execute the program
int main(int argc, char *argv[]) {
    struct timespec start, compute_end, end;

    // Read n from the command line
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <n>\n", argv[0]);
        return 1;
    }

    char *end_ptr = NULL;
    long n = strtol(argv[1], &end_ptr, 10);

    if (end_ptr == argv[1] || *end_ptr != '\0' || n < 0 || n > INT_MAX) {
        fprintf(stderr, "Error: n must be an integer in [0, %d].\n", INT_MAX);
        return 1;
    }

    if (n <= 2) {
        printf("There are no prime numbers less than %ld.\n", n);
        return 0;
    }

    IntList primes;
    primes.count = 0;
    primes.capacity = INITIAL_CAPACITY;
    primes.data = (int *)malloc((size_t)primes.capacity * sizeof(int));

    if (primes.data == NULL) {
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

    if (!listPush(&primes, 2)) {   // 2 is the only even prime
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    // Loop through odd numbers starting from 3 up to n and check if they are prime
    // incrementing by 2 to skip even numbers
    for (long i = 3; i < n; i += 2) {
        if (isPrime((int)i, small_primes, small_count)) {
            if (!listPush(&primes, (int)i)) {
                fprintf(stderr, "Memory allocation failed\n");
                return 1;
            }
        }
    }

    // Time used by the table build and the search
    clock_gettime(CLOCK_MONOTONIC, &compute_end);

    // Write the primes to the output file (included in the total time)
    bool written = writePrimes(primes.data, primes.count);

    // Get the clock current time again
    clock_gettime(CLOCK_MONOTONIC, &end);

    printf("Serial prime search: n = %ld, primes found: %d\n", n, primes.count);
    printf("Compute time (table + search): %lf s\n", elapsedSeconds(start, compute_end));
    printf("File write time: %lf s\n", elapsedSeconds(compute_end, end));
    printf("Total time (compute + file write): %lf s\n", elapsedSeconds(start, end));

    if (written) {
        printf("File '%s' written successfully.\n", OUTPUT_FILE);
    } else {
        printf("Error: Could not create output file.\n");
    }

    free(primes.data);
    free(small_primes);

    return 0;
}
