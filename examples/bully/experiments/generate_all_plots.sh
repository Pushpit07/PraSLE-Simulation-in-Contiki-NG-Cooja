#!/bin/bash
#
# Generate All Experiment Plots
#
# This script generates visualization plots for all experiment results.
# It auto-detects the most recent results for each experiment type.
#
# Usage:
#   ./generate_all_plots.sh [options]
#
# Options:
#   --experiments E  Comma-separated list of experiments:
#                    convergence, fault_tolerance, noise, partition, all (default: all)
#   --help           Show this help message
#
# Examples:
#   ./generate_all_plots.sh                           # Generate all plots
#   ./generate_all_plots.sh --experiments convergence # Only convergence plots
#

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BULLY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default values
EXPERIMENTS="all"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --experiments)
            EXPERIMENTS="$2"
            shift 2
            ;;
        --help|-h)
            head -20 "$0" | tail -18
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Convert comma-separated list to array
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
echo ""

GENERATED=0
FAILED=0

# ============================================================
# Convergence Plots
# ============================================================
if should_run "convergence"; then
    echo "--- Convergence Plots ---"
    if [ -x "$SCRIPT_DIR/convergence/create-convergence-plot.sh" ]; then
        if "$SCRIPT_DIR/convergence/create-convergence-plot.sh" 2>/dev/null; then
            echo "  [OK] Convergence plots generated"
            GENERATED=$((GENERATED + 1))
        else
            echo "  [SKIP] No convergence data found"
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
        if "$SCRIPT_DIR/fault_tolerance/create-fault-tolerance-plot.sh" 2>/dev/null; then
            echo "  [OK] Fault tolerance plots generated"
            GENERATED=$((GENERATED + 1))
        else
            echo "  [SKIP] No fault tolerance data found"
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
        if "$SCRIPT_DIR/noise/create-noise-plot.sh" 2>/dev/null; then
            echo "  [OK] Noise plots generated"
            GENERATED=$((GENERATED + 1))
        else
            echo "  [SKIP] No noise data found"
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
        if "$SCRIPT_DIR/network_partition/create-partition-plot.sh" 2>/dev/null; then
            echo "  [OK] Partition plots generated"
            GENERATED=$((GENERATED + 1))
        else
            echo "  [SKIP] No partition data found"
        fi
    else
        echo "  [ERROR] Script not found or not executable"
        FAILED=$((FAILED + 1))
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
echo "Failed:    $FAILED"
echo ""
echo "Plot locations:"
echo "  Convergence:      $BULLY_DIR/results/convergence_trials/*/convergence_*.png"
echo "  Fault Tolerance:  $BULLY_DIR/results/fault_tolerance/*/recovery_*.png"
echo "  Noise:            $BULLY_DIR/results/noise/*/noise*.png"
echo "  Partition:        $BULLY_DIR/results/network_partition/*/partition_*.png"
echo "=============================================================="

exit 0
