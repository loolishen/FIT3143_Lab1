////////////////////////////////////////////////////////////////////////////
// task2.c
// -------------------------------------------------------------------------
//
// Searches for prime numbers that are strictly less than an integer n
// using POSIX threads (pthreads) for parallelization
//
//////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <pthread.h> 
#include <stdbool.h>

#define NUM_THREADS 16


// Function prototype
void printPrimes(int k, char *is_prime);
bool isPrime(int k);
void *ThreadFunc(void *pArg);


// Structure to hold thread data
typedef struct {
    int thread_id;
    int k;
    char *is_prime;
} ThreadData;


// Function definition

// Function to print prime numbers less than k
void printPrimes(int k, char *is_prime) {
    if (k <= 100) {
        // Print primes to console if k is less than or equal to 100
        printf("Primes less than %d: ", k);
        bool first = true;

        // Loop through the is_prime array and print the prime numbers
        for (int j = 2; j < k; j++) {
            if (is_prime[j] == 1) {
                if (!first) printf(", ");
                printf("%d", j);
                first = false;
            }
        }

        printf("\n");

    } else if (k > 100) {
        // Write primes to file if k is greater than 100
        FILE *fptr;
        char filename[] = "task2_output.txt";

        fptr = fopen(filename, "w");

        if (fptr == NULL) {
            printf("Error: Could not create output file.\n");
            return;
        }

        bool first = true;

        // Loop through the is_prime array and write the prime numbers to the file
        for (int i = 2; i < k; i++) {
            if (is_prime[i] == 1) {
                if (!first) {
                    fprintf(fptr, ", ");
                }
                
                fprintf(fptr, "%d", i);
                first = false;
            }
        }

        fprintf(fptr, "\n");
        fclose(fptr);

        printf("File '%s' written successfully.\n", filename);
    }
}


// Function to check if a number is prime
bool isPrime(int k) {
    if (k <= 1) {
        return false;
    } else if (k == 2) {
        return true;
    } else if (k % 2 == 0) {
        return false;
    }
    
    // Loop through odd numbers starting from 3 up to the square root of k
    // incrementing by 2 to skip even numbers
    for (int i = 3; i * i <= k; i += 2) {
        if (k % i == 0) {
            return false;
        }
    }

    return true;
}


// Thread function to check for prime numbers in a given range
void *ThreadFunc(void *pArg) {
    ThreadData *data = (ThreadData *)pArg;

    // This is to determine the starting point for each thread based on its thread ID
    int starting_point = 3 + (2 * data->thread_id);

    // This is to determine the step amount for each thread based on the number of threads
    int step_amount = 2 * NUM_THREADS;

    // Loop through the range of numbers assigned to this thread and checks if they are prime,
    // marking them in the is_prime array
    for (int i = starting_point; i < data->k; i += step_amount) {
        if (isPrime(i)) {
            data->is_prime[i] = 1;
        }
    }

    return NULL;
}


// Main function to execute the program
int main() {
    int k;
    struct timespec start, end;
    double time_taken;

    pthread_t tid[NUM_THREADS];
	ThreadData thread_data[NUM_THREADS];

    printf("Enter a number to find prime numbers smaller than it: ");
    scanf("%d", &k);

    if (k <= 2) {
        printf("There are no prime numbers less than %d.\n", k);
        return 1;
    }

    // Allocate memory for the is_prime array and initialize it to 0
    char *is_prime = (char *)calloc(k, sizeof(char));

    if (is_prime == NULL){
        printf("Memory allocation failed\n");
        return 1;
    }

    if (k > 2) {
        is_prime[2] = 1;
    }

    // Get current clock time.
	clock_gettime(CLOCK_MONOTONIC, &start); 
	
    // Fork		
	for (int i = 0; i < NUM_THREADS; i++) {
	    thread_data[i].thread_id = i;
        thread_data[i].k = k;
        thread_data[i].is_prime = is_prime;

        pthread_create(&tid[i], NULL, ThreadFunc, &thread_data[i]);
	}
	
	// Join
	for (int i = 0; i < NUM_THREADS; i++) {
	    pthread_join(tid[i], NULL);
	}

    // Get the clock current time again
	// Subtract end from start to get the CPU time used.
	clock_gettime(CLOCK_MONOTONIC, &end); 
    time_taken = (end.tv_sec - start.tv_sec) + 
                 (end.tv_nsec - start.tv_nsec) * 1e-9;
    printf("Parallel prime search up to %d completed in: %lf seconds\n", k, time_taken);

    printPrimes(k, is_prime);
    free(is_prime);
}
