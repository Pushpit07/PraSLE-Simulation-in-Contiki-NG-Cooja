#!/bin/bash
#
# Simplified wrapper script to generate network noise analysis plots
#
# This script automatically finds the most recent noise trial data
# and generates visualization plots using analyze_noise.py.
#
# Usage:
#   ./create-noise-plot.sh [input]
#
# Parameters:
#   input  : Optional. Can be:
#            - Path to a directory containing noise_convergence_times.csv
#            - Path to a noise_convergence_times.csv file directly
#            - If omitted, uses the most recent trial results
#
# Examples:
#   ./create-noise-plot.sh
#   ./create-noise-plot.sh ../../results/noise/50nodes_noise90_20231201_120000
#   ./create-noise-plot.sh ../../results/noise/50nodes_noise90_20231201_120000/noise_convergence_times.csv
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BULLY_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
RESULTS_DIR="$BULLY_DIR/results/noise"
ANALYZE_SCRIPT="$SCRIPT_DIR/analyze_noise.py"

# Check if analyze_noise.py exists
if [ ! -f "$ANALYZE_SCRIPT" ]; then
    echo "Error: analyze_noise.py not found at: $ANALYZE_SCRIPT"
    exit 1
fi

# Function to find most recent noise trial directory
find_most_recent_trial() {
    if [ ! -d "$RESULTS_DIR" ]; then
        echo "Error: Results directory not found: $RESULTS_DIR"
        echo "Run some noise trials first using run_noise_trials.sh"
        exit 1
    fi

    # Find most recently modified directory containing noise_convergence_times.csv
    local most_recent=$(find "$RESULTS_DIR" -maxdepth 1 -type d -name "*nodes_noise*" -exec test -f {}/noise_convergence_times.csv \; -print | \
                       xargs ls -dt 2>/dev/null | head -n 1)

    if [ -z "$most_recent" ]; then
        echo "Error: No noise trial results found in: $RESULTS_DIR"
        echo "Run some noise trials first using run_noise_trials.sh"
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
    echo "No input provided. Auto-detecting most recent noise trial results..."
    TRIAL_DIR=$(find_most_recent_trial)
    CSV_FILE="$TRIAL_DIR/noise_convergence_times.csv"
    OUTPUT_DIR="$TRIAL_DIR"
    echo "Using: $TRIAL_DIR"
elif [ -d "$1" ]; then
    # Input is a directory
    TRIAL_DIR="$1"
    CSV_FILE="$TRIAL_DIR/noise_convergence_times.csv"
    OUTPUT_DIR="$TRIAL_DIR"

    if [ ! -f "$CSV_FILE" ]; then
        echo "Error: noise_convergence_times.csv not found in directory: $TRIAL_DIR"
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
    echo "Usage: $0 [input]"
    echo ""
    echo "Parameters:"
    echo "  input  : Optional. Can be:"
    echo "           - Path to a directory containing noise_convergence_times.csv"
    echo "           - Path to a noise_convergence_times.csv file directly"
    echo "           - If omitted, uses the most recent trial results"
    exit 1
fi

# Verify CSV file exists
if [ ! -f "$CSV_FILE" ]; then
    echo "Error: CSV file not found: $CSV_FILE"
    exit 1
fi

# Extract node count and noise level from directory name
NODE_COUNT=""
NOISE_LEVEL=""
if [[ "$(basename $(dirname "$CSV_FILE"))" =~ ([0-9]+)nodes_noise([0-9]+) ]]; then
    NODE_COUNT="${BASH_REMATCH[1]}"
    NOISE_LEVEL="${BASH_REMATCH[2]}"
fi

# Display information
echo ""
echo "========================================================"
echo " Generating Network Noise Analysis Plots"
echo "========================================================"
echo "Input CSV:     $CSV_FILE"
echo "Output dir:    $OUTPUT_DIR"
if [ -n "$NODE_COUNT" ]; then
    echo "Node count:    $NODE_COUNT"
fi
if [ -n "$NOISE_LEVEL" ]; then
    echo "Noise level:   ${NOISE_LEVEL}% success rate"
fi
echo "========================================================"
echo ""

# Run analysis script
python3 "$ANALYZE_SCRIPT" \
    --input "$CSV_FILE" \
    --output "$OUTPUT_DIR"

echo ""
echo "Done! Plots saved to: $OUTPUT_DIR"
echo ""

# List generated files
if [ -d "$OUTPUT_DIR" ]; then
    echo "Generated files:"
    ls -lh "$OUTPUT_DIR"/*.png "$OUTPUT_DIR"/noise_summary.csv 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
fi

echo ""

exit 0
