#!/bin/bash
#
# Bully Algorithm Convergence Time Trials
#
# Runs multiple trials to collect convergence time statistics for analysis.
# This script is designed to gather sufficient data for statistical analysis
# of the leader election convergence time.
#
# IMPORTANT: Each trial uses a unique random seed to ensure statistical
# independence and variation in the results. Without this, all trials would
# produce identical results due to simulation determinism.
#
# Usage:
#   ./run_convergence_trials.sh <node_count> [trials] [duration] [parallel_jobs]
#
# Parameters:
#   node_count   : Number of nodes (required: 5, 10, 50, or 100)
#   trials       : Number of trials to run (optional, default: 100)
#   duration     : Duration per trial in seconds (optional, default: 60)
#   parallel_jobs: Number of parallel jobs (optional, default: auto-detect CPU cores)
#
# Examples:
#   ./run_convergence_trials.sh 50                # 100 trials with 50 nodes, auto-parallel
#   ./run_convergence_trials.sh 100 200           # 200 trials with 100 nodes, auto-parallel
#   ./run_convergence_trials.sh 5 50 90           # 50 trials with 5 nodes, 90s each, auto-parallel
#   ./run_convergence_trials.sh 50 100 60 4       # 100 trials with 4 parallel jobs
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BULLY_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SCRIPTS_DIR="$BULLY_DIR/scripts"

