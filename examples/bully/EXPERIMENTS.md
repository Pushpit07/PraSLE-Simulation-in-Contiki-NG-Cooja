# Bully Algorithm Experiments

This document describes how to collect metrics from the Bully leader election algorithm for evaluation and comparison across three key experiment types.

## Table of Contents

1. [Experiment Types](#experiment-types)
2. [Overview](#overview)
3. [Metrics Collected](#metrics-collected)
4. [Quick Start](#quick-start)
5. [Running Experiments](#running-experiments)
6. [Analyzing Results](#analyzing-results)
7. [Visualization](#visualization)
8. [Troubleshooting](#troubleshooting)

## Experiment Types

The experiment suite is organized into three categories to comprehensively evaluate the Bully algorithm:

### 1. Normal Election (Convergence Analysis)
**Location:** `experiments/convergence/`

**Purpose:** Measure cold-start convergence time and baseline performance under ideal network conditions.

**Key Metrics:**
- Initial convergence time (time to first leader election)
- Message overhead during election
- Scalability across network sizes (5, 10, 50, 100 nodes)

**Scenarios:**
- Multiple trials with different random seeds for statistical analysis
- Variable node counts to evaluate scalability
- Parallel execution for efficient data collection (100+ trials)

**Use Cases:**
- Baseline performance comparison with other algorithms (PraSLE, Ring, etc.)
- Scalability analysis
- Statistical validation of convergence properties

---

### 2. Leader Crash Recovery (Fault Tolerance)
**Location:** `experiments/fault_tolerance/`

**Purpose:** Evaluate the algorithm's ability to detect leader failures and elect a new coordinator.

**Key Metrics:**
- Failure detection time (should be ~20s based on COORDINATOR_TIMEOUT)
- Re-election time
- Total recovery time
- Message overhead during recovery
- Election frequency after crashes

**Scenarios:**
- Single leader crash at specific time
- Multiple cascading crashes
- Crash-recovery cycles

**Use Cases:**
- Fault tolerance evaluation
- Recovery time analysis
- Comparison with self-healing algorithms

---

### 3. Network Disruption (Resilience Testing)
**Location:** `experiments/noise/` and `experiments/network_partition/`

**Purpose:** Test algorithm resilience under realistic network conditions with packet loss and partitions.

**Key Metrics:**
- Convergence time under noise
- Election success rate with packet loss
- Message retransmission overhead
- Partition healing time
- Behavior with asymmetric links

**Scenarios:**
- **Noise/Packet Loss:** 10%, 30%, 50% packet loss rates
- **Network Partitions:** Split network into isolated groups, then heal
- **Dynamic Disruption:** Add/remove noise during simulation

**Use Cases:**
- Real-world deployment evaluation
- Robustness analysis
- Comparison with partition-tolerant algorithms

---

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

### Random Seeds vs Fixed Seeds

All trial runner scripts support a `--fixed-seed` option to control simulation randomness:

| Mode | Flag | Behavior | Use Case |
|------|------|----------|----------|
| **Random** (default) | *(none)* | Each trial uses a unique random seed | Statistical analysis with variation |
| **Fixed** | `--fixed-seed` | All trials use the CSC file's original seed | Reproducibility testing, debugging |

**Random seeds** (default): Each trial modifies the `<randomseed>` value in the CSC file, producing different simulation outcomes. This is **required for meaningful statistical analysis** since Cooja simulations are fully deterministic - running the same CSC file twice produces identical results.

**Fixed seed** (`--fixed-seed`): All trials use the original seed from the CSC template, producing identical results. Useful for:
- Verifying experiment reproducibility
- Debugging specific simulation behaviors
- Confirming determinism of results

The `--fixed-seed` flag can be placed anywhere in the command:
```bash
# All of these are equivalent:
./experiments/convergence/run_convergence_trials.sh 50 --fixed-seed
./experiments/convergence/run_convergence_trials.sh 50 100 --fixed-seed
./experiments/convergence/run_convergence_trials.sh --fixed-seed 50 100 60 4
```

## Experiment 1: Normal Election (Convergence Time Statistical Analysis)

For rigorous statistical analysis and publication-quality data, use the convergence trials script to collect 100+ measurements of cold-start convergence time under ideal network conditions.

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

# Run with fixed seed for reproducible results (all trials identical)
./experiments/convergence/run_convergence_trials.sh 50 --fixed-seed
```

**Parameters**:
- `node_count` (required): 5, 10, 50, or 100
- `trials` (optional, default: 100): Number of trials to run
- `duration` (optional, default: 60): Duration per trial in seconds
- `parallel_jobs` (optional, default: auto-detect): Number of parallel jobs to run simultaneously
- `--fixed-seed` (optional): Use same seed for all trials (see [Random Seeds vs Fixed Seeds](#random-seeds-vs-fixed-seeds))

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

## Experiment 2: Leader Crash Recovery (Fault Tolerance Testing)

Evaluate the algorithm's ability to detect coordinator failures and recover through re-election.

### Running Crash Recovery Trials

The `experiments/fault_tolerance/run_crash_trials.sh` script crashes the elected leader at a specific time and measures recovery:

```bash
cd examples/bully

# Run 100 trials with 50 nodes, crash leader at 60s (default: 120s total duration)
./experiments/fault_tolerance/run_crash_trials.sh 50

# Run 200 trials with 100 nodes, crash leader at 60s
./experiments/fault_tolerance/run_crash_trials.sh 100 200

# Run 50 trials with 10 nodes, crash at 30s, 90s total duration
./experiments/fault_tolerance/run_crash_trials.sh 10 50 30 90

# Run 100 trials with 50 nodes, crash at 60s, 120s total, 4 parallel jobs
./experiments/fault_tolerance/run_crash_trials.sh 50 100 60 120 4

# Run with fixed seed for reproducible results
./experiments/fault_tolerance/run_crash_trials.sh 50 --fixed-seed
```

**Parameters**:
- `node_count` (required): 5, 10, 50, or 100
- `trials` (optional, default: 100): Number of trials to run
- `crash_time` (optional, default: 60): Time to crash leader in seconds
- `duration` (optional, default: 120): Total duration per trial in seconds
- `parallel_jobs` (optional, default: auto-detect): Number of parallel jobs
- `--fixed-seed` (optional): Use same seed for all trials (see [Random Seeds vs Fixed Seeds](#random-seeds-vs-fixed-seeds))

**Important**: `crash_time` must be less than `duration` to allow time for recovery observation.

**How it works**:
1. Simulation starts, nodes elect initial leader (highest ID)
2. At `crash_time`, the leader is removed from the simulation
3. Remaining nodes detect timeout (~20s based on COORDINATOR_TIMEOUT)
4. New election begins, new leader is elected
5. Metrics capture both initial and recovery convergence times

**Output**:
```
results/fault_tolerance/{nodes}nodes_crash{time}s_YYYYMMDD_HHMMSS/
├── crash_recovery_times.csv      # Recovery metrics for all trials
├── trial_1/metrics.csv            # Individual trial data
├── trial_2/metrics.csv
...
└── trial_N/metrics.csv
```

**CSV columns**:
- `trial`: Trial number
- `node_count`: Number of nodes
- `crash_time_s`: When leader was crashed
- `initial_convergence_time_ms`: Time to first leader election
- `recovery_convergence_time_ms`: Absolute time when new leader elected
- `total_recovery_time_ms`: Duration from crash to re-election

### Visualizing Recovery Results

Create recovery plots with the simple wrapper script:

```bash
# Simple wrapper script - auto-detects most recent trials (recommended)
./experiments/fault_tolerance/create-fault-tolerance-plot.sh

# Or specify a specific trials directory
./experiments/fault_tolerance/create-fault-tolerance-plot.sh results/fault_tolerance/50nodes_crash60s_20251201_123456

# Or specify the CSV file directly
./experiments/fault_tolerance/create-fault-tolerance-plot.sh results/fault_tolerance/50nodes_crash60s_20251201_123456/crash_recovery_times.csv
```

**Advanced**: Use the Python script directly for custom options:

```bash
# Analyze crash recovery data with custom output directory
python3 experiments/fault_tolerance/analyze_recovery.py \
    -i results/fault_tolerance/50nodes_crash60s_20251201_123456/crash_recovery_times.csv \
    -o analysis_output/

# Skip plots, statistics only
python3 experiments/fault_tolerance/analyze_recovery.py \
    -i results/fault_tolerance/50nodes_crash60s_20251201_123456/crash_recovery_times.csv \
    --no-plots
```

**Generated files**:
- `recovery_summary.csv`: Statistical summary
- `recovery_timeline_{nodes}nodes.png`: Timeline showing initial convergence, crash, and recovery
- `recovery_distribution_{nodes}nodes.png`: Distribution of recovery times

**Console output**:
```
Leader Crash Recovery Analysis:
======================================================================
Node Count:        50
Crash Time:        60s
Total Trials:      100
======================================================================

Initial Convergence (before crash):
----------------------------------------------------------------------
  Valid Trials:    100/100
  Mean:            2,427 ms
  Median:          2,401 ms
  Std Dev:         142 ms
  Min:             2,198 ms
  Max:             2,756 ms

Recovery Time (crash to re-election):
----------------------------------------------------------------------
  Valid Trials:    98/100
  Mean:            21,543 ms (21.54s)
  Median:          21,312 ms (21.31s)
  Std Dev:         1,234 ms
  Min:             20,012 ms (20.01s)
  Max:             24,567 ms (24.57s)

  Expected Detection Time: ~20s (COORDINATOR_TIMEOUT)
  Observed Detection+Re-election: 21.54s
======================================================================
```

### Key Fault Tolerance Metrics

When analyzing crash recovery:

1. **Failure Detection Time**
   - Should be ~20 seconds (COORDINATOR_TIMEOUT value)
   - Variance indicates timeout consistency

2. **Re-Election Time**
   - Time from detection to new leader election
   - Similar to initial convergence time
   - Overhead due to timeout detection

3. **Total Recovery Time**
   - End-to-end: crash → detection → re-election
   - Critical for service availability
   - Typically: COORDINATOR_TIMEOUT + convergence_time

4. **Success Rate**
   - Percentage of trials with successful recovery
   - Should be 100% for robust algorithm

### Comparing Recovery Across Node Counts

```bash
# Run crash recovery trials for all node counts
for nodes in 5 10 50 100; do
    echo "Running crash recovery trials for $nodes nodes..."
    ./experiments/fault_tolerance/run_crash_trials.sh $nodes
done

# Analyze each
for dir in results/fault_tolerance/*nodes_crash*; do
    python3 experiments/fault_tolerance/analyze_recovery.py -i "$dir/crash_recovery_times.csv" -o "$dir/analysis"
done
```

### Use Cases

**Fault tolerance experiments are ideal for**:
- Evaluating failure detection mechanisms
- Measuring recovery time under failures
- Testing self-healing properties
- Comparing resilience across algorithms
- Analyzing impact of node count on recovery

## Experiment 3: Network Disruption (Resilience Testing)

Test algorithm behavior under realistic network conditions with packet loss and partitions.

### Running Noise/Packet Loss Trials

The `experiments/noise/run_noise_trials.sh` script tests convergence under various packet loss rates:

```bash
cd examples/bully

# Run 100 trials with 50 nodes, 90% success rate (10% packet loss)
./experiments/noise/run_noise_trials.sh 50 90

# Run 200 trials with 100 nodes, 70% success rate (30% packet loss)
./experiments/noise/run_noise_trials.sh 100 70 200

# Run 50 trials with 10 nodes, 50% success rate, 90s duration
./experiments/noise/run_noise_trials.sh 10 50 50 90

# Run 100 trials with 50 nodes, 90% success, 60s duration, 4 parallel jobs
./experiments/noise/run_noise_trials.sh 50 90 100 60 4

# Run with fixed seed for reproducible results
./experiments/noise/run_noise_trials.sh 50 90 --fixed-seed
```

**Parameters**:
- `node_count` (required): 5, 10, 50, or 100
- `noise_level` (required): 90, 70, or 50 (success rate percentage)
- `trials` (optional, default: 100): Number of trials to run
- `duration` (optional, default: 60): Duration per trial in seconds
- `parallel_jobs` (optional, default: auto-detect): Number of parallel jobs
- `--fixed-seed` (optional): Use same seed for all trials (see [Random Seeds vs Fixed Seeds](#random-seeds-vs-fixed-seeds))

**Noise levels**:
- **90%**: Light noise (10% packet loss) - simulates good but imperfect network
- **70%**: Moderate noise (30% packet loss) - challenging but realistic conditions
- **50%**: Heavy noise (50% packet loss) - extreme stress test

**Output**:
```
results/noise/{nodes}nodes_noise{level}_YYYYMMDD_HHMMSS/
├── noise_convergence_times.csv    # Convergence and message metrics
├── trial_1/metrics.csv             # Individual trial data
├── trial_2/metrics.csv
...
└── trial_N/metrics.csv
```

**CSV columns**:
- `trial`: Trial number
- `node_count`: Number of nodes
- `noise_level`: Success rate percentage
- `convergence_time_ms`: Time to leader election
- `message_count`: Total messages sent (includes retransmissions)

### Visualizing Noise Results

Create noise analysis plots with the simple wrapper script:

```bash
# Simple wrapper script - auto-detects most recent trials (recommended)
./experiments/noise/create-noise-plot.sh

# Or specify a specific trials directory
./experiments/noise/create-noise-plot.sh results/noise/50nodes_noise90_20251201_123456

# Or specify the CSV file directly
./experiments/noise/create-noise-plot.sh results/noise/50nodes_noise90_20251201_123456/noise_convergence_times.csv
```

**Advanced**: Use the Python script directly for custom options:

```bash
# Analyze noise experiment data with custom output directory
python3 experiments/noise/analyze_noise.py \
    -i results/noise/50nodes_noise90_20251201_123456/noise_convergence_times.csv \
    -o analysis_output/

# Statistics only
python3 experiments/noise/analyze_noise.py \
    -i results/noise/50nodes_noise70_20251201_123456/noise_convergence_times.csv \
    --no-plots
```

**Generated files**:
- `noise_summary.csv`: Statistical summary
- `noise{level}_convergence_{nodes}nodes.png`: Convergence time distribution
- `noise{level}_messages_{nodes}nodes.png`: Message overhead distribution

**Console output**:
```
Network Disruption Analysis:
======================================================================
Node Count:        50
Noise Level:       90% success rate (10% packet loss)
Total Trials:      100
======================================================================

Convergence Time (under noise):
----------------------------------------------------------------------
  Valid Trials:    98/100
  Mean:            2,687 ms
  Median:          2,654 ms
  Std Dev:         312 ms
  Min:             2,234 ms
  Max:             3,456 ms
  CV:              11.6%

Message Overhead:
----------------------------------------------------------------------
  Valid Trials:    98/100
  Mean:            523 messages
  Median:          512 messages
  Std Dev:         45 messages
  Min:             456 messages
  Max:             678 messages

  Expected overhead increase: ~11%
  (due to 10% packet loss requiring retransmissions)
======================================================================
```

### Running Network Partition Trials

The partition scenario creates two isolated network groups that cannot communicate, testing split-brain behavior.

The `experiments/network_partition/run_partition_trials.sh` script runs multiple trials with partitioned networks:

```bash
cd examples/bully

# Run 100 trials with 50 nodes (25+25 partition split)
./experiments/network_partition/run_partition_trials.sh 50

# Run 200 trials with 100 nodes
./experiments/network_partition/run_partition_trials.sh 100 200

# Run 50 trials with 10 nodes, 90s duration
./experiments/network_partition/run_partition_trials.sh 10 50 90

# Run 100 trials with 4 parallel jobs
./experiments/network_partition/run_partition_trials.sh 50 100 60 4

# Run with fixed seed for reproducible results
./experiments/network_partition/run_partition_trials.sh 50 --fixed-seed
```

**Parameters**:
- `node_count` (required): 5, 10, 50, or 100
- `trials` (optional, default: 100): Number of trials to run
- `duration` (optional, default: 60): Duration per trial in seconds
- `parallel_jobs` (optional, default: auto-detect): Number of parallel jobs
- `--fixed-seed` (optional): Use same seed for all trials (see [Random Seeds vs Fixed Seeds](#random-seeds-vs-fixed-seeds))

**Partition Configuration**:

| Nodes | Partition A | Partition B | Expected Leaders |
|-------|-------------|-------------|------------------|
| 5     | Nodes 1-2   | Nodes 3-5   | Node 2 + Node 5  |
| 10    | Nodes 1-5   | Nodes 6-10  | Node 5 + Node 10 |
| 50    | Nodes 1-25  | Nodes 26-50 | Node 25 + Node 50|
| 100   | Nodes 1-50  | Nodes 51-100| Node 50 + Node 100|

**How it works**:
1. Partitions are separated by 180m (radio range is 100m)
2. Each partition elects its own leader independently
3. This creates a "split-brain" scenario with two leaders
4. Metrics capture convergence time for both partitions

**Output**:
```
results/network_partition/{nodes}nodes_partition_YYYYMMDD_HHMMSS/
├── partition_times.csv           # All partition metrics
├── trial_1/metrics.csv           # Individual trial data
├── trial_2/metrics.csv
...
└── trial_N/metrics.csv
```

**CSV columns**:
- `trial`: Trial number
- `node_count`: Number of nodes
- `partition_a_convergence_ms`: Convergence time for partition A
- `partition_b_convergence_ms`: Convergence time for partition B
- `partition_a_leader`: Leader elected in partition A
- `partition_b_leader`: Leader elected in partition B
- `split_brain`: Whether split-brain occurred (should be true)

### Visualizing Partition Results

Create partition analysis plots with the simple wrapper script:

```bash
# Simple wrapper script - auto-detects most recent trials (recommended)
./experiments/network_partition/create-partition-plot.sh

# Or specify a specific trials directory
./experiments/network_partition/create-partition-plot.sh results/network_partition/50nodes_partition_20231201_123456

# Or specify the CSV file directly
./experiments/network_partition/create-partition-plot.sh results/network_partition/50nodes_partition_20231201_123456/partition_times.csv
```

**Advanced**: Use the Python script directly for custom options:

```bash
# Analyze partition data with custom output directory
python3 experiments/network_partition/analyze_partition.py \
    -i results/network_partition/50nodes_partition_20231201_123456/partition_times.csv \
    -o analysis_output/

# Skip plots, statistics only
python3 experiments/network_partition/analyze_partition.py \
    -i results/network_partition/50nodes_partition_20231201_123456/partition_times.csv \
    --no-plots
```

**Generated files**:
- `partition_summary.csv`: Statistical summary for both partitions
- `partition_convergence_{nodes}nodes.png`: Convergence time comparison
- `partition_leaders_{nodes}nodes.png`: Leader distribution for each partition

**Console output**:
```
Network Partition Analysis:
======================================================================
Node Count:        50
Partition A:       Nodes 1-25 (expected leader: 25)
Partition B:       Nodes 26-50 (expected leader: 50)
Total Trials:      100
======================================================================

Partition A Convergence Time:
----------------------------------------------------------------------
  Valid Trials:    100/100
  Mean:            2,427 ms
  Median:          2,401 ms
  Std Dev:         142 ms
  Min:             2,198 ms
  Max:             2,756 ms

Partition B Convergence Time:
----------------------------------------------------------------------
  Valid Trials:    100/100
  Mean:            2,512 ms
  Median:          2,489 ms
  Std Dev:         156 ms
  Min:             2,234 ms
  Max:             2,845 ms

Split-Brain Detection:
----------------------------------------------------------------------
  Trials with split-brain: 100/100 (100.0%)
  Expected: 100% (partitions should elect independent leaders)
======================================================================
```

### Key Partition Metrics

1. **Convergence Time per Partition**
   - How quickly does each partition elect its leader?
   - Should be similar to normal election (no interference)
   - Compare between partitions of different sizes

2. **Leader Correctness**
   - Did each partition elect the highest-ID node?
   - Expected: 100% correct leader selection
   - Any deviation indicates algorithm issues

3. **Split-Brain Detection**
   - Did both partitions elect independent leaders?
   - Expected: 100% split-brain (by design)
   - Verifies partition isolation is working

4. **Use Cases**
   - Testing partition detection mechanisms
   - Evaluating split-brain scenarios
   - Comparing with partition-tolerant algorithms
   - Baseline for partition healing experiments

### Comparing Across Noise Levels

```bash
# Run noise experiments for all levels
for noise in 90 70 50; do
    echo "Running trials with ${noise}% success rate..."
    ./experiments/noise/run_noise_trials.sh 50 $noise
done

# Analyze each
for dir in results/noise/50nodes_noise*; do
    python3 experiments/noise/analyze_noise.py -i "$dir/noise_convergence_times.csv" -o "$dir/analysis"
done

# Compare results
echo "=== Convergence Time vs Noise Level ==="
for dir in results/noise/50nodes_noise*/analysis; do
    echo "$(basename $(dirname $dir)):"
    grep "Mean" "$dir/noise_summary.csv" | head -1
done
```

### Key Resilience Metrics

1. **Convergence Time Increase**
   - How much does packet loss slow convergence?
   - Expected: Higher packet loss → longer convergence
   - Measure: % increase vs baseline (0% loss)

2. **Message Overhead**
   - Total messages sent including retransmissions
   - Expected: ~(100/success_rate) multiplier
   - Example: 50% success → ~2x messages

3. **Convergence Success Rate**
   - Percentage of trials reaching consensus
   - Should remain high even under noise
   - Indicates algorithm robustness

4. **Variance/Consistency**
   - Standard deviation and CV
   - Higher noise → higher variance expected
   - Measure of predictability under disruption

### Use Cases

**Network disruption experiments are ideal for**:
- Real-world deployment validation
- Robustness testing under packet loss
- Comparing resilience across algorithms
- Analyzing retransmission overhead
- Testing partition detection and handling

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

## Quick Reference: All Available Experiments

### Experiment 1: Normal Election (Convergence Time)

**Location:** `experiments/convergence/`

| Command | Description |
|---------|-------------|
| `./experiments/convergence/run_convergence_trials.sh 50` | Run 100 trials with 50 nodes |
| `./experiments/convergence/run_convergence_trials.sh 100 200` | Run 200 trials with 100 nodes |
| `./experiments/convergence/run_convergence_trials.sh 5 50 90` | 50 trials, 5 nodes, 90s duration |
| `./experiments/convergence/run_convergence_trials.sh 50 100 60 4` | 100 trials, 4 parallel jobs |
| `./experiments/convergence/run_convergence_trials.sh 50 --fixed-seed` | Fixed seed (reproducible) |
| `./experiments/convergence/create-convergence-plot.sh` | Generate plots (auto-detect latest) |

**Node counts:** 5, 10, 50, 100

---

### Experiment 2: Leader Crash Recovery (Fault Tolerance)

**Location:** `experiments/fault_tolerance/`

| Command | Description |
|---------|-------------|
| `./experiments/fault_tolerance/run_crash_trials.sh 50` | 100 trials, crash at 60s, 120s total |
| `./experiments/fault_tolerance/run_crash_trials.sh 100 200` | 200 trials with 100 nodes |
| `./experiments/fault_tolerance/run_crash_trials.sh 10 50 30 90` | 50 trials, crash at 30s, 90s total |
| `./experiments/fault_tolerance/run_crash_trials.sh 50 100 60 120 4` | 100 trials, 4 parallel jobs |
| `./experiments/fault_tolerance/run_crash_trials.sh 50 --fixed-seed` | Fixed seed (reproducible) |
| `./experiments/fault_tolerance/create-fault-tolerance-plot.sh` | Generate plots (auto-detect latest) |

**Node counts:** 5, 10, 50, 100

---

### Experiment 3a: Network Noise (Packet Loss)

**Location:** `experiments/noise/`

| Command | Description |
|---------|-------------|
| `./experiments/noise/run_noise_trials.sh 50 90` | 100 trials, 50 nodes, 90% success (10% loss) |
| `./experiments/noise/run_noise_trials.sh 100 70 200` | 200 trials, 100 nodes, 70% success |
| `./experiments/noise/run_noise_trials.sh 10 50 50 90` | 50 trials, 10 nodes, 50% success, 90s |
| `./experiments/noise/run_noise_trials.sh 50 90 100 60 4` | 100 trials, 4 parallel jobs |
| `./experiments/noise/run_noise_trials.sh 50 90 --fixed-seed` | Fixed seed (reproducible) |
| `./experiments/noise/create-noise-plot.sh` | Generate plots (auto-detect latest) |

**Node counts:** 5, 10, 50, 100
**Noise levels:** 90 (10% loss), 70 (30% loss), 50 (50% loss)

---

### Experiment 3b: Network Partition (Split-Brain)

**Location:** `experiments/network_partition/`

| Command | Description |
|---------|-------------|
| `./experiments/network_partition/run_partition_trials.sh 50` | 100 trials, 50 nodes (25+25 split) |
| `./experiments/network_partition/run_partition_trials.sh 100 200` | 200 trials, 100 nodes |
| `./experiments/network_partition/run_partition_trials.sh 10 50 90` | 50 trials, 10 nodes, 90s duration |
| `./experiments/network_partition/run_partition_trials.sh 50 100 60 4` | 100 trials, 4 parallel jobs |
| `./experiments/network_partition/run_partition_trials.sh 50 --fixed-seed` | Fixed seed (reproducible) |
| `./experiments/network_partition/create-partition-plot.sh` | Generate plots (auto-detect latest) |

**Node counts:** 5, 10, 50, 100
**Partition splits:** 5 (2+3), 10 (5+5), 50 (25+25), 100 (50+50)

---

### Run All Experiments (Complete Suite)

Use the master script to run all experiments at once:

```bash
cd examples/bully

# Run ALL experiments with defaults (100 trials each, all node counts)
./experiments/run_all_experiments.sh

# Run with custom number of trials
./experiments/run_all_experiments.sh --trials 50

# Run only specific node counts
./experiments/run_all_experiments.sh --nodes 5,10

# Run only specific experiments
./experiments/run_all_experiments.sh --experiments convergence,noise

# Preview what would run (dry run)
./experiments/run_all_experiments.sh --dry-run

# Full customization
./experiments/run_all_experiments.sh --trials 200 --nodes 50,100 --parallel 8
```

**Available options:**
| Option | Description | Default |
|--------|-------------|---------|
| `--trials N` | Number of trials per experiment | 100 |
| `--nodes LIST` | Comma-separated node counts | 5,10,50,100 |
| `--experiments E` | Experiments to run: convergence, fault_tolerance, noise, partition, all | all |
| `--noise-levels L` | Noise levels for noise experiments | 90,70,50 |
| `--parallel N` | Number of parallel jobs | auto-detect |
| `--dry-run` | Preview commands without executing | - |

**Manual execution** (if you prefer running individually):

```bash
cd examples/bully

# Run all convergence trials for all node counts
for nodes in 5 10 50 100; do
    ./experiments/convergence/run_convergence_trials.sh $nodes
done

# Run all crash recovery trials for all node counts
for nodes in 5 10 50 100; do
    ./experiments/fault_tolerance/run_crash_trials.sh $nodes
done

# Run all noise trials for all node counts and noise levels
for nodes in 5 10 50 100; do
    for noise in 90 70 50; do
        ./experiments/noise/run_noise_trials.sh $nodes $noise
    done
done

# Run all partition trials for all node counts
for nodes in 5 10 50 100; do
    ./experiments/network_partition/run_partition_trials.sh $nodes
done
```

### Generate All Plots

Use the master script to generate all plots at once:

```bash
cd examples/bully

# Generate ALL plots (auto-detects most recent results)
./experiments/generate_all_plots.sh

# Generate only specific experiment plots
./experiments/generate_all_plots.sh --experiments convergence,noise
```

**Manual execution** (if you prefer running individually):

```bash
cd examples/bully

# Generate convergence plots
./experiments/convergence/create-convergence-plot.sh

# Generate fault tolerance plots
./experiments/fault_tolerance/create-fault-tolerance-plot.sh

# Generate noise plots
./experiments/noise/create-noise-plot.sh

# Generate partition plots
./experiments/network_partition/create-partition-plot.sh
```

## References

- Thesis Exposé: See Section 6 "Evaluation" for metrics definitions
- Bully Algorithm: [README.md](README.md) for algorithm description
- Contiki-NG Documentation: https://docs.contiki-ng.org/
- Cooja Simulator: https://docs.contiki-ng.org/en/develop/doc/tutorials/Running-Contiki-NG-in-Cooja.html
