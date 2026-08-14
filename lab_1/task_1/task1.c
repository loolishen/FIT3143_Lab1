
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>


void writeToFile(int *primes, int count) {
    FILE *fptr;
    char filename[] = "task1_output.txt";

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
    
    fclose(fptr);

    printf("File '%s' written successfully.\n", filename);
}


bool isPrime(int k) {
    if (k <= 1) {
        return false;
    }

    if (k == 2) {
        return true;
    }

    if (k % 2 == 0) {
        return false;
    }
        
    for (int i = 3; i <= sqrt(k); i += 2) {
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
        return;
    }

    int *primes = malloc(sizeof(int) * k);
    int count = 0;

    printf("Primes less than %d: ", k);

    for (int i = 2; i < k; i++) {
        if (isPrime(i)) {
            primes[count] = i;
            count += 1;
        }
    }

    for (int i = 0; i < count; i++) {
        printf("%d", primes[i]);

        if (i < count - 1) {
            printf(", ");
        }
    }

    printf("\n");

    writeToFile(primes, count);
    free(primes);
}


int main() {
    primesLessThan();
}
