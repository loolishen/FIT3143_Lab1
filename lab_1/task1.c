
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>


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


void primesLessThan(int k) {
    if (k <= 2) {
        return;
    }

    printf("Primes less than %d: ", k);

    for (int i = 2; i < k; i++) {
        if (isPrime(i)) {
            printf("%d, ", i);
        }
    }
}


int main() {
    int myNum;

    printf("Enter a number find prime numbers smaller than it: ");
    scanf("%d", &myNum);
    primesLessThan(myNum);
}
