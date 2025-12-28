#!/bin/bash
#
# Run All Bully Algorithm Experiments
#
# This script executes all experiment types across all node counts.
# It provides a complete data collection suite for thesis evaluation.
#
# Usage:
#   ./run_all_experiments.sh [options]
#
# Options:
#   --trials N       Number of trials per experiment (default: 100)
#   --nodes LIST     Comma-separated list of node counts (default: 5,10,50,100)
#   --experiments E  Comma-separated list of experiments to run:
#                    convergence, fault_tolerance, noise, partition, all (default: all)
#   --noise-levels L Comma-separated noise levels (default: 90,70,50)
#   --parallel N     Number of parallel jobs (default: auto-detect)
#   --dry-run        Show what would be run without executing
#   --help           Show this help message
#
# Examples:
#   ./run_all_experiments.sh                           # Run everything with defaults
#   ./run_all_experiments.sh --trials 50               # Run 50 trials each
#   ./run_all_experiments.sh --nodes 5,10              # Only 5 and 10 nodes
#   ./run_all_experiments.sh --experiments convergence # Only convergence experiments
#   ./run_all_experiments.sh --dry-run                 # Preview commands
#

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BULLY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default values
TRIALS=100
NODE_COUNTS="5,10,50,100"
EXPERIMENTS="all"
NOISE_LEVELS="90,70,50"
PARALLEL_JOBS=0
DRY_RUN=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --trials)
            TRIALS="$2"
            shift 2
            ;;
        --nodes)
            NODE_COUNTS="$2"
            shift 2
            ;;
        --experiments)
            EXPERIMENTS="$2"
            shift 2
            ;;
        --noise-levels)
            NOISE_LEVELS="$2"
            shift 2
            ;;
        --parallel)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --help|-h)
            head -35 "$0" | tail -33
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Convert comma-separated lists to arrays
IFS=',' read -ra NODES_ARRAY <<< "$NODE_COUNTS"
IFS=',' read -ra NOISE_ARRAY <<< "$NOISE_LEVELS"
IFS=',' read -ra EXP_ARRAY <<< "$EXPERIMENTS"

# Check if an experiment should run
should_run() {
    local exp=$1
    if [[ " ${EXP_ARRAY[*]} " =~ " all " ]] || [[ " ${EXP_ARRAY[*]} " =~ " $exp " ]]; then
        return 0
    fi
    return 1
}

# Build parallel jobs argument
PARALLEL_ARG=""
if [ "$PARALLEL_JOBS" -gt 0 ]; then
    PARALLEL_ARG="$PARALLEL_JOBS"
fi

# Print configuration
echo "=============================================================="
echo " Bully Algorithm - Complete Experiment Suite"
echo "=============================================================="
echo "Trials per experiment: $TRIALS"
echo "Node counts:           ${NODES_ARRAY[*]}"
echo "Experiments:           ${EXP_ARRAY[*]}"
echo "Noise levels:          ${NOISE_ARRAY[*]}"
echo "Parallel jobs:         ${PARALLEL_JOBS:-auto}"
echo "Dry run:               $DRY_RUN"
echo "=============================================================="
echo ""

# Track timing
START_TIME=$(date +%s)
TOTAL_EXPERIMENTS=0
COMPLETED_EXPERIMENTS=0

