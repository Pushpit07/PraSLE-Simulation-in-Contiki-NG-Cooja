#!/bin/bash
#
# Bully Algorithm Network Noise Resilience Trials
#
# Runs multiple trials to collect convergence time statistics under various
# network noise levels (packet loss conditions).
#
# IMPORTANT: Each trial uses a unique random seed to ensure statistical
# independence and variation in the results. Without this, all trials would
# produce identical results due to simulation determinism.
#
# Usage:
#   ./run_noise_trials.sh <node_count> <noise_level> [trials] [duration] [parallel_jobs] [--fixed-seed]
#
# Parameters:
#   node_count   : Number of nodes (required: 5, 10, 50, or 100)
#   noise_level  : Success rate percentage (required: 90, 70, or 50)
#   trials       : Number of trials to run (optional, default: 100)
#   duration     : Duration per trial in seconds (optional, default: 60)
#   parallel_jobs: Number of parallel jobs (optional, default: auto-detect CPU cores)
#   --fixed-seed : Use the same seed for all trials (for reproducibility)
#                  Default: each trial uses a unique random seed
#
# Examples:
#   ./run_noise_trials.sh 50 90                 # 100 trials, 50 nodes, 90% success rate
#   ./run_noise_trials.sh 100 70 200            # 200 trials, 100 nodes, 70% success rate
#   ./run_noise_trials.sh 10 50 50 90           # 50 trials, 10 nodes, 50% success, 90s each
#   ./run_noise_trials.sh 50 90 100 60 4        # 100 trials with 4 parallel jobs
#   ./run_noise_trials.sh 50 90 --fixed-seed    # 100 trials with fixed seed (reproducible)
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BULLY_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SCRIPTS_DIR="$BULLY_DIR/scripts"
CSC_TEMPLATES_DIR="$SCRIPT_DIR/csc_templates"

# Check for --fixed-seed flag anywhere in arguments
USE_RANDOM_SEED=true
ARGS=()
for arg in "$@"; do
    if [ "$arg" = "--fixed-seed" ]; then
        USE_RANDOM_SEED=false
    else
        ARGS+=("$arg")
    fi
done

