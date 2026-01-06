#!/bin/bash
#
# Leader Election Algorithm Leader Crash Recovery Trials
#
# Runs multiple trials to collect leader crash recovery statistics for analysis.
# This script crashes the elected leader at a specified time and measures the
# re-election convergence time. Supports all algorithms: bully, ring, prasle, adaptive-prasle.
#
# IMPORTANT: Each trial uses a unique random seed to ensure statistical
# independence and variation in the results. Without this, all trials would
# produce identical results due to simulation determinism.
#
# Usage:
#   ./run_crash_trials.sh <algorithm> <node_count> [trials] [crash_time] [duration] [parallel_jobs] [--fixed-seed]
#
# Parameters:
#   algorithm    : Algorithm to test (required: bully, ring, prasle, adaptive-prasle)
#   node_count   : Number of nodes (required: 5, 10, 50, or 100)
#   trials       : Number of trials to run (optional, default: 100)
#   crash_time   : Time to crash leader in seconds (optional, default: 60)
#   duration     : Total duration per trial in seconds (optional, default: 120)
#   parallel_jobs: Number of parallel jobs (optional, default: auto-detect CPU cores)
#   --fixed-seed : Use the same seed for all trials (for reproducibility)
#                  Default: each trial uses a unique random seed
#
# Examples:
#   ./run_crash_trials.sh bully 50                    # 100 trials, crash at 60s, 120s total
#   ./run_crash_trials.sh ring 100 200                # 200 trials with 100 nodes
#   ./run_crash_trials.sh prasle 5 50 30 90           # 50 trials, crash at 30s, 90s total
#   ./run_crash_trials.sh bully 50 100 60 120 4       # 100 trials with 4 parallel jobs
#   ./run_crash_trials.sh bully 50 --fixed-seed       # 100 trials with fixed seed (reproducible)
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SCRIPTS_DIR="$PROJECT_DIR/scripts"
CSC_TEMPLATES_DIR="$SCRIPT_DIR/csc_templates"
CONTIKI_DIR="$(cd "$PROJECT_DIR/../.." && pwd)"

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
    echo "Error: Algorithm and node count are required"
    echo ""
    echo "Usage: $0 <algorithm> <node_count> [trials] [crash_time] [duration] [parallel_jobs] [--fixed-seed]"
    echo ""
    echo "Parameters:"
    echo "  algorithm    : Algorithm (bully, ring, prasle, adaptive-prasle)"
    echo "  node_count   : Number of nodes (5, 10, 50, or 100)"
    echo "  trials       : Number of trials (default: 100)"
    echo "  crash_time   : Time to crash leader in seconds (default: 60)"
    echo "  duration     : Total duration per trial in seconds (default: 120)"
    echo "  parallel_jobs: Number of parallel jobs (default: auto-detect)"
    echo "  --fixed-seed : Use same seed for all trials (reproducible results)"
    echo ""
    echo "Examples:"
    echo "  $0 bully 50                   # 100 trials, crash at 60s, 120s total"
    echo "  $0 ring 100 200               # 200 trials with 100 nodes"
    echo "  $0 prasle 5 50 30 90          # 50 trials, crash at 30s, 90s total"
    echo "  $0 bully 50 --fixed-seed      # 100 trials with fixed seed (identical results)"
    exit 1
fi

ALGORITHM=${ARGS[0]}
NODE_COUNT=${ARGS[1]}
NUM_TRIALS=${ARGS[2]:-100}
CRASH_TIME=${ARGS[3]:-60}
DURATION=${ARGS[4]:-120}
PARALLEL_JOBS=${ARGS[5]:-0}  # 0 means auto-detect

# Validate algorithm - supports base algorithms and topology variants
# e.g., "prasle-ring" -> BASE_ALGO="prasle", TOPOLOGY="ring"
# e.g., "bully" -> BASE_ALGO="bully", TOPOLOGY=""
if [[ "$ALGORITHM" =~ ^(prasle|adaptive-prasle)-(ring|line|mesh)$ ]]; then
    BASE_ALGO="${BASH_REMATCH[1]}"
    TOPOLOGY="${BASH_REMATCH[2]}"
elif [[ "$ALGORITHM" =~ ^(bully|ring|prasle|adaptive-prasle)$ ]]; then
    BASE_ALGO="$ALGORITHM"
    TOPOLOGY=""
else
    echo "Error: Invalid algorithm '$ALGORITHM'"
    echo "Algorithm must be one of: bully, ring, prasle, adaptive-prasle"
    echo "Topology variants: prasle-ring, prasle-line, prasle-mesh, adaptive-prasle-ring, etc."
    exit 1
