#!/bin/bash
# Ring Algorithm - Full Experiment Pipeline

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

echo "=== Cleaning Previous Results ==="
rm -rf results/ring/

echo "=== Building Ring Algorithm ==="
make clean
make ALGORITHM=ring TARGET=cooja

echo "=== Running All Experiments ==="
cd experiments
./run_all_experiments.sh --algorithm ring --experiments convergence,fault_tolerance,noise,network_partition

echo "=== Analyzing Results ==="
./analyze.sh ring

echo "=== Generating Plots ==="
./generate_all_plots.sh --algorithm ring

echo "=== Done! ==="
