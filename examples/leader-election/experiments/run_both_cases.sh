#!/bin/bash
# ============================================================
# Run Complete Evaluation for Both Parameter Cases
# ============================================================
#
# This script runs the complete leader election algorithm evaluation
# for both parameter cases:
#
#   Case 1: Original paper parameters (each algorithm uses its published defaults)
#   Case 2: Standardized parameters (identical timing for fair cross-algorithm comparison)
#
# Usage:
#   ./run_both_cases.sh [options]
#
# All options are passed through to run_complete_evaluation.sh
#
# Examples:
#   ./run_both_cases.sh                           # Run full evaluation for both cases
#   ./run_both_cases.sh -t 10 -n 5,10             # Quick test: 10 trials, 5 and 10 nodes
#   ./run_both_cases.sh --algorithms bully,prasle # Only test bully and prasle
#
# ============================================================

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Activate Python virtual environment if available
if [ -f "$PROJECT_DIR/.venv/bin/activate" ]; then
    source "$PROJECT_DIR/.venv/bin/activate"
fi

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}Running Complete Evaluation for Both Parameter Cases${NC}"
echo -e "${BLUE}============================================================${NC}"
echo ""
echo "Arguments passed through: $@"
echo ""

# Run Case 1: Original paper parameters
echo -e "${GREEN}============================================================${NC}"
echo -e "${GREEN}CASE 1: Original Paper Parameters${NC}"
echo -e "${GREEN}============================================================${NC}"
echo ""
echo "Each algorithm uses its published default parameters:"
echo "  - Bully/Ring: ALIVE_INTERVAL=2s, COORDINATOR_TIMEOUT=4s"
echo "  - PraSLE/Adaptive-PraSLE: T=1.0s"
echo ""

PARAM_CASE=1 "$SCRIPT_DIR/run_complete_evaluation.sh" "$@"

CASE1_STATUS=$?

if [ $CASE1_STATUS -ne 0 ]; then
    echo -e "${YELLOW}[WARN]${NC} Case 1 completed with non-zero exit code: $CASE1_STATUS"
fi

echo ""
echo -e "${GREEN}============================================================${NC}"
echo -e "${GREEN}CASE 2: Standardized Parameters${NC}"
echo -e "${GREEN}============================================================${NC}"
echo ""
echo "All algorithms use identical timing parameters for fair comparison:"
echo "  - All algorithms: Heartbeat/T=2.0s, Startup delay max=2.0s"
echo "  - This enables apples-to-apples message overhead comparison"
echo ""

PARAM_CASE=2 "$SCRIPT_DIR/run_complete_evaluation.sh" "$@"

CASE2_STATUS=$?

if [ $CASE2_STATUS -ne 0 ]; then
    echo -e "${YELLOW}[WARN]${NC} Case 2 completed with non-zero exit code: $CASE2_STATUS"
fi

# Summary
echo ""
echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}Both Cases Completed${NC}"
echo -e "${BLUE}============================================================${NC}"
echo ""
echo "Results are organized in separate directories:"
echo "  Case 1 (Original):     results/case1/<algorithm>/<experiment>/"
echo "  Case 2 (Standardized): results/case2/<algorithm>/<experiment>/"
echo ""
echo "Comparison charts are in:"
echo "  Case 1 charts: results/case1/comparison_charts/"
echo "  Case 2 charts: results/case2/comparison_charts/"
echo ""

if [ $CASE1_STATUS -eq 0 ] && [ $CASE2_STATUS -eq 0 ]; then
    echo -e "${GREEN}[SUCCESS]${NC} Both cases completed successfully!"
    exit 0
else
    echo -e "${YELLOW}[WARN]${NC} One or both cases had issues. Check logs above."
    exit 1
fi