fi

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

# Validate crash_time < duration
if [ "$CRASH_TIME" -ge "$DURATION" ]; then
    echo "Error: crash_time ($CRASH_TIME) must be less than duration ($DURATION)"
    exit 1
fi

# Set simulation file based on algorithm and node count
SIMULATION_FILE="$CSC_TEMPLATES_DIR/$ALGORITHM/${NODE_COUNT}nodes-crash.csc"

# Check if simulation file exists
if [ ! -f "$SIMULATION_FILE" ]; then
    echo "Error: Simulation file not found: $SIMULATION_FILE"
    echo "You may need to generate CSC templates first using scripts/generate_csc.py"
    exit 1
fi

# Create output directory
OUTPUT_BASE="$PROJECT_DIR/results/$ALGORITHM/fault_tolerance/${NODE_COUNT}nodes_crash${CRASH_TIME}s_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT_BASE"

# Output CSV file
CSV_FILE="$OUTPUT_BASE/crash_recovery_times.csv"

echo "=============================================================="
echo " Leader Election Algorithm Crash Recovery Trials"
echo "=============================================================="
echo "Algorithm:         $ALGORITHM"
if [[ -n "$TOPOLOGY" ]]; then
    echo "Base algorithm:    $BASE_ALGO"
    echo "Topology:          $TOPOLOGY"
fi
echo "Node count:        $NODE_COUNT nodes"
echo "Simulation file:   $SIMULATION_FILE"
echo "Number of trials:  $NUM_TRIALS"
echo "Crash time:        ${CRASH_TIME}s"
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
echo "trial,algorithm,node_count,crash_time_s,initial_convergence_time_ms,recovery_convergence_time_ms,total_recovery_time_ms" > "$CSV_FILE"

# Track start time
START_TIME=$(date +%s)

# Find Cooja JAR
COOJA_JAR="$CONTIKI_DIR/tools/cooja/build/libs/cooja.jar"
if [ ! -f "$COOJA_JAR" ]; then
    COOJA_JAR="$CONTIKI_DIR/tools/cooja/dist/cooja.jar"
fi
if [ ! -f "$COOJA_JAR" ]; then
    echo "Error: Cooja JAR not found. Please build Cooja first: cd tools/cooja && ./gradlew jar"
    exit 1
fi

# Function to run a single trial
run_trial() {
    local trial=$1
    local trial_output="$OUTPUT_BASE/trial_$trial"
    local temp_csc="$OUTPUT_BASE/temp_trial_${trial}.csc"
    local log_file="$trial_output/cooja_output.log"

    mkdir -p "$trial_output"

    if [ "$USE_RANDOM_SEED" = "true" ]; then
        # Generate unique seed: strip leading zeros from nanoseconds to avoid octal interpretation
        local nanosec=$(date +%N 2>/dev/null | sed 's/^0*//' || echo "$RANDOM")
        nanosec=${nanosec:-$RANDOM}  # If empty, use RANDOM
        local random_seed=$((RANDOM * trial + nanosec))

        # Copy original CSC and replace the random seed
        sed "s/<randomseed>[0-9]*<\/randomseed>/<randomseed>$random_seed<\/randomseed>/" \
            "$SIMULATION_FILE" > "$temp_csc"
    else
        # Use original CSC file with its fixed seed
        cp "$SIMULATION_FILE" "$temp_csc"
    fi

    # Run Cooja simulation
    # Note: Cooja uses milliseconds for timeout, but log timestamps are in microseconds
    local duration_ms=$((DURATION * 1000))
    local crash_time_us=$((CRASH_TIME * 1000000))  # Crash time in microseconds for log comparison

    local crash_time_ms=$((CRASH_TIME * 1000))  # Crash time in ms for COOJA env var
    COOJA_TIMEOUT=$duration_ms CRASH_TIME=$crash_time_ms java --enable-preview -jar "$COOJA_JAR" \
        --no-gui --autostart --contiki="$CONTIKI_DIR" \
        "$temp_csc" > "$log_file" 2>&1 || true

    # Clean up temporary CSC file
    rm -f "$temp_csc"

    # Extract recovery metrics from log
    local initial_convergence="NA"
    local recovery_convergence="NA"
    local total_recovery="NA"

    if [ -f "$log_file" ]; then
        # Parse convergence times from log
        # Note: Cooja log timestamps are in MICROSECONDS, we convert to milliseconds
        #
        # Supported patterns for initial convergence:
        # - Bully: "becoming coordinator", "New coordinator"
        # - PraSLE: "CONVERGED: Leader"
        # - Ring: "Final Leader"
        local first_conv_us=$(grep -E "CONVERGED|New coordinator|becoming coordinator|Final Leader" "$log_file" 2>/dev/null | grep -v "^METRICS" | head -1 | grep -oE '^[0-9]+' | head -1)

        if [ -n "$first_conv_us" ] && [ "$first_conv_us" -lt "$crash_time_us" ]; then
            # Convert microseconds to milliseconds
            initial_convergence=$((first_conv_us / 1000))
        fi

        # Find recovery convergence after crash (timestamps in microseconds)
        # Supported patterns for recovery:
        # - Bully: "becoming coordinator", "New coordinator"
        # - PraSLE: "Leader changed" (logged when leader value changes after crash)
        # - Ring: "Final Leader"
        local recovery_conv_us=$(grep -E "CONVERGED|New coordinator|becoming coordinator|Final Leader|Leader changed" "$log_file" 2>/dev/null | \
            grep -v "^METRICS" | \
            awk -v crash="$crash_time_us" '{
                match($0, /^[0-9]+/);
                if (RSTART == 0) next;
                time = substr($0, RSTART, RLENGTH);
                if ((time+0) > (crash+0)) { print time; exit }
            }')

        if [ -n "$recovery_conv_us" ]; then
            # Convert microseconds to milliseconds
            recovery_convergence=$((recovery_conv_us / 1000))
            total_recovery=$(( (recovery_conv_us - crash_time_us) / 1000 ))
        fi

        # Clean up trial directory to save space
        rm -f "$log_file"
    fi

    # Write result to temporary file (one file per trial to avoid race conditions)
    echo "$trial,$ALGORITHM,$NODE_COUNT,$CRASH_TIME,$initial_convergence,$recovery_convergence,$total_recovery" > "$OUTPUT_BASE/.trial_${trial}.result"
}

