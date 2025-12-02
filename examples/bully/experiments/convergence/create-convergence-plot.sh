#!/bin/bash
#
# Create Convergence Distribution Plot
#
# Simple wrapper to generate convergence time distribution plots from trial data.
# Auto-detects the most recent convergence trials directory if no path is provided.
#
# Usage:
#   ./create-convergence-plot.sh [csv_file_or_directory] [output_file]
#
# Examples:
#   ./create-convergence-plot.sh                                              # Use most recent trials
#   ./create-convergence-plot.sh results/convergence_trials/50nodes_20251202_022841
#   ./create-convergence-plot.sh results/convergence_trials/50nodes_20251202_022841/convergence_times.csv
#   ./create-convergence-plot.sh results/convergence_trials/50nodes_20251202_022841 my_plot.png
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BULLY_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
TRIALS_DIR="$BULLY_DIR/results/convergence_trials"

# Auto-detect most recent trials directory if no input provided
if [ $# -lt 1 ]; then
    # Look for most recent convergence trials directory
    if [ -d "$TRIALS_DIR" ]; then
        # Find most recently modified directory
        LATEST_DIR=$(find "$TRIALS_DIR" -maxdepth 1 -type d -name "*nodes_*" -print0 | xargs -0 ls -td | head -1)

        if [ -n "$LATEST_DIR" ]; then
            INPUT_PATH="$LATEST_DIR"
            echo "Auto-detected most recent trials directory:"
            echo "  $(basename "$INPUT_PATH")"
            echo ""
        else
            echo "Error: No convergence trials directories found in $TRIALS_DIR"
            echo ""
            echo "Usage: $0 [csv_file_or_directory] [output_file]"
            echo ""
            echo "Examples:"
            echo "  $0                                              # Use most recent trials"
            echo "  $0 results/convergence_trials/50nodes_20251202_022841"
            echo "  $0 results/convergence_trials/50nodes_20251202_022841/convergence_times.csv"
            echo "  $0 results/convergence_trials/50nodes_20251202_022841 my_plot.png"
            exit 1
        fi
    else
        echo "Error: Convergence trials directory not found: $TRIALS_DIR"
        echo ""
        echo "Please run convergence trials first:"
        echo "  ./run_convergence_trials.sh 50"
        exit 1
    fi
else
    INPUT_PATH="$1"
fi

# Determine CSV file path
if [ -d "$INPUT_PATH" ]; then
    # Input is a directory, look for convergence_times.csv inside
    CSV_FILE="$INPUT_PATH/convergence_times.csv"
elif [ -f "$INPUT_PATH" ]; then
    # Input is a file
    CSV_FILE="$INPUT_PATH"
else
    echo "Error: Input path not found: $INPUT_PATH"
    exit 1
fi

# Check if CSV file exists
if [ ! -f "$CSV_FILE" ]; then
    echo "Error: CSV file not found: $CSV_FILE"
    exit 1
fi

# Determine output file
if [ $# -ge 2 ]; then
    # User provided output file
    OUTPUT_FILE="$2"
else
    # Auto-generate output filename based on directory name
    DIR_NAME=$(basename "$(dirname "$CSV_FILE")")
    # Extract node count from directory name (e.g., "50nodes_20251202_022841" -> "50")
    NODE_COUNT=$(echo "$DIR_NAME" | grep -o '^[0-9]*')
    if [ -n "$NODE_COUNT" ]; then
        OUTPUT_FILE="convergence_${NODE_COUNT}nodes.png"
    else
        OUTPUT_FILE="convergence_distribution.png"
    fi
fi

echo "=========================================="
echo " Convergence Distribution Plot"
echo "=========================================="
echo "Input CSV:  $CSV_FILE"
echo "Output PNG: $OUTPUT_FILE"
echo ""

# Run the plotting script
python3 "$BULLY_DIR/scripts/plot_convergence_distribution.py" \
    -i "$CSV_FILE" \
    -o "$OUTPUT_FILE"

echo ""
echo "=========================================="
echo " Plot saved to: $OUTPUT_FILE"
echo "=========================================="
