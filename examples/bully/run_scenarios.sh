#!/bin/bash
#
# Bully Algorithm Scenario Runner
#
# This script runs multiple experiment scenarios to collect baseline
# metrics for the Bully leader election algorithm across different
# node counts and durations.
#
# Usage:
#   ./run_scenarios.sh [output_base_dir]
#
# Examples:
#   ./run_scenarios.sh                                    # Uses default: results/baseline_YYYYMMDD_HHMMSS/
#   ./run_scenarios.sh results/my_experiment             # Custom directory in bully/results/
#   ./run_scenarios.sh /absolute/path/to/output          # Absolute path
#

set -e  # Exit on error

# Configuration
BULLY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="$BULLY_DIR/scripts"
OUTPUT_BASE="${1:-$BULLY_DIR/results/baseline_$(date +%Y%m%d_%H%M%S)}"

# Node count configurations
NODE_COUNTS=(5 10 50 100)

# Duration configurations (in seconds)
DURATIONS_LABELS=("5min" "15min" "30min")
DURATIONS_VALUES=(300 900 1800)

# Create output directory
mkdir -p "$OUTPUT_BASE"

echo "=============================================="
echo " Bully Algorithm Multi-Node Scenario Runner"
echo "=============================================="
echo "Node count configurations: ${NODE_COUNTS[@]}"
echo "Duration configurations: 5min (300s), 15min (900s), 30min (1800s)"
echo "Total experiments: $((${#NODE_COUNTS[@]} * ${#DURATIONS_LABELS[@]}))"
echo "Output directory: $OUTPUT_BASE"
echo ""

