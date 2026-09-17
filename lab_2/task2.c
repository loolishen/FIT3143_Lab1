#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mpi.h>

bool isPrime(int k);
void printPrimes(int k, char *is_prime);

bool isPrime(int k) {
    if (k <= 1) return false;
    if (k == 2) return true;
    if (k % 2 == 0) return false;
        
    for (int i = 3; i * i <= k; i += 2) {
        if (k % i == 0) {
            return false;
        }
    }
    return true;
}

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
    } else {
        FILE *fptr = fopen("task1_output.txt", "w");
        if (fptr == NULL) {
            fprintf(stderr, "Error: Could not create output file.\n");
            return;
        }
        bool first = true;
        for (int i = 2; i < k; i++) {
            if (is_prime[i] == 1) {
                if (!first) fprintf(fptr, ", ");
                fprintf(fptr, "%d", i);
                first = false;
            }
        }
        fprintf(fptr, "\n");
        fclose(fptr);
        printf("File 'task1_output.txt' written successfully.\n");
    }
}

int main(int argc, char *argv[]) {
    int rank, size;
    int k = 0;
    double start_time, end_time;

    // initialise MPI environment with thread support
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // input validation, checking if the user provided a valid integer n > 2
    if (rank == 0) {
        if (argc != 2) { // if user argc is not equal to 2, print error message and usage instructions
            fprintf(stderr, "Error: Missing input parameter.\nUsage: mpirun -np <procs> %s <integer_n>\n", argv[0]);
            k = -1; 
        } else { // otherwise then we take the ascii value and conver to int using "atoi" 
            k = atoi(argv[1]);
            if (k <= 2) {
                fprintf(stderr, "Error: Value n must be an integer strictly greater than 2.\n");
                k = -1;
            }
        }
    }

    // after the input validation, we broadcast the value of k to all processes in the MPI
    MPI_Bcast(&k, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // and then if the value of k is <= 2 we terminate the program
    if (k <= 2) {
        MPI_Finalize();
        return (rank == 0) ? 1 : 0;
    }


    char *local_is_prime = (char *)calloc(k, sizeof(char));
    char *global_is_prime = NULL;

    // process 0 being the root process allocates memory for the results of the processes
    // into an array
    if (rank == 0) {
        global_is_prime = (char *)calloc(k, sizeof(char));
    }

    // and this validates memory allocation for the local and global array
    // just checks if either of them are NULL then we terminate the program
    if (local_is_prime == NULL || (rank == 0 && global_is_prime == NULL)) {
        fprintf(stderr, "Process %d: Memory allocation failed!\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // if there is only one prime number (2) then the root process is enough
    if (rank == 0) {
        local_is_prime[2] = 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    // this is basically like the previous lab, we calculate the step amount and starting point for each process
    int step_amount = 2 * size;
    int starting_point = 3 + (2 * rank);

    #pragma omp parallel for schedule(guided)
    for (int i = starting_point; i < k; i += step_amount) {
        if (isPrime(i)) {
            local_is_prime[i] = 1;
        }
    }

    // then we combine the results of all processes into the global_is_prime array
    MPI_Reduce(local_is_prime, global_is_prime, k, MPI_CHAR, MPI_MAX, 0, MPI_COMM_WORLD);

    end_time = MPI_Wtime();

    // this is the output of the program, if the rank is 0 then we print the time taken and the primes found
    if (rank == 0) {
        printf("Open MPI prime search up to %d (%d processes) completed in: %lf seconds\n", 
               k, size, end_time - start_time);
        printPrimes(k, global_is_prime);
        free(global_is_prime);
    }

    free(local_is_prime);
    MPI_Finalize();
    return 0;
}