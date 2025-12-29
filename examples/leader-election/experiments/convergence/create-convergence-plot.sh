#!/bin/bash
#
# Create Convergence Distribution Plot
#
# Simple wrapper to generate convergence time distribution plots from trial data.
# Auto-detects the most recent convergence trials directory if no path is provided.
# Supports all algorithms: bully, ring, prasle, adaptive-prasle.
#
# Usage:
#   ./create-convergence-plot.sh [-a algorithm] [csv_file_or_directory] [output_file]
#
# Examples:
#   ./create-convergence-plot.sh                                              # Use most recent trials (bully)
#   ./create-convergence-plot.sh -a ring                                      # Use most recent ring trials
#   ./create-convergence-plot.sh results/bully/convergence/50nodes_20251202
#   ./create-convergence-plot.sh -a prasle results/prasle/convergence/50nodes_20251202
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

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

TRIALS_DIR="$PROJECT_DIR/results/$ALGORITHM/convergence"

# Auto-detect most recent trials directory if no input provided
if [ $# -lt 1 ]; then
    # Look for most recent convergence trials directory
    if [ -d "$TRIALS_DIR" ]; then
        # Find most recently modified directory
        LATEST_DIR=$(find "$TRIALS_DIR" -maxdepth 1 -type d -name "*nodes_*" -print0 2>/dev/null | xargs -0 ls -td 2>/dev/null | head -1)

        if [ -n "$LATEST_DIR" ]; then
            INPUT_PATH="$LATEST_DIR"
            echo "Auto-detected most recent trials directory for $ALGORITHM:"
            echo "  $(basename "$INPUT_PATH")"
            echo ""
        else
            echo "Error: No convergence trials directories found in $TRIALS_DIR"
            echo ""
            echo "Usage: $0 [-a algorithm] [csv_file_or_directory] [output_file]"
            echo ""
            echo "Examples:"
            echo "  $0                                              # Use most recent trials (bully)"
            echo "  $0 -a ring                                      # Use most recent ring trials"
            echo "  $0 results/$ALGORITHM/convergence/50nodes_20251202"
            exit 1
        fi
    else
        echo "Error: Convergence trials directory not found: $TRIALS_DIR"
        echo ""
        echo "Please run convergence trials first:"
        echo "  ./run_convergence.sh $ALGORITHM 50"
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
        OUTPUT_FILE="convergence_${NODE_COUNT}nodes_${ALGORITHM}.png"
    else
        OUTPUT_FILE="convergence_distribution_${ALGORITHM}.png"
    fi
fi

echo "=========================================="
echo " Convergence Distribution Plot - ${ALGORITHM^^}"
echo "=========================================="
echo "Algorithm:  $ALGORITHM"
echo "Input CSV:  $CSV_FILE"
echo "Output PNG: $OUTPUT_FILE"
echo ""

# Run the plotting script
python3 "$PROJECT_DIR/scripts/plot_convergence_distribution.py" \
    -i "$CSV_FILE" \
    -a "$ALGORITHM" \
    -o "$OUTPUT_FILE"

echo ""
echo "=========================================="
echo " Plot saved to: $OUTPUT_FILE"
echo "=========================================="
