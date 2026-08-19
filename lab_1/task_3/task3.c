#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>


void printPrimes(int k, int *primes, int count);
bool isPrime(int k);
void primesLessThan();


void printPrimes(int k, int *primes, int count) {
    if (k <= 100) {
        printf("Primes less than %d: ", k);

        for (int i = 0; i < count; i++) {
            printf("%d", primes[i]);

            if (i < count - 1) {
                printf(", ");
            }
        }

        printf("\n");

    } else if (k > 100) {
        FILE *fptr;
        char filename[] = "task3_output.txt";

        fptr = fopen(filename, "w");

        if (fptr == NULL) {
            printf("Error: Could not create or open the file.\n");
            return; 
        }
        
        for (int i = 0; i < count; i++) {
            fprintf(fptr, "%d", primes[i]);

            if (i < count - 1) {
                fprintf(fptr, ", ");
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


void primesLessThan() {
    int k;

    printf("Enter a number to find prime numbers smaller than it: ");
    scanf("%d", &k);

    if (k <= 2) {
        printf("There are no prime numbers less than %d.\n", k);
        return;
    }

    int *primes = malloc(sizeof(int) * k);
    int count = 0;

    if (primes == NULL) {
        printf("Memory allocation failed.\n");
        return;
    }

    for (int i = 2; i < k; i++) {
        if (isPrime(i)) {
            primes[count] = i;
            count += 1;
        }
    }

    printPrimes(k, primes, count);
    free(primes);
}


int main() {
    clock_t start = clock();

    primesLessThan();

    clock_t end = clock();
    double time_used = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Execution time: %f seconds\n", time_used);
}