# Track experiment count
experiment_num=0
total_experiments=$((${#NODE_COUNTS[@]} * ${#DURATIONS_LABELS[@]}))

# Run experiments for each node count and duration combination
for nodes in "${NODE_COUNTS[@]}"; do
    SIMULATION_FILE="$BULLY_DIR/bully-cooja-${nodes}nodes.csc"

    # Check if simulation file exists
    if [ ! -f "$SIMULATION_FILE" ]; then
        echo "[ERROR] Simulation file not found: $SIMULATION_FILE"
        exit 1
    fi

    for i in "${!DURATIONS_LABELS[@]}"; do
        duration_label="${DURATIONS_LABELS[$i]}"
        duration_seconds="${DURATIONS_VALUES[$i]}"
        experiment_num=$((experiment_num + 1))

        OUTPUT_DIR="$OUTPUT_BASE/${nodes}nodes_${duration_label}"

        echo "=============================================="
        echo " Experiment $experiment_num/$total_experiments"
        echo "=============================================="
        echo "Node count: $nodes nodes"
        echo "Duration: $duration_label ($duration_seconds seconds)"
        echo "Simulation file: $SIMULATION_FILE"
        echo "Output directory: $OUTPUT_DIR"
        echo ""

        python3 "$SCRIPTS_DIR/run_experiment.py" \
            --simulation "$SIMULATION_FILE" \
            --duration "$duration_seconds" \
            --output "$OUTPUT_DIR"

        echo ""
        echo "Experiment $experiment_num completed!"
        echo ""
    done
done

# Generate aggregate analysis
echo "=============================================="
echo " Generating Aggregate Analysis"
echo "=============================================="

ANALYSIS_DIR="$OUTPUT_BASE/analysis"
mkdir -p "$ANALYSIS_DIR"

# Analyze each experiment
for experiment_dir in "$OUTPUT_BASE"/*nodes_*min; do
    if [ -d "$experiment_dir" ]; then
        experiment_name=$(basename "$experiment_dir")
        echo "Analyzing $experiment_name..."

        if [ -f "$experiment_dir/metrics.csv" ]; then
            python3 "$SCRIPTS_DIR/parse_metrics.py" \
                "$experiment_dir/metrics.csv" \
                --output "$ANALYSIS_DIR/$experiment_name"

            # Generate plots
            python3 "$SCRIPTS_DIR/plot_results.py" \
                "$experiment_dir/metrics.csv" \
                --output "$ANALYSIS_DIR/$experiment_name/plots"
        else
            echo "[WARNING] No metrics file found for $experiment_name"
        fi
    fi
done

# Create summary report
echo "=============================================="
echo " Creating Summary Report"
echo "=============================================="

SUMMARY_FILE="$OUTPUT_BASE/SUMMARY_REPORT.txt"

cat > "$SUMMARY_FILE" << EOF
================================================================================
BULLY ALGORITHM MULTI-NODE EXPERIMENTS SUMMARY
================================================================================

Experiment Date: $(date +"%Y-%m-%d %H:%M:%S")
Output Directory: $OUTPUT_BASE

================================================================================
EXPERIMENT CONFIGURATIONS
================================================================================

Node Counts: ${NODE_COUNTS[@]}
Durations: 5min (300s), 15min (900s), 30min (1800s)
Total Experiments: ${#NODE_COUNTS[@]} node counts × ${#DURATIONS[@]} durations = $((${#NODE_COUNTS[@]} * ${#DURATIONS[@]})) experiments

================================================================================
EXPERIMENTS EXECUTED
================================================================================
EOF

# Add each experiment to the summary
for nodes in "${NODE_COUNTS[@]}"; do
    echo "" >> "$SUMMARY_FILE"
    echo "Node Count: $nodes nodes" >> "$SUMMARY_FILE"
    echo "----------------------------" >> "$SUMMARY_FILE"
    for i in "${!DURATIONS_LABELS[@]}"; do
        duration_label="${DURATIONS_LABELS[$i]}"
        duration_seconds="${DURATIONS_VALUES[$i]}"
        echo "  - ${nodes}nodes_${duration_label}/ : ${duration_seconds} seconds" >> "$SUMMARY_FILE"
    done
done

cat >> "$SUMMARY_FILE" << EOF

================================================================================
ANALYSIS OUTPUTS
================================================================================

Each experiment directory (e.g., 5nodes_5min/) contains:
  - cooja_output.log  : Raw Cooja simulation output
  - metrics.csv       : Extracted metrics in CSV format
  - summary.txt       : Automated summary statistics

Analysis directory ($ANALYSIS_DIR/) contains:
  - Per-experiment analysis CSV files
  - Visualization plots (convergence, messages, elections, etc.)
  - Comparative analysis across node counts and durations

================================================================================
METRICS COLLECTED
================================================================================

1. Convergence Metrics
   - Time to first leader election (cold start)
   - Convergence latency per node

2. Communication Overhead
   - Total messages sent/received
   - Messages by type (ELECTION, ANSWER, COORDINATOR, ALIVE)
   - Message counts per node

3. Election Stability
   - Number of elections started
   - Elections won/lost per node
   - Leader changes over time

4. State Distribution
   - Time in NORMAL state
   - Time in ELECTION state
   - Time in WAITING_COORDINATOR state

5. Fault Tolerance
   - Coordinator timeouts detected
   - ALIVE message resets
   - Partition healing events

================================================================================
NEXT STEPS
================================================================================

1. Review individual experiment summaries in each directory (e.g., 5nodes_5min/)

2. Examine visualization plots in:
   $ANALYSIS_DIR/*/plots/

3. Compare metrics across different node counts to analyze scalability

4. Compare metrics across different durations to observe stability over time

5. Use these baseline results to:
   - Understand how the bully algorithm scales with network size
   - Identify performance characteristics at different node counts
   - Compare against other leader election algorithms

================================================================================
DIRECTORY STRUCTURE
================================================================================

$OUTPUT_BASE/
├── 5nodes_5min/          # 5 nodes, 5 minute experiments
├── 5nodes_15min/         # 5 nodes, 15 minute experiments
├── 5nodes_30min/         # 5 nodes, 30 minute experiments
├── 10nodes_5min/         # 10 nodes, 5 minute experiments
├── 10nodes_15min/        # 10 nodes, 15 minute experiments
├── 10nodes_30min/        # 10 nodes, 30 minute experiments
├── 50nodes_5min/         # 50 nodes, 5 minute experiments
├── 50nodes_15min/        # 50 nodes, 15 minute experiments
├── 50nodes_30min/        # 50 nodes, 30 minute experiments
├── 100nodes_5min/        # 100 nodes, 5 minute experiments
├── 100nodes_15min/       # 100 nodes, 15 minute experiments
├── 100nodes_30min/       # 100 nodes, 30 minute experiments
├── analysis/             # Aggregated analysis and plots
└── SUMMARY_REPORT.txt    # This file

================================================================================
EOF

cat "$SUMMARY_FILE"

echo ""
echo "=============================================="
echo " ALL EXPERIMENTS COMPLETED!"
echo "=============================================="
echo "Total experiments run: $total_experiments"
echo "Node counts tested: ${NODE_COUNTS[@]}"
echo "Durations tested: 5min, 15min, 30min"
echo ""
echo "Results saved to: $OUTPUT_BASE"
echo "Summary report: $SUMMARY_FILE"
echo ""

# Make the summary report stand out
ls -lh "$OUTPUT_BASE" | grep -E "nodes_|SUMMARY"

exit 0
