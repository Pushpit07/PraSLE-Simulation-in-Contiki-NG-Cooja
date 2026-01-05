#!/bin/bash
# PraSLE Algorithm - Full Experiment Pipeline
#
# Based on "A Practical Self-Stabilizing Leader Election for
# Networks of Resource-Constrained IoT Devices" (Conard & Ebnenasir, 2021)
#
# Usage:
#   ./prasle.sh                    # Run all experiments with defaults
#   ./prasle.sh --quick            # Quick test (5 nodes, 10 trials)
#   ./prasle.sh --k 5 --t 0.5      # Custom K and T parameters
#
# Options:
#   --quick           Quick test mode (5 nodes, 10 trials, convergence only)
#   --k <rounds>      Set PRASLE_K_ROUNDS (default: 10)
#   --t <seconds>     Set PRASLE_T_SECONDS (default: 1.0)
#   --nodes <list>    Comma-separated node counts (default: 5,10,50,100)
#   --trials <n>      Number of trials (default: 10)
#   --experiments <e> Experiments to run (default: all)
#   --skip-build      Skip the build step
#   --skip-clean      Don't clean previous results

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

# Default configuration
PRASLE_K=10
PRASLE_T=1.0
NODE_COUNTS="5,10,50,100"
NUM_TRIALS=10
EXPERIMENTS="convergence,fault_tolerance,noise,network_partition"
SKIP_BUILD=false
SKIP_CLEAN=false
QUICK_MODE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --quick)
            QUICK_MODE=true
            NODE_COUNTS="5"
            NUM_TRIALS=10
            EXPERIMENTS="convergence"
            shift
            ;;
        --k)
            PRASLE_K="$2"
            shift 2
            ;;
        --t)
            PRASLE_T="$2"
            shift 2
            ;;
        --nodes)
            NODE_COUNTS="$2"
            shift 2
            ;;
        --trials)
            NUM_TRIALS="$2"
            shift 2
            ;;
        --experiments)
            EXPERIMENTS="$2"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --skip-clean)
            SKIP_CLEAN=true
            shift
            ;;
        --help|-h)
            echo "PraSLE Algorithm - Full Experiment Pipeline"
            echo ""
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --quick           Quick test (5 nodes, 10 trials, convergence only)"
            echo "  --k <rounds>      Set PRASLE_K_ROUNDS (default: 10)"
            echo "  --t <seconds>     Set PRASLE_T_SECONDS (default: 1.0)"
            echo "  --nodes <list>    Node counts, comma-separated (default: 5,10,50,100)"
            echo "  --trials <n>      Number of trials (default: 10)"
            echo "  --experiments <e> Experiments to run (default: all)"
            echo "  --skip-build      Skip the build step"
            echo "  --skip-clean      Don't clean previous results"
            echo ""
            echo "Examples:"
            echo "  $0                           # Full experiment suite"
            echo "  $0 --quick                   # Quick test"
            echo "  $0 --k 5 --t 0.5 --nodes 10  # Custom parameters"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "============================================================"
echo "PraSLE Algorithm - Experiment Pipeline"
echo "============================================================"
echo "Parameters:"
echo "  K (rounds):    $PRASLE_K"
echo "  T (seconds):   $PRASLE_T"
echo "  Node counts:   $NODE_COUNTS"
echo "  Trials:        $NUM_TRIALS"
echo "  Experiments:   $EXPERIMENTS"
echo "  Quick mode:    $QUICK_MODE"
echo "============================================================"
echo ""

# Clean previous results
if [[ "$SKIP_CLEAN" != "true" ]]; then
    echo "=== Cleaning Previous Results ==="
    rm -rf results/prasle/
fi

# Build PraSLE
if [[ "$SKIP_BUILD" != "true" ]]; then
    echo "=== Building PraSLE Algorithm ==="
    echo "Using K=$PRASLE_K rounds, T=$PRASLE_T seconds"
    make clean
    make ALGORITHM=prasle TARGET=cooja \
         CFLAGS+="-DPRASLE_K_ROUNDS=$PRASLE_K -DPRASLE_T_SECONDS=$PRASLE_T"
fi

# Run experiments
echo "=== Running Experiments ==="
cd experiments
./run_all_experiments.sh \
    --algorithm prasle \
    --experiments "$EXPERIMENTS" \
    --trials "$NUM_TRIALS" \
    --nodes "$NODE_COUNTS"

# Analyze results
echo "=== Analyzing Results ==="
./analyze.sh prasle

# Generate plots
echo "=== Generating Plots ==="
./generate_all_plots.sh --algorithm prasle

echo ""
echo "============================================================"
echo "=== PraSLE Experiments Complete! ==="
echo "============================================================"
echo "Results saved to: $PROJECT_DIR/results/prasle/"
echo ""
