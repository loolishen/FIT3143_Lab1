////////////////////////////////////////////////////////////////////////////
// task1.c
// -------------------------------------------------------------------------
//
// Searches for prime numbers that are strictly less than an integer n
//
//////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>


// Function prototype
void printPrimes(int k, char *is_prime);
bool isPrime(int k);


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
        char filename[] = "task1_output.txt";

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


// Main function to execute the program
int main() {
    int k;
    struct timespec start, end;
    double time_taken;

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


    // Loop through odd numbers starting from 3 up to k and check if they are prime
    // incrementing by 2 to skip even numbers
    for (int i = 3; i < k; i += 2) {
        if (isPrime(i)) {
            is_prime[i] = 1;
        }
    }

    // Get the clock current time again
	// Subtract end from start to get the CPU time used.
	clock_gettime(CLOCK_MONOTONIC, &end); 
    time_taken = (end.tv_sec - start.tv_sec) + 
                 (end.tv_nsec - start.tv_nsec) * 1e-9;
    printf("Series prime search up to %d completed in: %lf seconds\n", k, time_taken);

    printPrimes(k, is_prime);
    free(is_prime);
}
