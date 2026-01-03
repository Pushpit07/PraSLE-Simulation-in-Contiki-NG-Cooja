#!/bin/bash
# ============================================================
# Leader Election Algorithm Framework - Master Experiment Runner
# ============================================================
#
# Usage:
#   ./run_all_experiments.sh --algorithm bully [options]
#   ./run_all_experiments.sh --algorithm all [options]
#
# Options:
#   --algorithm, -a   Algorithm to test (bully, ring, prasle, adaptive-prasle, all)
#   --trials, -t      Number of trials per configuration (default: 10)
#   --nodes, -n       Comma-separated node counts (default: 5,10,50,100)
#   --experiments, -e Comma-separated experiments (default: convergence)
#   --parallel, -p    Number of parallel jobs (default: auto)
#   --dry-run         Preview commands without executing
#   --help, -h        Show this help message
#
# Examples:
#   ./run_all_experiments.sh -a bully -t 50 -n 5,10
#   ./run_all_experiments.sh -a all -e convergence -t 10
#
# ============================================================

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default configuration
ALGORITHM="all"
NUM_TRIALS=10
NODE_COUNTS="5,10,50,100"
EXPERIMENTS="convergence"
PARALLEL_JOBS=""
DRY_RUN=false

# Valid algorithms
VALID_ALGORITHMS=("bully" "ring" "prasle" "adaptive-prasle")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ============================================================
# Helper Functions
# ============================================================

print_header() {
    echo -e "${BLUE}============================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}============================================================${NC}"
}

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_help() {
    echo "Leader Election Algorithm Framework - Experiment Runner"
    echo ""
    echo "Usage:"
    echo "  $0 --algorithm <algo> [options]"
    echo ""
    echo "Options:"
    echo "  --algorithm, -a   Algorithm to test (bully, ring, prasle, adaptive-prasle, all)"
    echo "  --trials, -t      Number of trials per configuration (default: 10)"
    echo "  --nodes, -n       Comma-separated node counts (default: 5,10,50,100)"
    echo "  --experiments, -e Comma-separated experiments (default: convergence)"
    echo "  --parallel, -p    Number of parallel jobs (default: auto-detect)"
    echo "  --dry-run         Preview commands without executing"
    echo "  --help, -h        Show this help message"
    echo ""
    echo "Algorithms:"
    echo "  bully        - Bully leader election algorithm"
    echo "  ring         - Ring-based leader election algorithm"
    echo "  prasle       - PraSLE self-stabilizing algorithm"
    echo "  adaptive-prasle - Customized PraSLE algorithm"
    echo "  all          - Run all algorithms"
    echo ""
    echo "Experiments:"
    echo "  convergence       - Measure convergence time"
    echo "  fault_tolerance   - Test leader crash recovery"
    echo "  noise             - Test under packet loss"
    echo "  network_partition - Test split-brain scenarios"
    echo ""
    echo "Examples:"
    echo "  $0 -a bully -t 50 -n 5,10"
    echo "  $0 -a all -e convergence -t 10"
}

# Auto-detect number of CPU cores
detect_parallel_jobs() {
    if [[ -z "$PARALLEL_JOBS" ]]; then
        if [[ "$(uname)" == "Darwin" ]]; then
            PARALLEL_JOBS=$(sysctl -n hw.ncpu)
        else
            PARALLEL_JOBS=$(nproc 2>/dev/null || echo 4)
        fi
    fi
    echo "$PARALLEL_JOBS"
}

# ============================================================
# Parse Arguments
# ============================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --algorithm|-a)
            ALGORITHM="$2"
            shift 2
            ;;
        --trials|-t)
            NUM_TRIALS="$2"
            shift 2
            ;;
        --nodes|-n)
            NODE_COUNTS="$2"
            shift 2
            ;;
        --experiments|-e)
            EXPERIMENTS="$2"
            shift 2
            ;;
        --parallel|-p)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# ============================================================
# Validate Configuration
# ============================================================

