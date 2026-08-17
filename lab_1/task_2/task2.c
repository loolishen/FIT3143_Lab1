#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include <pthread.h> 
#include <stdbool.h>

#define NUM_THREADS 16

bool isPrime(int k);
void *ThreadFunc(void *pArg);

typedef struct {
    int thread_id;
    long n;
    char *is_prime;
} ThreadData;

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


void *ThreadFunc(void *pArg)
{
    ThreadData *data = (ThreadData *)pArg;


    // this is to parallel compute (HHAAHHAHA) the threads with like evenly distributed digits
    // so like we jump jump per number of threads instead of fixed 1-5, 6-10, 11-15 typa beat
    int starting_point = 3 + (2 * data->thread_id);

    int step_amount = 2 * NUM_THREADS;

    for (long k = starting_point; k < data->n; k+= step_amount){
        if (isPrime(k)){
            data->is_prime[k]=1;
        }
    }

    return NULL;

}

int main()
{
    int i;
    struct timespec start, end;
    double time_taken;

    pthread_t tid[NUM_THREADS];
	ThreadData thread_data[NUM_THREADS];

    long n = 10000000;

    char *is_prime = (char *)calloc(n, sizeof(char)); // using calloc cus want to set everything at 0 first
    // basically cus we want everything to be clear 0 for us to check properly otherwise we get dumb values jic

    if (is_prime == NULL){
            printf("Memory allocation failed\n");
            return 1;
        }

        if (n > 2) {
    is_prime[2] = 1;
    }

// Get current clock time.
	clock_gettime(CLOCK_MONOTONIC, &start); 
	
    	// Fork		
	for (i = 0; i < NUM_THREADS; i++)
	{
	    thread_data[i].thread_id = i;
        thread_data[i].n = n;
        thread_data[i].is_prime = is_prime;

        pthread_create(&tid[i], NULL, ThreadFunc, &thread_data[i]);
	}
	
	// Join
	for(i = 0; i < NUM_THREADS; i++)
	{
	    	pthread_join(tid[i], NULL);
	}

    // Get the clock current time again
	// Subtract end from start to get the CPU time used.
	clock_gettime(CLOCK_MONOTONIC, &end); 
    time_taken = (end.tv_sec - start.tv_sec) + 
                 (end.tv_nsec - start.tv_nsec) * 1e-9;

    printf("Parallel prime search up to %ld completed in: %lf seconds\n", n, time_taken);

    long count = 0;
    for (long i = 2; i < n; i++) {
        if (is_prime[i] == 1) {
            count++;
        }
    }

    printf("Total primes found: %ld\n", count);

    free(is_prime);

    return 0;


}
