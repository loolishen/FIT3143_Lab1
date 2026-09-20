#!/bin/bash

# Configuration settings
EXECUTABLE="./lab_1_revised/task_3/task3"             # Path to your OpenMP executable
N_VALUE=205000000
OUTPUT_LOG="results/omp_results_thread.csv" # CSV log file

MAX_THREADS=8

# Write CSV header row
echo "Run,Threads,n,OpenMP Run Time (s)" > "$OUTPUT_LOG"

echo "Starting thread scaling benchmark ($N_VALUE): running OMP_NUM_THREADS from 1 to $MAX_THREADS..."
echo "Logging output to $OUTPUT_LOG..."

# Loop thread count from 1 to 8
for (( t=1; t<=MAX_THREADS; t++ ))
do
    echo "Running [OMP_NUM_THREADS=$t]: $EXECUTABLE $N_VALUE..."

    # Set thread count for this iteration
    export OMP_NUM_THREADS=$t

    # Execute binary and capture output
    RAW_OUTPUT=$($EXECUTABLE $N_VALUE)

    # Extract execution time from output
    TOTAL_TIME=$(echo "$RAW_OUTPUT" | grep -i "Total time" | awk '{print $(NF-1)}')

    # Fallback parsing if grep output format varies
    if [ -z "$TOTAL_TIME" ]; then
        TOTAL_TIME=$(echo "$RAW_OUTPUT" | awk '/[0-9]+\.[0-9]+/ {print $NF}' | tail -n 1)
    fi

    # Append data row to CSV log file
    echo "$t,$t,$N_VALUE,$TOTAL_TIME" >> "$OUTPUT_LOG"
done

echo "Benchmark completed! Data saved to $OUTPUT_LOG."
