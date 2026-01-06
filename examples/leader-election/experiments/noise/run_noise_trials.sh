#!/bin/bash
#
# Leader Election Algorithm Network Noise Resilience Trials
#
# Runs multiple trials to collect convergence time statistics under various
# network noise levels (packet loss conditions). Supports all algorithms:
# bully, ring, prasle, adaptive-prasle.
#
# IMPORTANT: Each trial uses a unique random seed to ensure statistical
# independence and variation in the results. Without this, all trials would
# produce identical results due to simulation determinism.
#
# Usage:
#   ./run_noise_trials.sh <algorithm> <node_count> <noise_level> [trials] [duration] [parallel_jobs] [--fixed-seed]
#
# Parameters:
#   algorithm    : Algorithm to test (required: bully, ring, prasle, adaptive-prasle)
#   node_count   : Number of nodes (required: 5, 10, 50, or 100)
#   noise_level  : Success rate percentage (required: 90, 70, or 50)
#   trials       : Number of trials to run (optional, default: 100)
#   duration     : Duration per trial in seconds (optional, default: 60)
#   parallel_jobs: Number of parallel jobs (optional, default: auto-detect CPU cores)
#   --fixed-seed : Use the same seed for all trials (for reproducibility)
#                  Default: each trial uses a unique random seed
#
# Examples:
#   ./run_noise_trials.sh bully 50 90                 # 100 trials, 50 nodes, 90% success rate
#   ./run_noise_trials.sh ring 100 70 200             # 200 trials, 100 nodes, 70% success rate
#   ./run_noise_trials.sh prasle 10 50 50 90          # 50 trials, 10 nodes, 50% success, 90s each
#   ./run_noise_trials.sh bully 50 90 100 60 4        # 100 trials with 4 parallel jobs
#   ./run_noise_trials.sh bully 50 90 --fixed-seed    # 100 trials with fixed seed (reproducible)
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
if [ ${#ARGS[@]} -lt 3 ]; then
    echo "Error: Algorithm, node count, and noise level are required"
    echo ""
    echo "Usage: $0 <algorithm> <node_count> <noise_level> [trials] [duration] [parallel_jobs] [--fixed-seed]"
    echo ""
    echo "Parameters:"
    echo "  algorithm    : Algorithm (bully, ring, prasle, adaptive-prasle)"
    echo "  node_count   : Number of nodes (5, 10, 50, or 100)"
    echo "  noise_level  : Success rate percentage (90, 70, or 50)"
    echo "  trials       : Number of trials (default: 100)"
    echo "  duration     : Duration per trial in seconds (default: 60)"
    echo "  parallel_jobs: Number of parallel jobs (default: auto-detect)"
    echo "  --fixed-seed : Use same seed for all trials (reproducible results)"
    echo ""
    echo "Examples:"
    echo "  $0 bully 50 90              # 100 trials, 50 nodes, 90% success rate"
    echo "  $0 ring 100 70 200          # 200 trials, 100 nodes, 70% success rate"
    echo "  $0 bully 50 90 --fixed-seed # 100 trials with fixed seed (identical results)"
    exit 1
fi

ALGORITHM=${ARGS[0]}
NODE_COUNT=${ARGS[1]}
NOISE_LEVEL=${ARGS[2]}
NUM_TRIALS=${ARGS[3]:-100}
DURATION=${ARGS[4]:-60}
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

# Validate noise level
if [[ ! "$NOISE_LEVEL" =~ ^(90|70|50)$ ]]; then
    echo "Error: Invalid noise level '$NOISE_LEVEL'"
    echo "Noise level must be one of: 90, 70, 50"
    exit 1
fi

# Set simulation file based on algorithm, node count and noise level
SIMULATION_FILE="$CSC_TEMPLATES_DIR/$ALGORITHM/${NODE_COUNT}nodes-noise${NOISE_LEVEL}.csc"

# Check if simulation file exists
if [ ! -f "$SIMULATION_FILE" ]; then
    echo "Error: Simulation file not found: $SIMULATION_FILE"
    echo "You may need to generate CSC templates first using scripts/generate_csc.py"
    exit 1
fi

# Create output directory
OUTPUT_BASE="$PROJECT_DIR/results/$ALGORITHM/noise/${NODE_COUNT}nodes_noise${NOISE_LEVEL}_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT_BASE"

# Output CSV file
CSV_FILE="$OUTPUT_BASE/noise_convergence_times.csv"

echo "=============================================================="
echo " Leader Election Algorithm Network Noise Resilience Trials"
echo "=============================================================="
echo "Algorithm:         $ALGORITHM"
if [[ -n "$TOPOLOGY" ]]; then
    echo "Base algorithm:    $BASE_ALGO"
    echo "Topology:          $TOPOLOGY"
fi
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
echo "trial,algorithm,node_count,noise_level,convergence_time_ms,message_count" > "$CSV_FILE"

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
    local duration_ms=$((DURATION * 1000))

    COOJA_TIMEOUT=$duration_ms java --enable-preview -jar "$COOJA_JAR" \
        --no-gui --autostart --contiki="$CONTIKI_DIR" \
        "$temp_csc" > "$log_file" 2>&1 || true

    # Clean up temporary CSC file
    rm -f "$temp_csc"

    # Extract convergence time and message count from log
    local convergence_time="NA"
    local message_count="NA"

    if [ -f "$log_file" ]; then
        # Extract convergence time from ALL_CONVERGED message (already in milliseconds)
        convergence_time=$(grep -oE "ALL_CONVERGED,[0-9]+" "$log_file" 2>/dev/null | head -1 | cut -d',' -f2 || echo "NA")

        if [ -z "$convergence_time" ] || [ "$convergence_time" = "NA" ]; then
            # Try alternative pattern - extract timestamp from log line
            # Note: Cooja log timestamps are in MICROSECONDS, convert to milliseconds
            local timestamp_us=$(grep -E "CONVERGED|New coordinator|becoming coordinator|Final Leader" "$log_file" 2>/dev/null | \
                head -1 | grep -oE '^[0-9]+' | head -1)
            if [ -n "$timestamp_us" ]; then
                convergence_time=$((timestamp_us / 1000))
            else
                convergence_time="NA"
            fi
        fi

        # Extract message count from METRICS lines
        message_count=$(grep -E "^METRICS," "$log_file" 2>/dev/null | tail -1 | \
            awk -F',' '{
                # Sum up message counts - positions depend on algorithm
                total = 0
                for(i=1; i<=NF; i++) {
                    if($i ~ /^[0-9]+$/) total += $i
                }
                print total
            }' || echo "NA")

        if [ -z "$message_count" ] || [ "$message_count" = "0" ]; then
            message_count="NA"
        fi

        # Clean up log file to save space
        rm -f "$log_file"
        rmdir "$trial_output" 2>/dev/null || true
    fi

    # Write result to temporary file (one file per trial to avoid race conditions)
    echo "$trial,$ALGORITHM,$NODE_COUNT,$NOISE_LEVEL,$convergence_time,$message_count" > "$OUTPUT_BASE/.trial_${trial}.result"
}

# Export function and variables for parallel execution
export -f run_trial
export OUTPUT_BASE SIMULATION_FILE SCRIPTS_DIR DURATION NODE_COUNT NOISE_LEVEL USE_RANDOM_SEED ALGORITHM BASE_ALGO TOPOLOGY COOJA_JAR CONTIKI_DIR

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
        echo "$trial,$ALGORITHM,$NODE_COUNT,$NOISE_LEVEL,NA,NA" >> "$CSV_FILE"
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
    print(f'Algorithm:         $ALGORITHM')
    print(f'Trials completed:  {len(convergence_times)}/$NUM_TRIALS')
    print(f'Node count:        $NODE_COUNT')
    print(f'Noise level:       ${NOISE_LEVEL}% success rate')
    print()
    print('Convergence Time:')
    print(f'  Mean:            {statistics.mean(convergence_times):.2f} ms')
    print(f'  Median:          {statistics.median(convergence_times):.2f} ms')
    if len(convergence_times) > 1:
        print(f'  Std Dev:         {statistics.stdev(convergence_times):.2f} ms')
        print(f'  CV:              {(statistics.stdev(convergence_times) / statistics.mean(convergence_times) * 100):.2f}%')
    print(f'  Min:             {min(convergence_times):.2f} ms')
    print(f'  Max:             {max(convergence_times):.2f} ms')
    print()
    if message_counts:
        print('Message Overhead:')
        print(f'  Mean:            {statistics.mean(message_counts):.0f} messages')
        print(f'  Median:          {statistics.median(message_counts):.0f} messages')
        if len(message_counts) > 1:
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
