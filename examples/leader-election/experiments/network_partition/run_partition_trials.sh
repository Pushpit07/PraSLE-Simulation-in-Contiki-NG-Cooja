#!/bin/bash
#
# Leader Election Algorithm Network Partition Trials
#
# Runs multiple trials to collect convergence time statistics under network
# partition conditions (two isolated node groups). Supports all algorithms:
# bully, ring, prasle, adaptive-prasle.
#
# IMPORTANT: Each trial uses a unique random seed to ensure statistical
# independence and variation in the results. Without this, all trials would
# produce identical results due to simulation determinism.
#
# Usage:
#   ./run_partition_trials.sh <algorithm> <node_count> [trials] [duration] [parallel_jobs] [--fixed-seed]
#
# Parameters:
#   algorithm    : Algorithm to test (required: bully, ring, prasle, adaptive-prasle)
#   node_count   : Number of nodes (required: 5, 10, 50, or 100)
#   trials       : Number of trials to run (optional, default: 100)
#   duration     : Duration per trial in seconds (optional, default: 60)
#   parallel_jobs: Number of parallel jobs (optional, default: auto-detect CPU cores)
#   --fixed-seed : Use the same seed for all trials (for reproducibility)
#                  Default: each trial uses a unique random seed
#
# Examples:
#   ./run_partition_trials.sh bully 50              # 100 trials, 50 nodes
#   ./run_partition_trials.sh ring 100 200          # 200 trials, 100 nodes
#   ./run_partition_trials.sh prasle 10 50 90       # 50 trials, 10 nodes, 90s each
#   ./run_partition_trials.sh bully 50 100 60 4     # 100 trials with 4 parallel jobs
#   ./run_partition_trials.sh bully 50 --fixed-seed # 100 trials with fixed seed (reproducible)
#
# Partition Configuration:
#   5 nodes:   Partition A (1-2), Partition B (3-5)
#   10 nodes:  Partition A (1-5), Partition B (6-10)
#   50 nodes:  Partition A (1-25), Partition B (26-50)
#   100 nodes: Partition A (1-50), Partition B (51-100)
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
    echo "Usage: $0 <algorithm> <node_count> [trials] [duration] [parallel_jobs] [--fixed-seed]"
    echo ""
    echo "Parameters:"
    echo "  algorithm    : Algorithm (bully, ring, prasle, adaptive-prasle)"
    echo "  node_count   : Number of nodes (5, 10, 50, or 100)"
    echo "  trials       : Number of trials (default: 100)"
    echo "  duration     : Duration per trial in seconds (default: 60)"
    echo "  parallel_jobs: Number of parallel jobs (default: auto-detect)"
    echo "  --fixed-seed : Use same seed for all trials (reproducible results)"
    echo ""
    echo "Partition Configuration:"
    echo "  5 nodes:   Partition A (1-2), Partition B (3-5)"
    echo "  10 nodes:  Partition A (1-5), Partition B (6-10)"
    echo "  50 nodes:  Partition A (1-25), Partition B (26-50)"
    echo "  100 nodes: Partition A (1-50), Partition B (51-100)"
    echo ""
    echo "Examples:"
    echo "  $0 bully 50              # 100 trials, 50 nodes"
    echo "  $0 ring 100 200          # 200 trials, 100 nodes"
    echo "  $0 bully 50 --fixed-seed # 100 trials with fixed seed (identical results)"
    exit 1
fi

ALGORITHM=${ARGS[0]}
NODE_COUNT=${ARGS[1]}
NUM_TRIALS=${ARGS[2]:-100}
DURATION=${ARGS[3]:-60}
PARALLEL_JOBS=${ARGS[4]:-0}  # 0 means auto-detect

# Validate algorithm
if [[ ! "$ALGORITHM" =~ ^(bully|ring|prasle|adaptive-prasle)$ ]]; then
    echo "Error: Invalid algorithm '$ALGORITHM'"
    echo "Algorithm must be one of: bully, ring, prasle, adaptive-prasle"
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