# Validate algorithm
if [[ "$ALGORITHM" == "all" ]]; then
    ALGORITHMS=("${VALID_ALGORITHMS[@]}")
else
    if [[ ! " ${VALID_ALGORITHMS[@]} " =~ " ${ALGORITHM} " ]]; then
        print_error "Invalid algorithm: $ALGORITHM"
        print_error "Valid algorithms: ${VALID_ALGORITHMS[*]}"
        exit 1
    fi
    ALGORITHMS=("$ALGORITHM")
fi

# Parse node counts
IFS=',' read -ra NODES_ARRAY <<< "$NODE_COUNTS"

# Parse experiments
IFS=',' read -ra EXPERIMENTS_ARRAY <<< "$EXPERIMENTS"

# Get parallel jobs
PARALLEL_JOBS=$(detect_parallel_jobs)

# ============================================================
# Print Configuration
# ============================================================

print_header "Leader Election Experiment Configuration"
echo "Algorithms:     ${ALGORITHMS[*]}"
echo "Node counts:    ${NODES_ARRAY[*]}"
echo "Experiments:    ${EXPERIMENTS_ARRAY[*]}"
echo "Trials:         $NUM_TRIALS"
echo "Parallel jobs:  $PARALLEL_JOBS"
echo "Dry run:        $DRY_RUN"
echo ""

# Calculate total experiments
TOTAL_CONFIGS=0
for algo in "${ALGORITHMS[@]}"; do
    for nodes in "${NODES_ARRAY[@]}"; do
        for exp in "${EXPERIMENTS_ARRAY[@]}"; do
            ((TOTAL_CONFIGS++))
        done
    done
done
TOTAL_RUNS=$((TOTAL_CONFIGS * NUM_TRIALS))
print_info "Total configurations: $TOTAL_CONFIGS"
print_info "Total trial runs: $TOTAL_RUNS"
echo ""

if [[ "$DRY_RUN" == "true" ]]; then
    print_warn "DRY RUN MODE - No experiments will be executed"
    echo ""
fi

# ============================================================
# Ensure CSC Templates Exist
# ============================================================

print_header "Checking CSC Templates"

check_and_generate_csc() {
    local exp_type=$1
    local algo=$2
    local nodes=$3
    local csc_filename=$4

    local CSC_DIR="$SCRIPT_DIR/$exp_type/csc_templates/$algo"
    local CSC_FILE="$CSC_DIR/$csc_filename"

    if [[ ! -f "$CSC_FILE" ]]; then
        print_warn "Missing CSC template: $CSC_FILE"
        print_info "Generating CSC templates for $algo ($exp_type)..."

        if [[ "$DRY_RUN" != "true" ]]; then
            python3 "$PROJECT_DIR/scripts/generate_csc.py" \
                --algorithm "$algo" \
                --nodes "$nodes" \
                --experiment "$exp_type" \
                --output "$SCRIPT_DIR/$exp_type/csc_templates/"
        else
            echo "  [DRY RUN] python3 generate_csc.py --algorithm $algo --nodes $nodes --experiment $exp_type"
        fi
    else
        print_info "Found: $CSC_FILE"
    fi
}

for algo in "${ALGORITHMS[@]}"; do
    for nodes in "${NODES_ARRAY[@]}"; do
        for exp in "${EXPERIMENTS_ARRAY[@]}"; do
            case $exp in
                convergence)
                    check_and_generate_csc "$exp" "$algo" "$nodes" "${nodes}nodes.csc"
                    ;;
                fault_tolerance)
                    check_and_generate_csc "$exp" "$algo" "$nodes" "${nodes}nodes-crash.csc"
                    ;;
                noise)
                    for noise_level in 50 70 90; do
                        check_and_generate_csc "$exp" "$algo" "$nodes" "${nodes}nodes-noise${noise_level}.csc"
                    done
                    ;;
                network_partition)
                    check_and_generate_csc "$exp" "$algo" "$nodes" "${nodes}nodes-partition.csc"
                    ;;
            esac
        done
    done
done

# ============================================================
# Run Experiments
# ============================================================

