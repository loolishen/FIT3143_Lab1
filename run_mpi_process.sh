#!/bin/bash

# Configuration settings
EXECUTABLE="./lab_2/task_1/task1"
N_VALUE=205000000
DISTRIBUTION="cyclic"            # Best workload distribution
OUTPUT_LOG="results/mpi_results_process.csv" # CSV log output file

MAX_PROCESSES=8

# Write CSV header row
echo "Run,Processes,n,Distribution,Open MPI Run Time (s)" > "$OUTPUT_LOG"

echo "Starting process scaling benchmark ($N_VALUE): running np from 1 to $MAX_PROCESSES..."
echo "Logging output to $OUTPUT_LOG..."

# Loop process count from 1 to 8
for (( np=1; np<=MAX_PROCESSES; np++ ))
do
    echo "Running [np=$np]: $EXECUTABLE $N_VALUE with $DISTRIBUTION distribution..."

    # Execute mpirun and capture terminal output
    RAW_OUTPUT=$(mpirun -np $np $EXECUTABLE $N_VALUE $DISTRIBUTION)

    # Extract total time value from terminal output
    TOTAL_TIME=$(echo "$RAW_OUTPUT" | grep -i "Total time" | awk '{print $(NF-1)}')

    # Fallback parsing if grep output format varies
    if [ -z "$TOTAL_TIME" ]; then
        TOTAL_TIME=$(echo "$RAW_OUTPUT" | awk '/[0-9]+\.[0-9]+/ {print $NF}' | tail -n 1)
    fi

    # Append data row to CSV log file
    echo "$np,$np,$N_VALUE,$DISTRIBUTION,$TOTAL_TIME" >> "$OUTPUT_LOG"
done

echo "Benchmark finished! Data saved to $OUTPUT_LOG."
