#!/bin/bash
# ============================================================
# Convergence Time Trials - Algorithm Parameterized
# ============================================================
#
# Usage:
#   ./run_convergence_trials.sh <algorithm> <node_count> [trials] [duration] [parallel_jobs]
#
# Parameters:
#   algorithm    : bully, ring, prasle, or adaptive-prasle
#   node_count   : Number of nodes (5, 10, 50, or 100)
#   trials       : Number of trials to run (default: 100)
#   duration     : Simulation duration in seconds (default: 60)
#   parallel_jobs: Number of parallel jobs (default: auto-detect)
#
# Example:
#   ./run_convergence_trials.sh bully 10 50 60 4
#
# ============================================================

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
EXPERIMENTS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Parameters
ALGORITHM=${1:?Error: Algorithm required (bully, ring, prasle, adaptive-prasle)}
NODE_COUNT=${2:?Error: Node count required (5, 10, 50, or 100)}
NUM_TRIALS=${3:-100}
DURATION=${4:-60}
PARALLEL_JOBS=${5:-""}

# Validate algorithm
case "$ALGORITHM" in
    bully|ring|prasle|adaptive-prasle)
        ;;
    *)
        echo "Error: Invalid algorithm '$ALGORITHM'"
        echo "Valid algorithms: bully, ring, prasle, adaptive-prasle"
        exit 1
        ;;
esac

# Validate node count
case "$NODE_COUNT" in
    5|10|50|100)
        ;;
    *)
        echo "Error: Invalid node count '$NODE_COUNT'"
        echo "Valid node counts: 5, 10, 50, 100"
        exit 1
        ;;
esac

# Auto-detect parallel jobs
if [[ -z "$PARALLEL_JOBS" ]]; then
    if [[ "$(uname)" == "Darwin" ]]; then
        PARALLEL_JOBS=$(sysctl -n hw.ncpu)
    else
        PARALLEL_JOBS=$(nproc 2>/dev/null || echo 4)
    fi
fi

# CSC template path
CSC_TEMPLATE="$SCRIPT_DIR/csc_templates/$ALGORITHM/${NODE_COUNT}nodes.csc"

# Output directory
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_DIR="$PROJECT_DIR/results/$ALGORITHM/convergence/${NODE_COUNT}nodes_$TIMESTAMP"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# ============================================================
# Print Configuration
# ============================================================

echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}Convergence Time Trials${NC}"
echo -e "${BLUE}============================================================${NC}"
echo "Algorithm:      $ALGORITHM"
echo "Node count:     $NODE_COUNT"
echo "Trials:         $NUM_TRIALS"
echo "Duration:       ${DURATION}s"
echo "Parallel jobs:  $PARALLEL_JOBS"
echo "CSC template:   $CSC_TEMPLATE"
echo "Output dir:     $OUTPUT_DIR"
echo ""

# Check CSC template exists
if [[ ! -f "$CSC_TEMPLATE" ]]; then
    echo -e "${YELLOW}[WARN]${NC} CSC template not found: $CSC_TEMPLATE"
    echo "Generating CSC template..."
    python3 "$PROJECT_DIR/scripts/generate_csc.py" \
        --algorithm "$ALGORITHM" \
        --nodes "$NODE_COUNT" \
        --output "$SCRIPT_DIR/csc_templates/"
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Results file
RESULTS_FILE="$OUTPUT_DIR/convergence_times.csv"
echo "trial,algorithm,node_count,convergence_time_ms" > "$RESULTS_FILE"

# ============================================================
# Run Trials
# ============================================================

echo -e "${GREEN}[INFO]${NC} Starting $NUM_TRIALS trials..."
echo ""

# Find Cooja JAR
COOJA_JAR=""
if [[ -f "$PROJECT_DIR/../../tools/cooja/dist/cooja.jar" ]]; then
    COOJA_JAR="$PROJECT_DIR/../../tools/cooja/dist/cooja.jar"
elif [[ -f "$PROJECT_DIR/../../tools/cooja/build/libs/cooja.jar" ]]; then
    COOJA_JAR="$PROJECT_DIR/../../tools/cooja/build/libs/cooja.jar"
else
    echo "Error: Could not find cooja.jar"
    echo "Please build Cooja first: cd tools/cooja && ./gradlew jar"
    exit 1
fi

# Find timeout command (macOS uses gtimeout from coreutils)
TIMEOUT_CMD="timeout"
if ! command -v timeout &> /dev/null; then
    if command -v gtimeout &> /dev/null; then
        TIMEOUT_CMD="gtimeout"
    else
        echo -e "${YELLOW}[WARN]${NC} timeout command not found. Install coreutils: brew install coreutils"
        echo "Running without timeout..."
        TIMEOUT_CMD=""
    fi
fi

echo "Using Cooja: $COOJA_JAR"
echo ""