print_header "Running Experiments"

START_TIME=$(date +%s)

# Get experiment duration based on algorithm and node count
# Ring algorithm needs longer duration for large networks due to O(n) message complexity
get_experiment_duration() {
    local algo=$1
    local nodes=$2
    local experiment=$3

    if [[ "$algo" == "ring" ]]; then
        case $nodes in
            50)  echo 120 ;;  # 2 minutes for 50 nodes
            100) echo 180 ;;  # 3 minutes for 100 nodes
            *)   echo 60 ;;   # 1 minute for smaller rings
        esac
    else
        echo 60  # Default 1 minute for other algorithms
    fi
}

for algo in "${ALGORITHMS[@]}"; do
    for exp in "${EXPERIMENTS_ARRAY[@]}"; do
        for nodes in "${NODES_ARRAY[@]}"; do
            echo ""
            print_info "Running $exp experiment for $algo with $nodes nodes"

            # Ring algorithm needs clean build for each RING_SIZE
            if [[ "$algo" == "ring" && "$DRY_RUN" != "true" ]]; then
                rm -rf "$PROJECT_DIR/build" 2>/dev/null || true
            fi

            # Get appropriate duration for this algorithm and node count
            DURATION=$(get_experiment_duration "$algo" "$nodes" "$exp")

            if [[ "$DRY_RUN" != "true" ]]; then
                case $exp in
                    convergence)
                        if [[ -x "$SCRIPT_DIR/convergence/run_convergence_trials.sh" ]]; then
                            "$SCRIPT_DIR/convergence/run_convergence_trials.sh" \
                                "$algo" "$nodes" "$NUM_TRIALS" "$DURATION" "$PARALLEL_JOBS"
                        else
                            print_warn "Convergence trial script not found, skipping..."
                        fi
                        ;;
                    fault_tolerance)
                        if [[ -x "$SCRIPT_DIR/fault_tolerance/run_crash_trials.sh" ]]; then
                            "$SCRIPT_DIR/fault_tolerance/run_crash_trials.sh" \
                                "$algo" "$nodes" "$NUM_TRIALS" 60 120 "$PARALLEL_JOBS"
                        else
                            print_warn "Fault tolerance trial script not found, skipping..."
                        fi
                        ;;
                    noise)
                        if [[ -x "$SCRIPT_DIR/noise/run_noise_trials.sh" ]]; then
                            # Run for all three noise levels: 50%, 70%, 90%
                            for noise_level in 50 70 90; do
                                print_info "Running noise experiment with ${noise_level}% success rate"
                                "$SCRIPT_DIR/noise/run_noise_trials.sh" \
                                    "$algo" "$nodes" "$noise_level" "$NUM_TRIALS" "$DURATION" "$PARALLEL_JOBS"
                            done
                        else
                            print_warn "Noise trial script not found, skipping..."
                        fi
                        ;;
                    network_partition)
                        if [[ -x "$SCRIPT_DIR/network_partition/run_partition_trials.sh" ]]; then
                            "$SCRIPT_DIR/network_partition/run_partition_trials.sh" \
                                "$algo" "$nodes" "$NUM_TRIALS" "$DURATION" "$PARALLEL_JOBS"
                        else
                            print_warn "Network partition trial script not found, skipping..."
                        fi
                        ;;
                    *)
                        print_warn "Experiment type '$exp' not yet implemented"
                        ;;
                esac
            else
                echo "  [DRY RUN] Would run $exp for $algo with $nodes nodes"
            fi
        done
    done
done

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

# ============================================================
# Summary
# ============================================================

echo ""
print_header "Experiment Summary"
echo "Total time:     ${ELAPSED}s"
echo "Configurations: $TOTAL_CONFIGS"
echo "Trial runs:     $TOTAL_RUNS"
echo ""

if [[ "$DRY_RUN" == "true" ]]; then
    print_warn "This was a dry run. No experiments were actually executed."
else
    print_info "Results saved to: $PROJECT_DIR/results/"
fi

echo ""
print_info "Done!"
