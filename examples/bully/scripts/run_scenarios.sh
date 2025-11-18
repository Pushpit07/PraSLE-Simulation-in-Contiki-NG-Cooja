#!/bin/bash
#
# Bully Algorithm Scenario Runner
#
# This script runs multiple experiment scenarios to collect baseline
# metrics for the Bully leader election algorithm.
#
# Usage:
#   ./run_scenarios.sh [output_base_dir]
#
# Example:
#   ./run_scenarios.sh results/baseline_$(date +%Y%m%d)
#

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BULLY_DIR="$(dirname "$SCRIPT_DIR")"
SIMULATION_FILE="$BULLY_DIR/bully-cooja.csc"
OUTPUT_BASE="${1:-results/baseline_$(date +%Y%m%d_%H%M%S)}"

# Create output directory
mkdir -p "$OUTPUT_BASE"

echo "=============================================="
echo " Bully Algorithm Baseline Scenario Runner"
echo "=============================================="
echo "Simulation file: $SIMULATION_FILE"
echo "Output directory: $OUTPUT_BASE"
echo ""

# Check if simulation file exists
if [ ! -f "$SIMULATION_FILE" ]; then
    echo "[ERROR] Simulation file not found: $SIMULATION_FILE"
    exit 1
fi

# Scenario 1: Cold Start (Initial Election)
echo "----------------------------------------"
echo "Scenario 1: Cold Start Election"
echo "----------------------------------------"
echo "Description: All nodes start simultaneously, elect initial leader"
echo "Duration: 5 minutes"
echo ""

python3 "$SCRIPT_DIR/run_experiment.py" \
    --simulation "$SIMULATION_FILE" \
    --duration 300 \
    --output "$OUTPUT_BASE/scenario1_cold_start"

echo ""
echo "Scenario 1 completed!"
echo ""

# Scenario 2: Extended Runtime (Stability)
echo "----------------------------------------"
echo "Scenario 2: Extended Runtime"
echo "----------------------------------------"
echo "Description: Extended runtime to observe leader stability"
echo "Duration: 15 minutes"
echo ""

python3 "$SCRIPT_DIR/run_experiment.py" \
    --simulation "$SIMULATION_FILE" \
    --duration 900 \
    --output "$OUTPUT_BASE/scenario2_stability"

echo ""
echo "Scenario 2 completed!"
echo ""

# Scenario 3: Long-term Monitoring
echo "----------------------------------------"
echo "Scenario 3: Long-term Monitoring"
echo "----------------------------------------"
echo "Description: Long-term execution for comprehensive baseline"
echo "Duration: 30 minutes"
echo ""

python3 "$SCRIPT_DIR/run_experiment.py" \
    --simulation "$SIMULATION_FILE" \
    --duration 1800 \
    --output "$OUTPUT_BASE/scenario3_longterm"

echo ""
echo "Scenario 3 completed!"
echo ""

# Generate aggregate analysis
echo "=============================================="
echo " Generating Aggregate Analysis"
echo "=============================================="

ANALYSIS_DIR="$OUTPUT_BASE/analysis"
mkdir -p "$ANALYSIS_DIR"

# Analyze each scenario
for scenario in "$OUTPUT_BASE"/scenario*; do
    scenario_name=$(basename "$scenario")
    echo "Analyzing $scenario_name..."

    if [ -f "$scenario/metrics.csv" ]; then
        python3 "$SCRIPT_DIR/parse_metrics.py" \
            "$scenario/metrics.csv" \
            --output "$ANALYSIS_DIR/$scenario_name"

        # Generate plots
        python3 "$SCRIPT_DIR/plot_results.py" \
            "$scenario/metrics.csv" \
            --output "$ANALYSIS_DIR/$scenario_name/plots"
    else
        echo "[WARNING] No metrics file found for $scenario_name"
    fi
done

# Create summary report
echo "=============================================="
echo " Creating Summary Report"
echo "=============================================="

SUMMARY_FILE="$OUTPUT_BASE/SUMMARY_REPORT.txt"

cat > "$SUMMARY_FILE" << EOF
================================================================================
BULLY ALGORITHM BASELINE EXPERIMENTS SUMMARY
================================================================================

Experiment Date: $(date +"%Y-%m-%d %H:%M:%S")
Simulation File: $SIMULATION_FILE
Output Directory: $OUTPUT_BASE

================================================================================
SCENARIOS EXECUTED
================================================================================

Scenario 1: Cold Start Election
  - Description: Initial leader election with all nodes starting simultaneously
  - Duration: 5 minutes (300 seconds)
  - Results: $OUTPUT_BASE/scenario1_cold_start/

Scenario 2: Extended Runtime
  - Description: Extended runtime to observe leader stability and behavior
  - Duration: 15 minutes (900 seconds)
  - Results: $OUTPUT_BASE/scenario2_stability/

Scenario 3: Long-term Monitoring
  - Description: Long-term execution for comprehensive baseline metrics
  - Duration: 30 minutes (1800 seconds)
  - Results: $OUTPUT_BASE/scenario3_longterm/

================================================================================
ANALYSIS OUTPUTS
================================================================================

Each scenario directory contains:
  - cooja_output.log  : Raw Cooja simulation output
  - metrics.csv       : Extracted metrics in CSV format
  - summary.txt       : Automated summary statistics

Analysis directory ($ANALYSIS_DIR/) contains:
  - Per-scenario analysis CSV files
  - Visualization plots (convergence, messages, elections, etc.)

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

1. Review individual scenario summaries in each scenario directory

2. Examine visualization plots in:
   $ANALYSIS_DIR/scenario*/plots/

3. Compare metrics across scenarios to establish baseline behavior

4. Use these baseline results to compare against extended PraSLE implementation

================================================================================
EOF

cat "$SUMMARY_FILE"

echo ""
echo "=============================================="
echo " ALL SCENARIOS COMPLETED!"
echo "=============================================="
echo "Results saved to: $OUTPUT_BASE"
echo "Summary report: $SUMMARY_FILE"
echo ""

# Make the summary report stand out
ls -lh "$OUTPUT_BASE"

exit 0