# Parse positional arguments
if [ ${#ARGS[@]} -lt 2 ]; then
    echo "Error: Node count and noise level are required"
    echo ""
    echo "Usage: $0 <node_count> <noise_level> [trials] [duration] [parallel_jobs] [--fixed-seed]"
    echo ""
    echo "Parameters:"
    echo "  node_count   : Number of nodes (5, 10, 50, or 100)"
    echo "  noise_level  : Success rate percentage (90, 70, or 50)"
    echo "  trials       : Number of trials (default: 100)"
    echo "  duration     : Duration per trial in seconds (default: 60)"
    echo "  parallel_jobs: Number of parallel jobs (default: auto-detect)"
    echo "  --fixed-seed : Use same seed for all trials (reproducible results)"
    echo ""
    echo "Examples:"
    echo "  $0 50 90              # 100 trials, 50 nodes, 90% success rate"
    echo "  $0 100 70 200         # 200 trials, 100 nodes, 70% success rate"
    echo "  $0 50 90 --fixed-seed # 100 trials with fixed seed (identical results)"
    exit 1
fi

NODE_COUNT=${ARGS[0]}
NOISE_LEVEL=${ARGS[1]}
NUM_TRIALS=${ARGS[2]:-100}
DURATION=${ARGS[3]:-60}
PARALLEL_JOBS=${ARGS[4]:-0}  # 0 means auto-detect

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

# Validate noise level
if [[ ! "$NOISE_LEVEL" =~ ^(90|70|50)$ ]]; then
    echo "Error: Invalid noise level '$NOISE_LEVEL'"
    echo "Noise level must be one of: 90, 70, 50"
    exit 1
fi

# Set simulation file based on node count and noise level
SIMULATION_FILE="$CSC_TEMPLATES_DIR/${NODE_COUNT}nodes-noise${NOISE_LEVEL}.csc"

# Check if simulation file exists
if [ ! -f "$SIMULATION_FILE" ]; then
    echo "Error: Simulation file not found: $SIMULATION_FILE"
    exit 1
fi

# Create output directory
OUTPUT_BASE="$BULLY_DIR/results/noise/${NODE_COUNT}nodes_noise${NOISE_LEVEL}_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT_BASE"

# Output CSV file
CSV_FILE="$OUTPUT_BASE/noise_convergence_times.csv"

echo "=============================================================="
echo " Bully Algorithm Network Noise Resilience Trials"
echo "=============================================================="
echo "Node count:        $NODE_COUNT nodes"
echo "Noise level:       ${NOISE_LEVEL}% success rate"
echo "Simulation file:   $SIMULATION_FILE"
echo "Number of trials:  $NUM_TRIALS"
echo "Duration/trial:    ${DURATION}s"
echo "Parallel jobs:     $PARALLEL_JOBS"
if [ "$USE_RANDOM_SEED" = "true" ]; then
    echo "Seed mode:         Random (unique per trial)"
else
    echo "Seed mode:         Fixed (reproducible results)"
fi
echo "Output directory:  $OUTPUT_BASE"
echo "=============================================================="
echo ""

# Create CSV header
echo "trial,node_count,noise_level,convergence_time_ms,message_count" > "$CSV_FILE"

# Track start time
START_TIME=$(date +%s)

# Function to run a single trial
run_trial() {
    local trial=$1
    local trial_output="$OUTPUT_BASE/trial_$trial"
    local temp_csc="$OUTPUT_BASE/temp_trial_${trial}.csc"

    if [ "$USE_RANDOM_SEED" = "true" ]; then
        # Generate unique seed: strip leading zeros from nanoseconds to avoid octal interpretation
        local nanosec=$(date +%N | sed 's/^0*//')
        nanosec=${nanosec:-0}  # If empty (all zeros), use 0
        local random_seed=$((RANDOM * trial + nanosec))

        # Copy original CSC and replace the random seed
        sed "s/<randomseed>[0-9]*<\/randomseed>/<randomseed>$random_seed<\/randomseed>/" \
            "$SIMULATION_FILE" > "$temp_csc"
    else
        # Use original CSC file with its fixed seed
        cp "$SIMULATION_FILE" "$temp_csc"
    fi

    # Run the experiment with the temporary CSC file
    python3 "$SCRIPTS_DIR/run_experiment.py" \
        --simulation "$temp_csc" \
        --duration "$DURATION" \
        --output "$trial_output" \
        > /dev/null 2>&1

    # Clean up temporary CSC file
    rm -f "$temp_csc"

    # Extract convergence time and message count from metrics.csv
    local convergence_time="NA"
    local message_count="NA"

    if [ -f "$trial_output/metrics.csv" ]; then
        # Extract convergence time
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

        # Extract message count (sum of all message types sent)
        message_count=$(python3 -c "
import csv
import sys

try:
    with open('$trial_output/metrics.csv', 'r') as f:
        reader = csv.DictReader(f)
        total = 0
        for row in reader:
            # Sum all sent message types
            for col in ['msg_election_sent', 'msg_answer_sent', 'msg_coordinator_sent', 'msg_alive_sent']:
                if col in row and row[col]:
                    total += int(float(row[col]))
        print(total if total > 0 else '0')
except Exception as e:
    print('0', file=sys.stderr)
" 2>/dev/null)

        if [ -z "$convergence_time" ] || [ "$convergence_time" = "0" ]; then
            convergence_time="NA"
        fi

        if [ -z "$message_count" ] || [ "$message_count" = "0" ]; then
            message_count="NA"
        fi

        # Clean up trial directory to save space (keep only metrics.csv)
        rm -f "$trial_output/cooja_output.log"
        rm -f "$trial_output/summary.txt"
    fi

    # Write result to temporary file (one file per trial to avoid race conditions)
    echo "$trial,$NODE_COUNT,$NOISE_LEVEL,$convergence_time,$message_count" > "$OUTPUT_BASE/.trial_${trial}.result"
}

# Export function and variables for parallel execution
export -f run_trial
export OUTPUT_BASE SIMULATION_FILE SCRIPTS_DIR DURATION NODE_COUNT NOISE_LEVEL USE_RANDOM_SEED

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
        if [ $COMPLETED -gt 0 ]; then
            printf "\r  Progress: %d/%d (%d%%)   " \
                $COMPLETED $NUM_TRIALS $((COMPLETED * 100 / NUM_TRIALS))
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
    if [ $COMPLETED -gt 0 ]; then
        printf "\r  Progress: %d/%d (%d%%)   " \
            $COMPLETED $NUM_TRIALS $((COMPLETED * 100 / NUM_TRIALS))
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
        echo "$trial,$NODE_COUNT,$NOISE_LEVEL,NA,NA" >> "$CSV_FILE"
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
message_counts = []

with open('$CSV_FILE', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        if row['convergence_time_ms'] != 'NA':
            convergence_times.append(float(row['convergence_time_ms']))
        if row['message_count'] != 'NA':
            message_counts.append(float(row['message_count']))

if convergence_times:
    print('Summary Statistics:')
    print('=' * 60)
    print(f'Trials completed:  {len(convergence_times)}/$NUM_TRIALS')
    print(f'Node count:        $NODE_COUNT')
    print(f'Noise level:       ${NOISE_LEVEL}% success rate')
    print()
    print('Convergence Time:')
    print(f'  Mean:            {statistics.mean(convergence_times):.2f} ms')
    print(f'  Median:          {statistics.median(convergence_times):.2f} ms')
    print(f'  Std Dev:         {statistics.stdev(convergence_times):.2f} ms')
    print(f'  Min:             {min(convergence_times):.2f} ms')
    print(f'  Max:             {max(convergence_times):.2f} ms')
    print(f'  CV:              {(statistics.stdev(convergence_times) / statistics.mean(convergence_times) * 100):.2f}%')
    print()
    if message_counts:
        print('Message Overhead:')
        print(f'  Mean:            {statistics.mean(message_counts):.0f} messages')
        print(f'  Median:          {statistics.median(message_counts):.0f} messages')
        print(f'  Std Dev:         {statistics.stdev(message_counts):.0f} messages')
    print('=' * 60)
else:
    print('ERROR: No valid convergence times collected')
"

echo ""
echo "Results saved to:"
echo "  CSV: $CSV_FILE"
echo ""

exit 0
