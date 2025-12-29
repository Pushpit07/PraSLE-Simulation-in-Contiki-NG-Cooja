#!/bin/bash
#
# Leader Election Algorithm Metrics Collection Script
#
# Simple wrapper to run experiments without typing long commands.
#
# Usage:
#   ./create-metrics.sh <algorithm>                    # Run 60s experiment with default output
#   ./create-metrics.sh <algorithm> 300                # Run 5-minute experiment
#   ./create-metrics.sh <algorithm> 300 my_test        # Run 5-minute experiment with custom name
#
# Algorithms: bully, ring, prasle, adaptive-prasle
#
# Examples:
#   ./create-metrics.sh bully
#   ./create-metrics.sh ring 300
#   ./create-metrics.sh prasle 120 test_prasle
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Valid algorithms
VALID_ALGORITHMS=("bully" "ring" "prasle" "adaptive-prasle")

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# Show usage
show_usage() {
    echo "Usage: $0 <algorithm> [duration] [output_name]"
    echo ""
    echo "Arguments:"
    echo "  algorithm    Algorithm to test: bully, ring, prasle, adaptive-prasle"
    echo "  duration     Duration in seconds (default: 60)"
    echo "  output_name  Output directory name (default: test)"
    echo ""
    echo "Examples:"
    echo "  $0 bully"
    echo "  $0 ring 300"
    echo "  $0 prasle 120 my_test"
}

# Check arguments
if [ $# -lt 1 ]; then
    echo -e "${RED}Error: Algorithm is required${NC}"
    echo ""
    show_usage
    exit 1
fi

ALGORITHM=$1
DURATION=${2:-60}
OUTPUT_NAME=${3:-"test"}

# Validate algorithm
VALID=false
for algo in "${VALID_ALGORITHMS[@]}"; do
    if [ "$algo" == "$ALGORITHM" ]; then
        VALID=true
        break
    fi
done

if [ "$VALID" == "false" ]; then
    echo -e "${RED}Error: Invalid algorithm '$ALGORITHM'${NC}"
    echo "Valid algorithms: ${VALID_ALGORITHMS[*]}"
    exit 1
fi

# Set paths
SIMULATION_FILE="${SCRIPT_DIR}/experiments/convergence/csc_templates/${ALGORITHM}/5nodes.csc"
OUTPUT_DIR="${SCRIPT_DIR}/results/${ALGORITHM}/${OUTPUT_NAME}"

# Check simulation file exists
if [ ! -f "$SIMULATION_FILE" ]; then
    echo -e "${RED}Error: Simulation file not found: $SIMULATION_FILE${NC}"
    echo "Run: python3 scripts/generate_csc.py --algorithm $ALGORITHM --experiment convergence --output experiments/"
    exit 1
fi

echo "=========================================="
echo " Leader Election Metrics Collection"
echo "=========================================="
echo "Algorithm: ${ALGORITHM}"
echo "Duration: ${DURATION}s"
echo "Output: ${OUTPUT_DIR}"
echo ""

# Run the experiment
python3 "${SCRIPT_DIR}/scripts/run_experiment.py" \
    --simulation "${SIMULATION_FILE}" \
    --duration "${DURATION}" \
    --output "${OUTPUT_DIR}" \
    --algorithm "${ALGORITHM}"

# Show results location
echo ""
echo "=========================================="
echo -e "${GREEN} Results saved to:${NC}"
echo "   ${OUTPUT_DIR}"
echo "=========================================="
echo ""
echo "View results:"
echo "  cat ${OUTPUT_DIR}/summary.txt"
echo "  cat ${OUTPUT_DIR}/metrics.csv"
