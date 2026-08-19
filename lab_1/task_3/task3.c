#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <omp.h>


void printPrimes(int k, char *is_prime);
bool isPrime(int k);


void printPrimes(int k, char *is_prime) {
    if (k <= 100) {
        printf("Primes less than %d: ", k);
        bool first = true;

        for (int j = 2; j < k; j++) {
            if (is_prime[j] == 1) {
                if (!first) printf(", ");
                printf("%d", j);
                first = false;
            }
        }

        printf("\n");

    } else if (k > 100) {
        FILE *fptr;
        char filename[] = "task3_output.txt";

        fptr = fopen(filename, "w");

        if (fptr == NULL) {
            printf("Error: Could not create output file.\n");
            return;
        }

        bool first = true;

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


bool isPrime(int k) {
    if (k <= 1) {
        return false;
    } else if (k == 2) {
        return true;
    } else if (k % 2 == 0) {
        return false;
    }
        
    for (int i = 3; i * i <= k; i += 2) {
        if (k % i == 0) {
            return false;
        }
    }

    return true;
}


int main() {
    int k;
    int i;
    struct timespec start, end;
    double time_taken;

    printf("Enter a number to find prime numbers smaller than it: ");
    scanf("%d", &k);

    if (k <= 2) {
        printf("There are no prime numbers less than %d.\n", k);
        return 1;
    }

    char *is_prime = (char *)calloc(k, sizeof(char)); // using calloc cus want to set everything at 0 first
    // basically cus we want everything to be clear 0 for us to check properly otherwise we get dumb values jic

    if (is_prime == NULL){
        printf("Memory allocation failed\n");
        return 1;
    }

    if (k > 2) {
        is_prime[2] = 1;
    }

    // Get current clock time.
	clock_gettime(CLOCK_MONOTONIC, &start); 

    #pragma omp parallel for private(i) shared(is_prime, k) schedule(dynamic, 500) 
    for (i = 2; i < k; i++) {
        if (isPrime(i)) {
            is_prime[i] = 1;
        }
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