# Function to run a single trial
run_trial() {
    local trial=$1
    local temp_csc="/tmp/trial_${ALGORITHM}_${NODE_COUNT}_${trial}.csc"
    local trial_output="$OUTPUT_DIR/trial_$trial"
    local log_file="$trial_output/cooja_output.log"

    mkdir -p "$trial_output"

    # Generate unique random seed
    local random_seed=$((RANDOM * trial + $(date +%N | sed 's/^0*//')))

    # Create modified CSC with new random seed
    sed "s/<randomseed>.*<\/randomseed>/<randomseed>$random_seed<\/randomseed>/" \
        "$CSC_TEMPLATE" > "$temp_csc"

    # Run Cooja simulation
    if [[ -n "$TIMEOUT_CMD" ]]; then
        $TIMEOUT_CMD $((DURATION + 30)) java --enable-preview -jar "$COOJA_JAR" \
            --no-gui \
            --contiki="$PROJECT_DIR/../.." \
            --logdir="$trial_output" \
            "$temp_csc" \
            > "$log_file" 2>&1 || true
    else
        java --enable-preview -jar "$COOJA_JAR" \
            --no-gui \
            --contiki="$PROJECT_DIR/../.." \
            --logdir="$trial_output" \
            "$temp_csc" \
            > "$log_file" 2>&1 || true
    fi

    # Extract convergence time from log
    # Look for the minimum non-zero first_convergence_time_ms from any node's METRICS
    local convergence_time=""
    if grep -q "ALL_CONVERGED" "$log_file"; then
        convergence_time=$(grep "ALL_CONVERGED" "$log_file" | head -1 | sed 's/.*ALL_CONVERGED,\([0-9]*\).*/\1/')
    elif grep -q "METRICS," "$log_file"; then
        # Extract field 5 (first_convergence_time_ms), filter non-zero, get minimum
        convergence_time=$(grep "METRICS," "$log_file" | cut -d',' -f5 | grep -v "^0$" | sort -n | head -1)
    fi

    # Write result
    if [[ -n "$convergence_time" && "$convergence_time" -gt 0 ]]; then
        echo "$trial,$ALGORITHM,$NODE_COUNT,$convergence_time" >> "$RESULTS_FILE"
        echo "Trial $trial: ${convergence_time}ms"
    else
        echo "Trial $trial: FAILED (no convergence detected)"
    fi

    # Clean up temp CSC
    rm -f "$temp_csc"
}

# Export for parallel execution
export -f run_trial
export ALGORITHM NODE_COUNT CSC_TEMPLATE OUTPUT_DIR RESULTS_FILE COOJA_JAR PROJECT_DIR DURATION TIMEOUT_CMD

# Run trials in parallel
START_TIME=$(date +%s)

for ((trial=1; trial<=NUM_TRIALS; trial++)); do
    # Check if we have slots available
    while [[ $(jobs -r | wc -l) -ge $PARALLEL_JOBS ]]; do
        sleep 1
    done

    run_trial $trial &

    # Progress
    if ((trial % 10 == 0)); then
        echo -e "${GREEN}[INFO]${NC} Progress: $trial/$NUM_TRIALS trials started"
    fi
done

# Wait for all background jobs
wait

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

# ============================================================
# Summary Statistics
# ============================================================

echo ""
echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}Summary Statistics${NC}"
echo -e "${BLUE}============================================================${NC}"

if [[ -f "$RESULTS_FILE" && $(wc -l < "$RESULTS_FILE") -gt 1 ]]; then
    # Extract convergence times and sort them for median calculation
    TIMES=$(tail -n +2 "$RESULTS_FILE" | cut -d',' -f4 | sort -n)
    COUNT=$(echo "$TIMES" | wc -l | tr -d ' ')

    if [[ "$COUNT" -gt 0 ]]; then
        # Calculate statistics
        SUM=$(echo "$TIMES" | awk '{sum+=$1} END {print sum}')
        MEAN=$(echo "scale=2; $SUM / $COUNT" | bc)
        MIN=$(echo "$TIMES" | head -1)
        MAX=$(echo "$TIMES" | tail -1)

        # Calculate median (sorted list)
        if [[ $((COUNT % 2)) -eq 0 ]]; then
            MID=$((COUNT / 2))
            VAL1=$(echo "$TIMES" | sed -n "${MID}p")
            VAL2=$(echo "$TIMES" | sed -n "$((MID + 1))p")
            MEDIAN=$(echo "scale=2; ($VAL1 + $VAL2) / 2" | bc)
        else
            MID=$(((COUNT + 1) / 2))
            MEDIAN=$(echo "$TIMES" | sed -n "${MID}p")
        fi

        # Calculate std dev
        STDDEV=$(echo "$TIMES" | awk -v mean="$MEAN" '{sumsq+=($1-mean)^2} END {print sqrt(sumsq/NR)}')

        echo "Successful trials: $COUNT"
        printf "Mean:              %.2f ms\n" "$MEAN"
        printf "Median:            %.2f ms\n" "$MEDIAN"
        printf "Std Dev:           %.2f ms\n" "$STDDEV"
        printf "Min:               %d ms\n" "$MIN"
        printf "Max:               %d ms\n" "$MAX"
        printf "Range:             %d ms\n" "$((MAX - MIN))"
        if [[ $(echo "$MEAN > 0" | bc) -eq 1 ]]; then
            CV=$(echo "scale=2; ($STDDEV / $MEAN) * 100" | bc)
            printf "CV:                %.2f%%\n" "$CV"
        fi
    else
        echo "No successful trials recorded"
    fi
else
    echo "No successful trials recorded"
fi

echo ""
echo "Total time:      ${ELAPSED}s"
echo "Results saved:   $RESULTS_FILE"
echo ""

echo -e "${GREEN}[INFO]${NC} Done!"
