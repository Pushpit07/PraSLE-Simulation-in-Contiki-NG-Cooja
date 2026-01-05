#!/bin/bash
# Adaptive PraSLE Algorithm - Full Experiment Pipeline
#
# Custom variant of PraSLE using NullNet (link-layer broadcast)
# instead of IPv6/UDP for direct neighbor communication.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

echo "=== Cleaning Previous Results ==="
rm -rf results/adaptive-prasle/

echo "=== Building Adaptive PraSLE Algorithm ==="
make clean
make ALGORITHM=adaptive-prasle TARGET=cooja

echo "=== Running All Experiments ==="
cd experiments
./run_all_experiments.sh --algorithm adaptive-prasle --experiments convergence,fault_tolerance,noise,network_partition

echo "=== Analyzing Results ==="
./analyze.sh adaptive-prasle

echo "=== Generating Plots ==="
./generate_all_plots.sh --algorithm adaptive-prasle

echo "=== Done! ==="
