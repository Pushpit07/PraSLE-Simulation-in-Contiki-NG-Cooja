# Bully Algorithm Experiments

This document describes how to collect baseline metrics from the Bully leader election algorithm for evaluation and comparison.

## Table of Contents

1. [Overview](#overview)
2. [Metrics Collected](#metrics-collected)
3. [Quick Start](#quick-start)
4. [Running Experiments](#running-experiments)
5. [Analyzing Results](#analyzing-results)
6. [Visualization](#visualization)
7. [Troubleshooting](#troubleshooting)

## Overview

The Bully algorithm implementation includes comprehensive metrics collection to evaluate:

- **Convergence Time**: How quickly the network elects an initial leader
- **Communication Overhead**: Number and types of messages exchanged
- **Election Stability**: Frequency of elections and leader changes
- **State Distribution**: Time spent in different algorithm states
- **Fault Tolerance**: Behavior during coordinator failures
- **Scalability**: Performance across different network sizes (5, 10, 50, 100 nodes)

These metrics align with the evaluation criteria in the thesis exposé for comparison with PraSLE and other leader election protocols.

### Network Configurations

The experiment suite includes four different network sizes to evaluate scalability:
- **5 nodes**: Small network baseline (3×2 grid, 40m spacing)
- **10 nodes**: Medium-small network (2×5 grid, 22m spacing)
- **50 nodes**: Medium-large network (5×10 grid, 10m spacing)
- **100 nodes**: Large network (10×10 grid, 8m spacing)

All configurations use a dense network topology where all nodes are within radio range (100m), ensuring single-hop communication.

## Metrics Collected

### Election Metrics
- `elections_started`: Total elections initiated by each node
- `elections_won`: Elections won (node became coordinator)
- `elections_lost`: Elections lost (received ANSWER from higher-priority node)
- `first_convergence_time`: Time to first leader election (cold start)
- `total_election_time`: Cumulative time spent in election state

### Message Metrics
- `msg_election_sent/recv`: ELECTION messages sent/received
- `msg_answer_sent/recv`: ANSWER messages sent/received
- `msg_coordinator_sent/recv`: COORDINATOR messages sent/received
- `msg_alive_sent/recv`: ALIVE heartbeat messages sent/received
- `duplicates_filtered`: Duplicate messages filtered out

### Leader Metrics
- `leader_changes`: Number of coordinator changes
- `current_leader`: Current coordinator ID
- `total_time_as_leader`: Cumulative time as coordinator

### State Metrics
- `time_in_normal`: Time in NORMAL state
- `time_in_election`: Time in ELECTION state
- `time_in_waiting`: Time in WAITING_COORDINATOR state
- `state_transitions`: Total state transitions

### Fault Detection Metrics
- `coordinator_timeouts`: Times coordinator timeout triggered
- `alive_resets`: Times ALIVE messages reset the timeout timer
- `coordinator_reannouncements`: Coordinator re-announcements (partition healing)
- `alive_adoptions`: Adoptions via ALIVE discovery (partition healing)

## Quick Start

### Prerequisites

1. **Build Contiki-NG and Cooja**:
   ```bash
   cd /path/to/contiki-ng
   git submodule update --init --recursive
   cd tools/cooja
   ./gradlew jar
   ```

2. **Build the Bully algorithm**:
   ```bash
   cd examples/bully
   make TARGET=cooja
   ```

3. **Install Python dependencies** (for analysis scripts):
   ```bash
   pip install matplotlib pandas
   ```

### Running a Simple Experiment

```bash
cd examples/bully/scripts

# Run a 5-minute experiment with 5 nodes
python3 run_experiment.py \
    --simulation ../bully-cooja-5nodes.csc \
    --duration 300 \
    --output ../results/test_run1

# Or run with 100 nodes
python3 run_experiment.py \
    --simulation ../bully-cooja-100nodes.csc \
    --duration 300 \
    --output ../results/test_run2
```

This will:
1. Run the Cooja simulation for 300 seconds
2. Capture all log output
3. Extract metrics to CSV
4. Generate summary statistics

**Available simulation files:**
- `bully-cooja-5nodes.csc` - 5 nodes
- `bully-cooja-10nodes.csc` - 10 nodes
- `bully-cooja-50nodes.csc` - 50 nodes
- `bully-cooja-100nodes.csc` - 100 nodes

## Running Experiments

### Option 1: Single Experiment

Run a single experiment with custom parameters:

```bash
python3 scripts/run_experiment.py \
    --simulation bully-cooja.csc \
    --duration 600 \
    --output results/test
```

**Parameters:**
- `--simulation` or `-s`: Path to Cooja .csc file
- `--duration` or `-d`: Simulation duration in seconds
- `--output` or `-o`: Output directory for results
- `--contiki` or `-c`: Path to Contiki-NG root (optional, auto-detected)

### Option 2: Automated Multi-Node Scenario Suite

Run the complete experiment suite across all node counts and durations:

```bash
cd examples/bully
./run_scenarios.sh
# Or specify a custom output directory:
# ./run_scenarios.sh results/my_experiment
# ./run_scenarios.sh /absolute/path/to/output
```

This runs **12 experiments** (4 node counts × 3 durations):

**Node Counts:** 5, 10, 50, 100 nodes
**Durations:** 5 min, 15 min, 30 min

**Experiment Matrix:**
- 5 nodes: 5min, 15min, 30min
- 10 nodes: 5min, 15min, 30min
- 50 nodes: 5min, 15min, 30min
- 100 nodes: 5min, 15min, 30min

Each experiment produces:
- Raw Cooja logs
- Extracted metrics CSV
- Summary statistics
- Analysis files
- Visualization plots

**Output structure:**
```
examples/bully/results/baseline_YYYYMMDD_HHMMSS/
├── 5nodes_5min/
├── 5nodes_15min/
├── 5nodes_30min/
├── 10nodes_5min/
├── 10nodes_15min/
├── 10nodes_30min/
├── 50nodes_5min/
├── 50nodes_15min/
├── 50nodes_30min/
├── 100nodes_5min/
├── 100nodes_15min/
├── 100nodes_30min/
├── analysis/
└── SUMMARY_REPORT.txt
```

## Analyzing Results

### Using the Parser Script

Analyze metrics from a single experiment:

```bash
# Print summary to console
python3 scripts/parse_metrics.py results/test/metrics.csv --summary

# Export detailed analysis
python3 scripts/parse_metrics.py results/test/metrics.csv \
    --output results/test/analysis
```

This generates:
- `convergence.csv`: Convergence time per node
- `message_overhead.csv`: Message counts per node
- `elections.csv`: Election statistics per node

### Example Summary Output

```
======================================================================
 BULLY ALGORITHM METRICS ANALYSIS
======================================================================

1. CONVERGENCE TIME
----------------------------------------------------------------------
  Min convergence time: 245 ticks
  Max convergence time: 312 ticks
  Avg convergence time: 278.5 ticks

2. MESSAGE OVERHEAD
----------------------------------------------------------------------
  Total messages sent: 487
  Total messages received: 2922
  Avg messages per node: 81.2

  By message type:
    ELECTION:    42
    ANSWER:      38
    COORDINATOR: 6
    ALIVE:       401

3. ELECTION STATISTICS
----------------------------------------------------------------------
  Total elections started: 12
  Total elections won: 6
  Total elections lost: 6
  Avg elections per node: 2.0

4. LEADER STABILITY
----------------------------------------------------------------------
  Total leader changes: 1
  Avg leader changes per node: 0.2
  Final leader (consensus): Node 6

5. STATE DISTRIBUTION (Aggregate)
----------------------------------------------------------------------
  Time in NORMAL:      14567 ticks ( 97.1%)
  Time in ELECTION:      312 ticks (  2.1%)
  Time in WAITING:       121 ticks (  0.8%)

======================================================================
```

## Visualization

### Generate All Plots

```bash
python3 scripts/plot_results.py results/test/metrics.csv \
    --output results/test/plots
```

This creates:
- `convergence.png`: Convergence time per node (bar chart)
- `message_overhead.png`: Message overhead by type (stacked bar chart)
- `elections.png`: Election statistics (grouped bar chart)
- `state_distribution.png`: Time in each state (stacked bar chart)
- `leader_timeline.png`: Leader changes over time (line plot)
- `message_timeline.png`: Message activity over time (dual line plots)

### Generate Specific Plot Types

```bash
# Only convergence plot
python3 scripts/plot_results.py results/test/metrics.csv \
    --type convergence \
    --output results/convergence.png

# Only message timeline
python3 scripts/plot_results.py results/test/metrics.csv \
    --type timeline \
    --output results/timeline.png
```

**Plot types**: `convergence`, `messages`, `elections`, `states`, `timeline`, `all`

## Comparing Results Across Node Counts

### Scalability Analysis

After running the multi-node scenario suite, you can compare metrics across different network sizes:

```bash
# Compare convergence times (run from examples/bully/)
RESULTS_DIR="results/baseline_YYYYMMDD_HHMMSS"  # Replace with your actual directory
for nodes in 5 10 50 100; do
    echo "=== $nodes nodes ==="
    python3 scripts/parse_metrics.py \
        $RESULTS_DIR/${nodes}nodes_5min/metrics.csv \
        --summary | grep "convergence"
done

# Compare message overhead
for nodes in 5 10 50 100; do
    echo "=== $nodes nodes ==="
    python3 scripts/parse_metrics.py \
        $RESULTS_DIR/${nodes}nodes_5min/metrics.csv \
        --summary | grep "Total messages"
done
```

### Key Scalability Metrics

When analyzing scalability, focus on:

1. **Convergence Time vs Node Count**
   - Does convergence time increase linearly, quadratically, or logarithmically?
   - What is the convergence time for 5 vs 100 nodes?

2. **Message Overhead vs Node Count**
   - Total messages sent/received per node
   - Does overhead increase as O(n) or O(n²)?

3. **Election Frequency vs Node Count**
   - More nodes = more potential failures
   - How does election frequency scale?

4. **Network Stability vs Node Count**
   - Leader change frequency
   - Time to stabilize after cold start

### Expected Bully Algorithm Behavior

The Bully algorithm's theoretical complexity:
- **Message complexity**: O(n²) per election in worst case
- **Time complexity**: O(n) rounds for convergence
- **Convergence**: Deterministic, guaranteed in asynchronous networks

Compare experimental results against these theoretical bounds.

## Troubleshooting

### Cooja Not Found

**Error**: `Cooja JAR not found`

**Solution**:
```bash
cd /path/to/contiki-ng/tools/cooja
./gradlew jar
```

### No Metrics in Output

**Problem**: CSV file is empty or missing

**Causes**:
1. Metrics collection disabled
2. Simulation crashed before metrics output
3. Incorrect simulation file

**Solutions**:
- Check `bully-node.c`: `#define ENABLE_METRICS 1`
- Rebuild: `make TARGET=cooja clean && make TARGET=cooja`
- Verify simulation file loads in Cooja GUI
- Check Cooja output log for errors

### Python Dependencies Missing

**Error**: `ImportError: No module named 'pandas'`

**Solution**:
```bash
pip install matplotlib pandas
# or
pip3 install matplotlib pandas
```

### Simulation Hangs

**Problem**: Simulation doesn't complete

**Solutions**:
- Check simulation duration in .csc file
- Verify nodes are properly configured
- Run in Cooja GUI to debug
- Check system resources (memory, CPU)

## Experiment Design Recommendations

### For Baseline Metrics

Run multiple trials to account for randomness:

```bash
# Run 5 trials for each node count (run from examples/bully/)
for nodes in 5 10 50 100; do
    for trial in {1..5}; do
        python3 scripts/run_experiment.py \
            --simulation bully-cooja-${nodes}nodes.csc \
            --duration 600 \
            --output results/trials/${nodes}nodes_trial_${trial}
    done
done
```

Then compute aggregate statistics across trials.

### For Scalability Studies

The multi-node experiment suite is designed to evaluate scalability:

```bash
# Run the complete suite (run from examples/bully/)
./run_scenarios.sh
# Results will be saved to: results/baseline_YYYYMMDD_HHMMSS/
```

This provides data for:
- **Weak scaling**: Fixed duration, increasing nodes
- **Performance trends**: How metrics change with network size
- **Overhead analysis**: Message complexity vs node count

### For Comparison Studies

Keep these parameters constant across experiments:
- Network topology (dense single-hop for all node counts)
- Radio model and parameters (100m range, perfect links)
- Simulation duration (when comparing algorithms)
- Timing parameters (unless studying their effect)

Vary only:
- The algorithm being compared (Bully vs PraSLE vs others)
- Network size (when studying scalability)
- Specific parameters being evaluated

### Statistical Validity

For publishable results:
- Run at least **5-10 trials** per configuration
- Compute mean, median, and standard deviation
- Use appropriate statistical tests (e.g., t-test, ANOVA)
- Report confidence intervals

## Convergence Time Statistical Analysis

For rigorous statistical analysis and publication-quality data, use the convergence trials script to collect 100+ measurements.

### Running Convergence Trials

The `experiments/convergence/run_convergence_trials.sh` script runs multiple trials focused on convergence time:

```bash
cd examples/bully

# Run 100 trials with 50 nodes (default: 60s per trial, auto-detect CPU cores)
./experiments/convergence/run_convergence_trials.sh 50

# Run 200 trials with 100 nodes (parallel execution)
./experiments/convergence/run_convergence_trials.sh 100 200

# Run 50 trials with 5 nodes, 90s duration per trial
./experiments/convergence/run_convergence_trials.sh 5 50 90

# Run 100 trials with 50 nodes using 4 parallel jobs
./experiments/convergence/run_convergence_trials.sh 50 100 60 4
```

**Parameters**:
- `node_count` (required): 5, 10, 50, or 100
- `trials` (optional, default: 100): Number of trials to run
- `duration` (optional, default: 60): Duration per trial in seconds
- `parallel_jobs` (optional, default: auto-detect): Number of parallel jobs to run simultaneously

**Parallel Execution**:
The script automatically detects the number of CPU cores and runs trials in parallel to significantly reduce total execution time. For example, with 8 cores, 100 trials that would take ~116 minutes sequentially can complete in ~15 minutes. You can override the automatic detection by specifying the number of parallel jobs as the 4th parameter.

**Important**: Each trial uses a unique random seed to ensure statistical independence. Without this, all trials would produce identical results due to Cooja's deterministic simulation.

**Output**:
```
results/convergence_trials/{nodes}nodes_YYYYMMDD_HHMMSS/
├── convergence_times.csv         # All measurements
├── trial_1/metrics.csv           # Individual trial data
├── trial_2/metrics.csv
...
└── trial_N/metrics.csv
```

### Visualizing Results

Create distribution plots with mean and error bars:

```bash
# Simple wrapper script - auto-detects most recent trials (recommended)
./experiments/convergence/create-convergence-plot.sh

# Or specify a specific trials directory
./experiments/convergence/create-convergence-plot.sh results/convergence_trials/50nodes_20251201_123456

# Or specify custom output file
./experiments/convergence/create-convergence-plot.sh results/convergence_trials/50nodes_20251201_123456 my_plot.png

# Advanced: Use the Python script directly for custom options
python3 scripts/plot_convergence_distribution.py \
    -i results/convergence_trials/100nodes_20251201_123456/convergence_times.csv \
    -o plots/convergence_100nodes.png \
    --title "Bully Algorithm - 100 Nodes Convergence" \
    --dpi 150
```

**Plot features**:
- Scatter plot of all individual measurements
- Horizontal line showing mean convergence time
- Shaded region for ±1σ standard deviation
- Statistics box with mean, median, std dev, min, max, CV
- Grid for easy reading

### Example Output

**Console summary**:
```
Convergence Time Statistics:
============================================================
Trials:        100
Node Count:    50
Mean:          2,427 ms
Median:        2,401 ms
Std Dev:       142 ms
Min:           2,198 ms
Max:           2,756 ms
Range:         558 ms
CV:            5.8%
============================================================
```

### Comparing Across Node Counts

Run trials for each node count and compare:

```bash
# Run trials for all node counts
for nodes in 5 10 50 100; do
    echo "Running trials for $nodes nodes..."
    ./experiments/convergence/run_convergence_trials.sh $nodes
done

# Generate plots for each
for dir in results/convergence_trials/*nodes_*/; do
    ./experiments/convergence/create-convergence-plot.sh "$dir"
done
```

### Statistical Analysis

Use the collected data for:

1. **Mean comparison**: t-test or ANOVA to compare convergence times across node counts
2. **Variance analysis**: F-test to compare consistency across configurations
3. **Scalability trends**: Regression analysis to model convergence time vs node count
4. **Outlier detection**: Identify and investigate anomalous convergence times
5. **Confidence intervals**: Calculate 95% CI for mean convergence time

### Use Cases

**Convergence trials are ideal for**:
- Publication data with statistical rigor
- Comparing algorithm performance (Bully vs PraSLE vs others)
- Analyzing scalability (how convergence time changes with network size)
- Validating simulation consistency
- Identifying performance regressions

**Not recommended for**:
- Testing message overhead or leader changes (use full scenario suite instead)
- Long-term stability analysis (convergence happens quickly)
- Partition healing (requires deliberate network manipulation)

## Next Steps

After collecting Bully baseline metrics across all node counts:

1. **Analyze scalability**: Compare metrics across 5, 10, 50, and 100 node configurations
2. **Implement PraSLE**: Implement PraSLE algorithm in Contiki-NG
3. **Create PraSLE CSC files**: Generate matching simulation files for 5, 10, 50, 100 nodes
4. **Run PraSLE experiments**: Use same multi-node experimental setup
5. **Compare algorithms**: Bully vs PraSLE across all node counts
   - Convergence time vs network size
   - Message overhead vs network size
   - Stability and fault tolerance
   - Scalability characteristics
6. **Implement extensions**: Energy-aware, link-quality-aware, adaptive timeouts
7. **Evaluate extensions**: Compare extended PraSLE vs baseline PraSLE vs Bully across all scales

## References

- Thesis Exposé: See Section 6 "Evaluation" for metrics definitions
- Bully Algorithm: [README.md](README.md) for algorithm description
- Contiki-NG Documentation: https://docs.contiki-ng.org/
- Cooja Simulator: https://docs.contiki-ng.org/en/develop/doc/tutorials/Running-Contiki-NG-in-Cooja.html