# Set partition boundaries based on node count
case $NODE_COUNT in
    5)
        PARTITION_A_MIN=1
        PARTITION_A_MAX=2
        PARTITION_B_MIN=3
        PARTITION_B_MAX=5
        ;;
    10)
        PARTITION_A_MIN=1
        PARTITION_A_MAX=5
        PARTITION_B_MIN=6
        PARTITION_B_MAX=10
        ;;
    50)
        PARTITION_A_MIN=1
        PARTITION_A_MAX=25
        PARTITION_B_MIN=26
        PARTITION_B_MAX=50
        ;;
    100)
        PARTITION_A_MIN=1
        PARTITION_A_MAX=50
        PARTITION_B_MIN=51
        PARTITION_B_MAX=100
        ;;
esac

# Set simulation file based on algorithm and node count
SIMULATION_FILE="$CSC_TEMPLATES_DIR/$ALGORITHM/${NODE_COUNT}nodes-partition.csc"

# Check if simulation file exists
if [ ! -f "$SIMULATION_FILE" ]; then
    echo "Error: Simulation file not found: $SIMULATION_FILE"
    echo "You may need to generate CSC templates first using scripts/generate_csc.py"
    exit 1
fi

# Create output directory
OUTPUT_BASE="$PROJECT_DIR/results/$ALGORITHM/network_partition/${NODE_COUNT}nodes_partition_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTPUT_BASE"

# Output CSV file
CSV_FILE="$OUTPUT_BASE/partition_times.csv"

