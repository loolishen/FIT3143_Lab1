#!/bin/bash

# Configuration settings
EXECUTABLE="./lab_1_revised/task_3/task3"              # Replace with your compiled OpenMP executable path
THREADS=4                         # Number of OpenMP threads
OUTPUT_LOG="results/omp_results_n.csv" # CSV log output file

START_N=60000000
STEP=5000000
COUNT=30

# Export OpenMP threads environment variable
export OMP_NUM_THREADS=$THREADS

# Write CSV header row
echo "Run,n,Threads,OpenMP Run Time (s)" > "$OUTPUT_LOG"

echo "Starting OpenMP benchmarks ($THREADS threads): 30 runs from n=$START_N to n=$((START_N + (COUNT - 1) * STEP))..."
echo "Logging output to $OUTPUT_LOG..."

# Loop 30 times
for (( i=0; i<COUNT; i++ ))
do
    CURRENT_N=$(( START_N + i * STEP ))
    RUN_NUM=$(( i + 1 ))

    echo "Running [$RUN_NUM/$COUNT]: $EXECUTABLE with n=$CURRENT_N (Threads=$OMP_NUM_THREADS)..."

    # Execute and store terminal output
    RAW_OUTPUT=$($EXECUTABLE $CURRENT_N)

    # Extract time value (searches for "Total time" or adapt search string based on program output)
    TOTAL_TIME=$(echo "$RAW_OUTPUT" | grep -i "Total time" | awk '{print $(NF-1)}')

    # Fallback if grep doesn't match standard output formatting
    if [ -z "$TOTAL_TIME" ]; then
        TOTAL_TIME=$(echo "$RAW_OUTPUT" | awk '/[0-9]+\.[0-9]+/ {print $NF}' | tail -n 1)
    fi

    # Append data row to CSV log file
    echo "$RUN_NUM,$CURRENT_N,$THREADS,$TOTAL_TIME" >> "$OUTPUT_LOG"
done

echo "Benchmark finished! Data saved to $OUTPUT_LOG."
