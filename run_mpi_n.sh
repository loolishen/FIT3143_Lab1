#!/bin/bash

# Configuration settings
EXECUTABLE="./lab_2/task_1/task1"
NP=4                            # Number of Open MPI processes
DISTRIBUTION="cyclic"          # Workload distribution scheme
OUTPUT_LOG="results/mpi_results_n.csv" # CSV file to record results

START_N=60000000
STEP=5000000
COUNT=30

# Initialize/overwrite the CSV output file with headers
echo "Run,n,Processes,Distribution,Open MPI Run Time (s)" > "$OUTPUT_LOG"

# Display progress on terminal
echo "Starting benchmarks: 30 runs from n=$START_N to n=$((START_N + (COUNT - 1) * STEP))..."
echo "Logging output to $OUTPUT_LOG..."

# Loop 30 times
for (( i=0; i<COUNT; i++ ))
do
    # Calculate current n
    CURRENT_N=$(( START_N + i * STEP ))
    RUN_NUM=$(( i + 1 ))

    echo "Running [$RUN_NUM/$COUNT]: $EXECUTABLE with n=$CURRENT_N..."

    # Execute mpirun and capture terminal output
    RAW_OUTPUT=$(mpirun -np $NP $EXECUTABLE $CURRENT_N $DISTRIBUTION)

    # Extract total time from the output line: "Total time (...): X.XXXXXX s"
    TOTAL_TIME=$(echo "$RAW_OUTPUT" | grep -i "Total time" | awk '{print $(NF-1)}')

    # Append data row to CSV log file
    echo "$RUN_NUM,$CURRENT_N,$NP,$DISTRIBUTION,$TOTAL_TIME" >> "$OUTPUT_LOG"
done

echo "Benchmark completed! Results saved to $OUTPUT_LOG."
