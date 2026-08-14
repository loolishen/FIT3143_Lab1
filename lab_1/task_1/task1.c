
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>


void writeToFile() {
    FILE *fptr;

    fptr = fopen("task1_output.txt", "w");

    if (fptr == NULL) {
        printf("Error: Could not create or open the file.\n");
        return; 
    }
    
    fprintf(fptr, "test");
    fclose(fptr);

    printf("File 'output.txt' written successfully.\n");
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

    printf("Primes less than %d: ", k);

    for (int i = 2; i < k; i++) {
        if (isPrime(i)) {
            printf("%d, ", i);
        }
    }

    printf("\n");
    writeToFile();
}


int main() {
    primesLessThan();
}
