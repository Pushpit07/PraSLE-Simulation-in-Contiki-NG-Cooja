#!/bin/bash
#
# Generate All Experiment Plots
#
# This script generates visualization plots for all experiment results.
# It auto-detects the most recent results for each experiment type.
# Supports all algorithms: bully, ring, prasle, adaptive-prasle.
#
# Usage:
#   ./generate_all_plots.sh [options]
#
# Options:
#   --algorithm A    Algorithm to generate plots for (default: all)
#                    Options: bully, ring, prasle, adaptive-prasle, all
#   --experiments E  Comma-separated list of experiments:
#                    convergence, fault_tolerance, noise, partition, all (default: all)
#   --help           Show this help message
#
# Examples:
#   ./generate_all_plots.sh                              # Generate all plots for all algorithms
#   ./generate_all_plots.sh --algorithm bully            # Only bully algorithm
#   ./generate_all_plots.sh --experiments convergence    # Only convergence plots
#   ./generate_all_plots.sh --algorithm ring --experiments fault_tolerance,noise
#

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default values
ALGORITHMS="all"
EXPERIMENTS="all"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --algorithm|-a)
            ALGORITHMS="$2"
            shift 2
            ;;
        --experiments|-e)
            EXPERIMENTS="$2"
            shift 2
            ;;
        --help|-h)
            head -25 "$0" | tail -23
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Build algorithm list
if [ "$ALGORITHMS" = "all" ]; then
    ALGO_ARRAY=("bully" "ring" "prasle" "adaptive-prasle")
else
    IFS=',' read -ra ALGO_ARRAY <<< "$ALGORITHMS"
fi

# Convert comma-separated experiments list to array
IFS=',' read -ra EXP_ARRAY <<< "$EXPERIMENTS"

# Check if an experiment should run
should_run() {
    local exp=$1
    if [[ " ${EXP_ARRAY[*]} " =~ " all " ]] || [[ " ${EXP_ARRAY[*]} " =~ " $exp " ]]; then
        return 0
    fi
    return 1
}

echo "=============================================================="
echo " Generating Experiment Plots"
echo "=============================================================="
echo "Algorithms:  ${ALGO_ARRAY[*]}"
echo "Experiments: ${EXPERIMENTS}"
echo ""

GENERATED=0
FAILED=0
SKIPPED=0

for ALGORITHM in "${ALGO_ARRAY[@]}"; do
    echo ""
    ALGORITHM_UPPER=$(echo "$ALGORITHM" | tr '[:lower:]' '[:upper:]')
    echo "=============================================================="
    echo " Algorithm: $ALGORITHM_UPPER"
    echo "=============================================================="

    # ============================================================
    # Convergence Plots
    # ============================================================
    if should_run "convergence"; then
        echo "--- Convergence Plots ---"
        if [ -x "$SCRIPT_DIR/convergence/create-convergence-plot.sh" ]; then
            if "$SCRIPT_DIR/convergence/create-convergence-plot.sh" -a "$ALGORITHM" 2>/dev/null; then
                echo "  [OK] Convergence plots generated"
                GENERATED=$((GENERATED + 1))
            else
                echo "  [SKIP] No convergence data found for $ALGORITHM"
                SKIPPED=$((SKIPPED + 1))
            fi
        else
            echo "  [ERROR] Script not found or not executable"
            FAILED=$((FAILED + 1))
        fi
        echo ""
    fi

    # ============================================================
    # Fault Tolerance Plots
    # ============================================================
    if should_run "fault_tolerance"; then
        echo "--- Fault Tolerance Plots ---"
        if [ -x "$SCRIPT_DIR/fault_tolerance/create-fault-tolerance-plot.sh" ]; then
            if "$SCRIPT_DIR/fault_tolerance/create-fault-tolerance-plot.sh" -a "$ALGORITHM" 2>/dev/null; then
                echo "  [OK] Fault tolerance plots generated"
                GENERATED=$((GENERATED + 1))
            else
                echo "  [SKIP] No fault tolerance data found for $ALGORITHM"
                SKIPPED=$((SKIPPED + 1))
            fi
        else
            echo "  [ERROR] Script not found or not executable"
            FAILED=$((FAILED + 1))
        fi
        echo ""
    fi

    # ============================================================
    # Noise Plots
    # ============================================================
    if should_run "noise"; then
        echo "--- Noise Plots ---"
        if [ -x "$SCRIPT_DIR/noise/create-noise-plot.sh" ]; then
            if "$SCRIPT_DIR/noise/create-noise-plot.sh" -a "$ALGORITHM" 2>/dev/null; then
                echo "  [OK] Noise plots generated"
                GENERATED=$((GENERATED + 1))
            else
                echo "  [SKIP] No noise data found for $ALGORITHM"
                SKIPPED=$((SKIPPED + 1))
            fi
        else
            echo "  [ERROR] Script not found or not executable"
            FAILED=$((FAILED + 1))
        fi
        echo ""
    fi

    # ============================================================
    # Partition Plots
    # ============================================================
    if should_run "partition"; then
        echo "--- Partition Plots ---"
        if [ -x "$SCRIPT_DIR/network_partition/create-partition-plot.sh" ]; then
            if "$SCRIPT_DIR/network_partition/create-partition-plot.sh" -a "$ALGORITHM" 2>/dev/null; then
                echo "  [OK] Partition plots generated"
                GENERATED=$((GENERATED + 1))
            else
                echo "  [SKIP] No partition data found for $ALGORITHM"
                SKIPPED=$((SKIPPED + 1))
            fi
        else
            echo "  [ERROR] Script not found or not executable"
            FAILED=$((FAILED + 1))
        fi
        echo ""
    fi
done

# ============================================================
# Cross-Algorithm Comparison (if running all algorithms)
# ============================================================
if [ "$ALGORITHMS" = "all" ]; then
    echo ""
    echo "=============================================================="
    echo " Cross-Algorithm Comparison"
    echo "=============================================================="
    if [ -f "$SCRIPT_DIR/comparison/compare_algorithms.py" ]; then
        if python3 "$SCRIPT_DIR/comparison/compare_algorithms.py" 2>/dev/null; then
            echo "  [OK] Comparison plots generated"
            GENERATED=$((GENERATED + 1))
        else
            echo "  [SKIP] Insufficient data for comparison"
            SKIPPED=$((SKIPPED + 1))
        fi
    else
        echo "  [SKIP] compare_algorithms.py not found"
        SKIPPED=$((SKIPPED + 1))
    fi
    echo ""
fi

# ============================================================
# Summary
# ============================================================
echo "=============================================================="
echo " Plot Generation Complete"
echo "=============================================================="
echo "Generated: $GENERATED"
echo "Skipped:   $SKIPPED"
echo "Failed:    $FAILED"
echo ""
echo "Plot locations:"
for ALGORITHM in "${ALGO_ARRAY[@]}"; do
    echo "  $ALGORITHM:"
    echo "    Convergence:      $PROJECT_DIR/results/$ALGORITHM/convergence_trials/*/convergence_*.png"
    echo "    Fault Tolerance:  $PROJECT_DIR/results/$ALGORITHM/fault_tolerance/*/recovery_*.png"
    echo "    Noise:            $PROJECT_DIR/results/$ALGORITHM/noise/*/noise*.png"
    echo "    Partition:        $PROJECT_DIR/results/$ALGORITHM/network_partition/*/partition_*.png"
done
echo "=============================================================="

exit 0