# Export function and variables for parallel execution
export -f run_trial
export OUTPUT_BASE SIMULATION_FILE SCRIPTS_DIR DURATION NODE_COUNT CRASH_TIME USE_RANDOM_SEED ALGORITHM BASE_ALGO TOPOLOGY COOJA_JAR CONTIKI_DIR

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
        echo "$trial,$ALGORITHM,$NODE_COUNT,$CRASH_TIME,NA,NA,NA" >> "$CSV_FILE"
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

initial_times = []
recovery_times = []
total_recovery_times = []

with open('$CSV_FILE', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        if row['initial_convergence_time_ms'] != 'NA':
            initial_times.append(float(row['initial_convergence_time_ms']))
        if row['recovery_convergence_time_ms'] != 'NA':
            recovery_times.append(float(row['recovery_convergence_time_ms']))
        if row['total_recovery_time_ms'] != 'NA':
            total_recovery_times.append(float(row['total_recovery_time_ms']))

print('Summary Statistics:')
print('=' * 60)
print(f'Algorithm:         $ALGORITHM')
print(f'Trials completed:  {max(len(initial_times), len(recovery_times))}/$NUM_TRIALS')
print(f'Node count:        $NODE_COUNT')
print(f'Crash time:        $CRASH_TIME s')
print()

if initial_times:
    print('Initial Convergence (before crash):')
    print(f'  Mean:            {statistics.mean(initial_times):.2f} ms')
    print(f'  Median:          {statistics.median(initial_times):.2f} ms')
    if len(initial_times) > 1:
        print(f'  Std Dev:         {statistics.stdev(initial_times):.2f} ms')
    print(f'  Min:             {min(initial_times):.2f} ms')
    print(f'  Max:             {max(initial_times):.2f} ms')
    print()

if total_recovery_times:
    print('Recovery Time (crash to re-election):')
    print(f'  Mean:            {statistics.mean(total_recovery_times):.2f} ms')
    print(f'  Median:          {statistics.median(total_recovery_times):.2f} ms')
    if len(total_recovery_times) > 1:
        print(f'  Std Dev:         {statistics.stdev(total_recovery_times):.2f} ms')
    print(f'  Min:             {min(total_recovery_times):.2f} ms')
    print(f'  Max:             {max(total_recovery_times):.2f} ms')
    print()
else:
    print('WARNING: No recovery times collected - check simulation logs')

print('=' * 60)
"

echo ""
echo "Results saved to:"
echo "  CSV: $CSV_FILE"
echo ""

exit 0