# Count total experiments
if should_run "convergence"; then
    TOTAL_EXPERIMENTS=$((TOTAL_EXPERIMENTS + ${#NODES_ARRAY[@]}))
fi
if should_run "fault_tolerance"; then
    TOTAL_EXPERIMENTS=$((TOTAL_EXPERIMENTS + ${#NODES_ARRAY[@]}))
fi
if should_run "noise"; then
    TOTAL_EXPERIMENTS=$((TOTAL_EXPERIMENTS + ${#NODES_ARRAY[@]} * ${#NOISE_ARRAY[@]}))
fi
if should_run "partition"; then
    TOTAL_EXPERIMENTS=$((TOTAL_EXPERIMENTS + ${#NODES_ARRAY[@]}))
fi

echo "Total experiments to run: $TOTAL_EXPERIMENTS"
echo ""

# Function to run a command
run_cmd() {
    local cmd="$1"
    local desc="$2"

    echo "--------------------------------------------------------------"
    echo "[$((COMPLETED_EXPERIMENTS + 1))/$TOTAL_EXPERIMENTS] $desc"
    echo "Command: $cmd"
    echo "--------------------------------------------------------------"

    if [ "$DRY_RUN" = true ]; then
        echo "[DRY RUN] Skipping execution"
    else
        eval "$cmd"
    fi

    COMPLETED_EXPERIMENTS=$((COMPLETED_EXPERIMENTS + 1))
    echo ""
}

# ============================================================
# Experiment 1: Convergence Trials
# ============================================================
if should_run "convergence"; then
    echo ""
    echo "=============================================================="
    echo " EXPERIMENT 1: Normal Election (Convergence Time)"
    echo "=============================================================="
    echo ""

    for nodes in "${NODES_ARRAY[@]}"; do
        cmd="$SCRIPT_DIR/convergence/run_convergence_trials.sh $nodes $TRIALS 60"
        if [ -n "$PARALLEL_ARG" ]; then
            cmd="$cmd $PARALLEL_ARG"
        fi
        run_cmd "$cmd" "Convergence trials: $nodes nodes, $TRIALS trials"
    done
fi

# ============================================================
# Experiment 2: Fault Tolerance (Crash Recovery)
# ============================================================
if should_run "fault_tolerance"; then
    echo ""
    echo "=============================================================="
    echo " EXPERIMENT 2: Leader Crash Recovery (Fault Tolerance)"
    echo "=============================================================="
    echo ""

    for nodes in "${NODES_ARRAY[@]}"; do
        cmd="$SCRIPT_DIR/fault_tolerance/run_crash_trials.sh $nodes $TRIALS 60 120"
        if [ -n "$PARALLEL_ARG" ]; then
            cmd="$cmd $PARALLEL_ARG"
        fi
        run_cmd "$cmd" "Crash recovery trials: $nodes nodes, $TRIALS trials"
    done
fi

# ============================================================
# Experiment 3a: Network Noise (Packet Loss)
# ============================================================
if should_run "noise"; then
    echo ""
    echo "=============================================================="
    echo " EXPERIMENT 3a: Network Noise (Packet Loss)"
    echo "=============================================================="
    echo ""

    for nodes in "${NODES_ARRAY[@]}"; do
        for noise in "${NOISE_ARRAY[@]}"; do
            cmd="$SCRIPT_DIR/noise/run_noise_trials.sh $nodes $noise $TRIALS 60"
            if [ -n "$PARALLEL_ARG" ]; then
                cmd="$cmd $PARALLEL_ARG"
            fi
            run_cmd "$cmd" "Noise trials: $nodes nodes, ${noise}% success, $TRIALS trials"
        done
    done
fi

# ============================================================
# Experiment 3b: Network Partition (Split-Brain)
# ============================================================
if should_run "partition"; then
    echo ""
    echo "=============================================================="
    echo " EXPERIMENT 3b: Network Partition (Split-Brain)"
    echo "=============================================================="
    echo ""

    for nodes in "${NODES_ARRAY[@]}"; do
        cmd="$SCRIPT_DIR/network_partition/run_partition_trials.sh $nodes $TRIALS 60"
        if [ -n "$PARALLEL_ARG" ]; then
            cmd="$cmd $PARALLEL_ARG"
        fi
        run_cmd "$cmd" "Partition trials: $nodes nodes, $TRIALS trials"
    done
fi

# ============================================================
# Summary
# ============================================================
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
HOURS=$((DURATION / 3600))
MINUTES=$(((DURATION % 3600) / 60))
SECONDS=$((DURATION % 60))

echo ""
echo "=============================================================="
echo " All Experiments Completed!"
echo "=============================================================="
echo "Total experiments:     $COMPLETED_EXPERIMENTS"
echo "Total time:            ${HOURS}h ${MINUTES}m ${SECONDS}s"
echo ""
echo "Results saved to:"
echo "  Convergence:      $BULLY_DIR/results/convergence_trials/"
echo "  Fault Tolerance:  $BULLY_DIR/results/fault_tolerance/"
echo "  Noise:            $BULLY_DIR/results/noise/"
echo "  Partition:        $BULLY_DIR/results/network_partition/"
echo ""
echo "To generate plots, run:"
echo "  ./experiments/generate_all_plots.sh"
echo "=============================================================="

exit 0