# Parse arguments
if [ $# -lt 1 ]; then
    echo "Error: Node count is required"
    echo ""
    echo "Usage: $0 <node_count> [trials] [duration]"
    echo ""
    echo "Parameters:"
    echo "  node_count   : Number of nodes (5, 10, 50, or 100)"
    echo "  trials       : Number of trials (default: 100)"
    echo "  duration     : Duration per trial in seconds (default: 60)"
    echo ""
    echo "Examples:"
    echo "  $0 50              # 100 trials with 50 nodes"
    echo "  $0 100 200         # 200 trials with 100 nodes"
    echo "  $0 5 50 90         # 50 trials with 5 nodes, 90s each"
    exit 1
fi

NODE_COUNT=$1
NUM_TRIALS=${2:-100}
DURATION=${3:-60}
PARALLEL_JOBS=${4:-0}  # 0 means auto-detect

# Auto-detect CPU cores if parallel_jobs not specified or is 0
if [ "$PARALLEL_JOBS" -eq 0 ]; then
    # Try macOS sysctl first, then Linux nproc
    if command -v sysctl &> /dev/null; then
        PARALLEL_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    elif command -v nproc &> /dev/null; then
        PARALLEL_JOBS=$(nproc)
    else
        PARALLEL_JOBS=4  # Default fallback
    fi
fi

# Validate node count
if [[ ! "$NODE_COUNT" =~ ^(5|10|50|100)$ ]]; then
    echo "Error: Invalid node count '$NODE_COUNT'"
    echo "Node count must be one of: 5, 10, 50, 100"
    exit 1
fi

# Set simulation file based on node count
SIMULATION_FILE="$BULLY_DIR/bully-cooja-${NODE_COUNT}nodes.csc"

# Check if simulation file exists
if [ ! -f "$SIMULATION_FILE" ]; then
    echo "Error: Simulation file not found: $SIMULATION_FILE"
    exit 1
fi

# Create output directory
OUTPUT_BASE="$BULLY_DIR/results/convergence_trials/${NODE_COUNT}nodes_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT_BASE"

# Output CSV file
CSV_FILE="$OUTPUT_BASE/convergence_times.csv"

echo "=============================================================="
echo " Bully Algorithm Convergence Time Trials"
echo "=============================================================="
echo "Node count:        $NODE_COUNT nodes"
echo "Simulation file:   $SIMULATION_FILE"
echo "Number of trials:  $NUM_TRIALS"
echo "Duration/trial:    ${DURATION}s"
echo "Parallel jobs:     $PARALLEL_JOBS"
echo "Output directory:  $OUTPUT_BASE"
echo ""
echo "Estimated total time: ~$((NUM_TRIALS * (DURATION + 10) / PARALLEL_JOBS / 60)) minutes"
echo "=============================================================="
echo ""

# Create CSV header
echo "trial,node_count,convergence_time_ms" > "$CSV_FILE"

# Track start time
START_TIME=$(date +%s)

# Function to run a single trial
run_trial() {
    local trial=$1
    local trial_output="$OUTPUT_BASE/trial_$trial"
    local temp_csc="$OUTPUT_BASE/temp_trial_${trial}.csc"

    # Generate unique seed: strip leading zeros from nanoseconds to avoid octal interpretation
    local nanosec=$(date +%N | sed 's/^0*//')
    nanosec=${nanosec:-0}  # If empty (all zeros), use 0
    local random_seed=$((RANDOM * trial + nanosec))

    # Copy original CSC and replace the random seed
    sed "s/<randomseed>[0-9]*<\/randomseed>/<randomseed>$random_seed<\/randomseed>/" \
        "$SIMULATION_FILE" > "$temp_csc"

    # Run the experiment with the temporary CSC file
    python3 "$SCRIPTS_DIR/run_experiment.py" \
        --simulation "$temp_csc" \
        --duration "$DURATION" \
        --output "$trial_output" \
        > /dev/null 2>&1

    # Clean up temporary CSC file
    rm -f "$temp_csc"

    # Extract convergence time from metrics.csv
    local convergence_time="NA"
    if [ -f "$trial_output/metrics.csv" ]; then
        convergence_time=$(python3 -c "
import csv
import sys

try:
    with open('$trial_output/metrics.csv', 'r') as f:
        reader = csv.DictReader(f)
        times = []
        for row in reader:
            if 'first_convergence_time' in row and row['first_convergence_time']:
                val = float(row['first_convergence_time'])
                if val > 0:
                    times.append(val)
        if times:
            print(int(min(times)))
        else:
            print('0')
except Exception as e:
    print('0', file=sys.stderr)
" 2>/dev/null)

        if [ -z "$convergence_time" ] || [ "$convergence_time" = "0" ]; then
            convergence_time="NA"
        fi

        # Clean up trial directory to save space (keep only metrics.csv)
        rm -f "$trial_output/cooja_output.log"
        rm -f "$trial_output/summary.txt"
    fi

    # Write result to temporary file (one file per trial to avoid race conditions)
    echo "$trial,$NODE_COUNT,$convergence_time" > "$OUTPUT_BASE/.trial_${trial}.result"
}

# Export function and variables for parallel execution
export -f run_trial
export OUTPUT_BASE SIMULATION_FILE SCRIPTS_DIR DURATION NODE_COUNT

# Run trials in parallel
echo "Running $NUM_TRIALS trials with $PARALLEL_JOBS parallel jobs..."
echo ""

# Array to track running jobs
declare -a RUNNING_JOBS=()
COMPLETED=0

for trial in $(seq 1 $NUM_TRIALS); do
    # Wait if we've reached the parallel job limit
    while [ ${#RUNNING_JOBS[@]} -ge $PARALLEL_JOBS ]; do
        # Check which jobs have finished
        for i in "${!RUNNING_JOBS[@]}"; do
            if ! kill -0 ${RUNNING_JOBS[$i]} 2>/dev/null; then
                # Job finished, remove from array
                unset 'RUNNING_JOBS[$i]'
                ((COMPLETED++))
            fi
        done
        # Rebuild array to remove gaps
        RUNNING_JOBS=("${RUNNING_JOBS[@]}")

        # Show progress
        ELAPSED=$(($(date +%s) - START_TIME))
        if [ $COMPLETED -gt 0 ]; then
            AVG_TIME=$((ELAPSED / COMPLETED))
            REMAINING=$((NUM_TRIALS - COMPLETED))
            ETA=$((REMAINING * AVG_TIME / PARALLEL_JOBS))
            ETA_MIN=$((ETA / 60))
            printf "\r  Progress: %d/%d (%d%%) - ETA: ~%d min   " \
                $COMPLETED $NUM_TRIALS $((COMPLETED * 100 / NUM_TRIALS)) $ETA_MIN
        fi

        sleep 1
    done

    # Start new trial in background
    run_trial $trial &
    RUNNING_JOBS+=($!)
done

# Wait for remaining jobs to complete
while [ ${#RUNNING_JOBS[@]} -gt 0 ]; do
    for i in "${!RUNNING_JOBS[@]}"; do
        if ! kill -0 ${RUNNING_JOBS[$i]} 2>/dev/null; then
            unset 'RUNNING_JOBS[$i]'
            ((COMPLETED++))
        fi
    done
    RUNNING_JOBS=("${RUNNING_JOBS[@]}")

    # Show progress
    ELAPSED=$(($(date +%s) - START_TIME))
    if [ $COMPLETED -gt 0 ]; then
        AVG_TIME=$((ELAPSED / COMPLETED))
        REMAINING=$((NUM_TRIALS - COMPLETED))
        ETA=$((REMAINING * AVG_TIME / PARALLEL_JOBS))
        ETA_MIN=$((ETA / 60))
        printf "\r  Progress: %d/%d (%d%%) - ETA: ~%d min   " \
            $COMPLETED $NUM_TRIALS $((COMPLETED * 100 / NUM_TRIALS)) $ETA_MIN
    fi

    sleep 1
done

echo ""
echo ""
echo "All trials completed! Collecting results..."

# Collect results from temporary files and write to CSV in order
for trial in $(seq 1 $NUM_TRIALS); do
    if [ -f "$OUTPUT_BASE/.trial_${trial}.result" ]; then
        cat "$OUTPUT_BASE/.trial_${trial}.result" >> "$CSV_FILE"
        rm -f "$OUTPUT_BASE/.trial_${trial}.result"
    else
        echo "$trial,$NODE_COUNT,NA" >> "$CSV_FILE"
    fi
done

# Calculate summary statistics
echo "=============================================================="
echo " All Trials Completed!"
echo "=============================================================="
echo ""

# Generate summary statistics using Python
python3 -c "
import csv
import statistics

convergence_times = []

with open('$CSV_FILE', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        if row['convergence_time_ms'] != 'NA':
            convergence_times.append(float(row['convergence_time_ms']))

if convergence_times:
    print('Summary Statistics:')
    print('=' * 60)
    print(f'Trials completed:  {len(convergence_times)}/$NUM_TRIALS')
    print(f'Node count:        $NODE_COUNT')
    print(f'Mean:              {statistics.mean(convergence_times):.2f} ms')
    print(f'Median:            {statistics.median(convergence_times):.2f} ms')
    print(f'Std Dev:           {statistics.stdev(convergence_times):.2f} ms')
    print(f'Min:               {min(convergence_times):.2f} ms')
    print(f'Max:               {max(convergence_times):.2f} ms')
    print(f'Range:             {max(convergence_times) - min(convergence_times):.2f} ms')
    print(f'CV:                {(statistics.stdev(convergence_times) / statistics.mean(convergence_times) * 100):.2f}%')
    print('=' * 60)
else:
    print('ERROR: No valid convergence times collected')
"

echo ""
echo "Results saved to:"
echo "  CSV: $CSV_FILE"
echo ""
echo "To visualize results, run:"
echo "  python3 scripts/plot_convergence_distribution.py -i $CSV_FILE -o convergence_${NODE_COUNT}nodes.png"
echo ""

exit 0
