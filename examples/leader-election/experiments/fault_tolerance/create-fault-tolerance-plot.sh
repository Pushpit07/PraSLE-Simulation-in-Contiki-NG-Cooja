#!/bin/bash
#
# Simplified wrapper script to generate fault tolerance (crash recovery) plots
#
# This script automatically finds the most recent crash recovery trial data
# and generates visualization plots using analyze_recovery.py.
# Supports all algorithms: bully, ring, prasle, adaptive-prasle.
#
# Usage:
#   ./create-fault-tolerance-plot.sh [-a algorithm] [input]
#
# Parameters:
#   -a algorithm : Algorithm type (default: bully)
#   input  : Optional. Can be:
#            - Path to a directory containing crash_recovery_times.csv
#            - Path to a crash_recovery_times.csv file directly
#            - If omitted, uses the most recent trial results
#
# Examples:
#   ./create-fault-tolerance-plot.sh
#   ./create-fault-tolerance-plot.sh -a ring
#   ./create-fault-tolerance-plot.sh -a prasle ../../results/prasle/fault_tolerance/50nodes_crash60s_20231201
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
ANALYZE_SCRIPT="$SCRIPT_DIR/analyze_recovery.py"

# Default algorithm
ALGORITHM="bully"

# Parse algorithm option
while getopts "a:" opt; do
    case $opt in
        a)
            ALGORITHM="$OPTARG"
            ;;
        \?)
            echo "Invalid option: -$OPTARG"
            exit 1
            ;;
    esac
done
shift $((OPTIND-1))

# Validate algorithm
if [[ ! "$ALGORITHM" =~ ^(bully|ring|prasle|adaptive-prasle)$ ]]; then
    echo "Error: Invalid algorithm '$ALGORITHM'"
    echo "Valid algorithms: bully, ring, prasle, adaptive-prasle"
    exit 1
fi

RESULTS_DIR="$PROJECT_DIR/results/$ALGORITHM/fault_tolerance"

# Check if analyze_recovery.py exists
if [ ! -f "$ANALYZE_SCRIPT" ]; then
    echo "Error: analyze_recovery.py not found at: $ANALYZE_SCRIPT"
    exit 1
fi

# Function to find most recent trial directory
find_most_recent_trial() {
    if [ ! -d "$RESULTS_DIR" ]; then
        echo "Error: Results directory not found: $RESULTS_DIR"
        echo "Run some crash recovery trials first using run_crash_trials.sh"
        exit 1
    fi

    # Find most recently modified directory containing crash_recovery_times.csv
    local most_recent=$(find "$RESULTS_DIR" -maxdepth 1 -type d -name "*nodes_crash*" -exec test -f {}/crash_recovery_times.csv \; -print | \
                       xargs ls -dt 2>/dev/null | head -n 1)

    if [ -z "$most_recent" ]; then
        echo "Error: No trial results found in: $RESULTS_DIR"
        echo "Run some crash recovery trials first using run_crash_trials.sh"
        exit 1
    fi

    echo "$most_recent"
}

# Determine input source
INPUT=""
CSV_FILE=""
OUTPUT_DIR=""

if [ $# -eq 0 ]; then
    # No input provided - auto-detect most recent trial
    echo "No input provided. Auto-detecting most recent $ALGORITHM trial results..."
    TRIAL_DIR=$(find_most_recent_trial)
    CSV_FILE="$TRIAL_DIR/crash_recovery_times.csv"
    OUTPUT_DIR="$TRIAL_DIR"
    echo "Using: $TRIAL_DIR"
elif [ -d "$1" ]; then
    # Input is a directory
    TRIAL_DIR="$1"
    CSV_FILE="$TRIAL_DIR/crash_recovery_times.csv"
    OUTPUT_DIR="$TRIAL_DIR"

    if [ ! -f "$CSV_FILE" ]; then
        echo "Error: crash_recovery_times.csv not found in directory: $TRIAL_DIR"
        exit 1
    fi
elif [ -f "$1" ]; then
    # Input is a file
    CSV_FILE="$1"
    OUTPUT_DIR="$(dirname "$CSV_FILE")"

    # Verify it's a CSV file
    if [[ ! "$CSV_FILE" =~ \.csv$ ]]; then
        echo "Warning: Input file does not have .csv extension: $CSV_FILE"
    fi
else
    echo "Error: Input not found: $1"
    echo ""
    echo "Usage: $0 [-a algorithm] [input]"
    echo ""
    echo "Parameters:"
    echo "  -a algorithm : Algorithm type (default: bully)"
    echo "  input  : Optional. Can be:"
    echo "           - Path to a directory containing crash_recovery_times.csv"
    echo "           - Path to a crash_recovery_times.csv file directly"
    echo "           - If omitted, uses the most recent trial results"
    exit 1
fi

# Verify CSV file exists
if [ ! -f "$CSV_FILE" ]; then
    echo "Error: CSV file not found: $CSV_FILE"
    exit 1
fi

# Extract node count from filename or directory for informational purposes
NODE_COUNT=""
if [[ "$(basename $(dirname "$CSV_FILE"))" =~ ([0-9]+)nodes ]]; then
    NODE_COUNT="${BASH_REMATCH[1]}"
elif [[ "$(basename "$CSV_FILE")" =~ ([0-9]+)nodes ]]; then
    NODE_COUNT="${BASH_REMATCH[1]}"
fi

# Display information
ALGORITHM_UPPER=$(echo "$ALGORITHM" | tr '[:lower:]' '[:upper:]')
echo ""
echo "========================================================"
echo " Generating Crash Recovery Analysis Plots - $ALGORITHM_UPPER"
echo "========================================================"
echo "Algorithm:     $ALGORITHM"
echo "Input CSV:     $CSV_FILE"
echo "Output dir:    $OUTPUT_DIR"
if [ -n "$NODE_COUNT" ]; then
    echo "Node count:    $NODE_COUNT"
fi
echo "========================================================"
echo ""

# Run analysis script
python3 "$ANALYZE_SCRIPT" \
    --input "$CSV_FILE" \
    --output "$OUTPUT_DIR" \
    --algorithm "$ALGORITHM"

echo ""
echo "Done! Plots saved to: $OUTPUT_DIR"
echo ""

# List generated files
if [ -d "$OUTPUT_DIR" ]; then
    echo "Generated files:"
    ls -lh "$OUTPUT_DIR"/*.png "$OUTPUT_DIR"/recovery_summary.csv 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
fi

echo ""

exit 0