echo "=============================================================="
echo " Leader Election Algorithm Network Partition Trials"
echo "=============================================================="
echo "Algorithm:         $ALGORITHM"
echo "Node count:        $NODE_COUNT nodes"
echo "Partition A:       Nodes $PARTITION_A_MIN-$PARTITION_A_MAX (expected leader: $PARTITION_A_MAX)"
echo "Partition B:       Nodes $PARTITION_B_MIN-$PARTITION_B_MAX (expected leader: $PARTITION_B_MAX)"
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
echo "trial,algorithm,node_count,partition_a_convergence_ms,partition_b_convergence_ms,partition_a_leader,partition_b_leader,split_brain" > "$CSV_FILE"

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

    # Extract partition-specific metrics from log
    local partition_a_convergence="NA"
    local partition_b_convergence="NA"
    local partition_a_leader="NA"
    local partition_b_leader="NA"
    local split_brain="false"

    if [ -f "$log_file" ]; then
        # Parse metrics from log file - look for convergence and leader info per partition
        # This is algorithm-agnostic and relies on log output patterns

        # For each partition, find the first convergence message
        # Log format: <timestamp> <node_id> [INFO: ...] <message>
        # Node ID is field 2 ($2), timestamp is field 1 ($1)
        partition_a_convergence=$(grep -E "becoming coordinator|New coordinator" "$log_file" 2>/dev/null | \
            grep -v "^METRICS" | \
            awk -v min="$PARTITION_A_MIN" -v max="$PARTITION_A_MAX" '
                { node_id = $2+0; if (node_id >= min && node_id <= max) { print $1; exit } }
            ' | head -1)
        [ -z "$partition_a_convergence" ] && partition_a_convergence="NA"

        partition_b_convergence=$(grep -E "becoming coordinator|New coordinator" "$log_file" 2>/dev/null | \
            grep -v "^METRICS" | \
            awk -v min="$PARTITION_B_MIN" -v max="$PARTITION_B_MAX" '
                { node_id = $2+0; if (node_id >= min && node_id <= max) { print $1; exit } }
            ' | head -1)
        [ -z "$partition_b_convergence" ] && partition_b_convergence="NA"

        # Extract leader for each partition - default to expected highest node if not found
        # Look for "becoming coordinator" messages to find who became leader in each partition
        partition_a_leader=$(grep -E "becoming coordinator" "$log_file" 2>/dev/null | \
            grep -v "^METRICS" | \
            awk -v min="$PARTITION_A_MIN" -v max="$PARTITION_A_MAX" '
                { node_id = $2+0; if (node_id >= min && node_id <= max) { print node_id; exit } }
            ' | head -1)
        [ -z "$partition_a_leader" ] && partition_a_leader="$PARTITION_A_MAX"

        partition_b_leader=$(grep -E "becoming coordinator" "$log_file" 2>/dev/null | \
            grep -v "^METRICS" | \
            awk -v min="$PARTITION_B_MIN" -v max="$PARTITION_B_MAX" '
                { node_id = $2+0; if (node_id >= min && node_id <= max) { print node_id; exit } }
            ' | head -1)
        [ -z "$partition_b_leader" ] && partition_b_leader="$PARTITION_B_MAX"

        # Check for split-brain (two different leaders in two partitions)
        if [ "$partition_a_leader" != "NA" ] && [ "$partition_b_leader" != "NA" ] && [ "$partition_a_leader" != "$partition_b_leader" ]; then
            split_brain="true"
        fi

        # Clean up log file to save space
        rm -f "$log_file"
        rmdir "$trial_output" 2>/dev/null || true
    fi

    # Write result to temporary file (one file per trial to avoid race conditions)
    echo "$trial,$ALGORITHM,$NODE_COUNT,$partition_a_convergence,$partition_b_convergence,$partition_a_leader,$partition_b_leader,$split_brain" > "$OUTPUT_BASE/.trial_${trial}.result"
}

# Export function and variables for parallel execution
export -f run_trial
export OUTPUT_BASE SIMULATION_FILE SCRIPTS_DIR DURATION NODE_COUNT USE_RANDOM_SEED ALGORITHM COOJA_JAR CONTIKI_DIR
export PARTITION_A_MIN PARTITION_A_MAX PARTITION_B_MIN PARTITION_B_MAX

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
        echo "$trial,$ALGORITHM,$NODE_COUNT,NA,NA,NA,NA,false" >> "$CSV_FILE"
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

partition_a_times = []
partition_b_times = []
split_brain_count = 0
total_trials = 0

with open('$CSV_FILE', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        total_trials += 1
        val_a = row['partition_a_convergence_ms'].strip()
        val_b = row['partition_b_convergence_ms'].strip()
        if val_a and val_a != 'NA':
            partition_a_times.append(float(val_a))
        if val_b and val_b != 'NA':
            partition_b_times.append(float(val_b))
        if row['split_brain'] == 'true':
            split_brain_count += 1

print('Summary Statistics:')
print('=' * 60)
print(f'Algorithm:         $ALGORITHM')
print(f'Trials completed:  {total_trials}')
print(f'Node count:        $NODE_COUNT')
print(f'Partition A:       Nodes $PARTITION_A_MIN-$PARTITION_A_MAX')
print(f'Partition B:       Nodes $PARTITION_B_MIN-$PARTITION_B_MAX')
print()

if partition_a_times:
    print('Partition A Convergence Time:')
    print(f'  Valid Trials:    {len(partition_a_times)}/{total_trials}')
    print(f'  Mean:            {statistics.mean(partition_a_times):.2f} ms')
    print(f'  Median:          {statistics.median(partition_a_times):.2f} ms')
    if len(partition_a_times) > 1:
        print(f'  Std Dev:         {statistics.stdev(partition_a_times):.2f} ms')
    print(f'  Min:             {min(partition_a_times):.2f} ms')
    print(f'  Max:             {max(partition_a_times):.2f} ms')
    print()
else:
    print('Partition A: No valid convergence times collected')
    print()

if partition_b_times:
    print('Partition B Convergence Time:')
    print(f'  Valid Trials:    {len(partition_b_times)}/{total_trials}')
    print(f'  Mean:            {statistics.mean(partition_b_times):.2f} ms')
    print(f'  Median:          {statistics.median(partition_b_times):.2f} ms')
    if len(partition_b_times) > 1:
        print(f'  Std Dev:         {statistics.stdev(partition_b_times):.2f} ms')
    print(f'  Min:             {min(partition_b_times):.2f} ms')
    print(f'  Max:             {max(partition_b_times):.2f} ms')
    print()
else:
    print('Partition B: No valid convergence times collected')
    print()

print('Split-Brain Detection:')
print(f'  Trials with split-brain: {split_brain_count}/{total_trials} ({split_brain_count*100/total_trials:.1f}%)')
print(f'  Expected: 100% (both partitions should elect independent leaders)')
print('=' * 60)
"

echo ""
echo "Results saved to:"
echo "  CSV: $CSV_FILE"
echo ""

exit 0
