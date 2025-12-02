#!/bin/bash
#
# Bully Algorithm Metrics Collection Script
#
# Simple wrapper to run experiments without typing long commands.
#
# Usage:
#   ./create-metrics.sh                    # Run 60s experiment with default output
#   ./create-metrics.sh 300                # Run 5-minute experiment
#   ./create-metrics.sh 300 my_test        # Run 5-minute experiment with custom name
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIMULATION_FILE="${SCRIPT_DIR}/bully-cooja.csc"

# Parse arguments
DURATION=${1:-60}  # Default: 60 seconds
# OUTPUT_NAME=${2:-"test_$(date +%Y%m%d_%H%M%S)"}  # Default: test_YYYYMMDD_HHMMSS
OUTPUT_NAME=${2:-"test"}
OUTPUT_DIR="${SCRIPT_DIR}/results/${OUTPUT_NAME}"

echo "=========================================="
echo " Bully Algorithm Metrics Collection"
echo "=========================================="
echo "Duration: ${DURATION}s"
echo "Output: ${OUTPUT_DIR}"
echo ""

# Run the experiment
python3 "${SCRIPT_DIR}/scripts/run_experiment.py" \
    --simulation "${SIMULATION_FILE}" \
    --duration "${DURATION}" \
    --output "${OUTPUT_DIR}"

# Show results location
echo ""
echo "=========================================="
echo " Results saved to:"
echo "   ${OUTPUT_DIR}"
echo "=========================================="
echo ""
echo "View results:"
echo "  cat ${OUTPUT_DIR}/summary.txt"
echo "  cat ${OUTPUT_DIR}/metrics.csv"
