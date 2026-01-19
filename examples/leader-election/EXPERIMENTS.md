# Leader Election Experiments Guide

This document describes how to run and analyze experiments for all leader election algorithms in the framework.

## Table of Contents

1. [Supported Algorithms](#supported-algorithms)
2. [Experiment Types](#experiment-types)
3. [Overview](#overview)
4. [Metrics Collected](#metrics-collected)
5. [Quick Start](#quick-start)
6. [Running Experiments](#running-experiments)
7. [Random Seeds vs Fixed Seeds](#random-seeds-vs-fixed-seeds)
8. [Analyzing Results](#analyzing-results)
9. [Visualization](#visualization)
10. [Quick Reference: All Available Commands](#quick-reference-all-available-commands)
11. [Experiment Design Recommendations](#experiment-design-recommendations)
12. [Expected Algorithm Behavior](#expected-algorithm-behavior)
13. [Statistical Analysis Recommendations](#statistical-analysis-recommendations)
14. [Troubleshooting](#troubleshooting)
15. [Cross-Algorithm Comparison](#cross-algorithm-comparison)
16. [Key Metrics by Experiment Type](#key-metrics-by-experiment-type)
17. [Run All Experiments (Complete Suite)](#run-all-experiments-complete-suite)
18. [Next Steps](#next-steps)
19. [References](#references)

## Supported Algorithms

| Algorithm | Description | Network Stack |
|-----------|-------------|---------------|
| `bully` | Classic Bully algorithm with highest-ID wins | IPv6/UDP |
| `ring` | Chang-Roberts ring-based leader election | IPv6/UDP |
| `prasle` | PraSLE self-stabilizing algorithm | IPv6/UDP |
| `adaptive-prasle` | Adaptive PraSLE with RTT-based timeouts | IPv6/UDP |

**Topology Variants:** PraSLE and Adaptive-PraSLE support multiple network topologies. Use the topology suffix when running experiments:
- `prasle-line`, `prasle-ring`, `prasle-mesh` (default: clique)
- `adaptive-prasle-line`, `adaptive-prasle-ring`, `adaptive-prasle-mesh` (default: clique)

This gives a total of **10 algorithm variants** for comprehensive evaluation.

## Experiment Types

The experiment suite is organized into four categories to comprehensively evaluate leader election algorithms:

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
- Baseline performance comparison across algorithms
- Scalability analysis
- Statistical validation of convergence properties

---

### 2. Leader Crash Recovery (Fault Tolerance)
**Location:** `experiments/fault_tolerance/`

**Purpose:** Evaluate the algorithm's ability to detect leader failures and elect a new coordinator.

**Key Metrics:**
- Failure detection time
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
- **Network Partitions:** Split network into isolated groups
- **Dynamic Disruption:** Add/remove noise during simulation

**Use Cases:**
- Real-world deployment evaluation
- Robustness analysis
- Comparison with partition-tolerant algorithms

---

## Overview

All leader election algorithm implementations include comprehensive metrics collection to evaluate:

- **Convergence Time**: How quickly the network elects an initial leader
- **Communication Overhead**: Number and types of messages exchanged
- **Election Stability**: Frequency of elections and leader changes
- **State Distribution**: Time spent in different algorithm states
- **Fault Tolerance**: Behavior during coordinator failures
- **Scalability**: Performance across different network sizes (5, 10, 50, 100 nodes)

### Network Configurations

The experiment suite includes four different network sizes to evaluate scalability:
- **5 nodes**: Small network baseline (3x2 grid, 40m spacing)
- **10 nodes**: Medium-small network (2x5 grid, 22m spacing)
- **50 nodes**: Medium-large network (5x10 grid, 10m spacing)
- **100 nodes**: Large network (10x10 grid, 8m spacing)

All configurations use a dense network topology where all nodes are within radio range (100m), ensuring single-hop communication.

## Metrics Collected

### Election Metrics
- `elections_started`: Total elections initiated by each node
- `elections_won`: Elections won (node became coordinator)
- `elections_lost`: Elections lost (received higher-priority response)
- `first_convergence_time`: Time to first leader election (cold start)
- `total_election_time`: Cumulative time spent in election state

### Message Metrics
- `msg_election_sent/recv`: Election messages sent/received
- `msg_answer_sent/recv`: Answer messages sent/received
- `msg_coordinator_sent/recv`: Coordinator messages sent/received
- `msg_alive_sent/recv`: Alive heartbeat messages sent/received
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
- `alive_resets`: Times alive messages reset the timeout timer
- `coordinator_reannouncements`: Coordinator re-announcements (partition healing)
- `alive_adoptions`: Adoptions via alive discovery (partition healing)

## Quick Start

### Prerequisites

1. **Build Contiki-NG and Cooja**:
   ```bash
   cd /path/to/contiki-ng
   git submodule update --init --recursive
   cd tools/cooja
   ./gradlew jar
   ```

2. **Build the desired algorithm**:
   ```bash
   cd examples/leader-election
   make ALGORITHM=bully TARGET=cooja
   # Or: make ALGORITHM=ring TARGET=cooja
   # Or: make ALGORITHM=prasle TARGET=cooja
   # Or: make ALGORITHM=adaptive-prasle TARGET=cooja
   ```

3. **Install Python dependencies** (for analysis scripts):
   ```bash
   pip install matplotlib pandas numpy
   ```

### Running a Simple Experiment

```bash
cd examples/leader-election/scripts

# Run a 5-minute experiment with 5 nodes using bully algorithm
python3 run_experiment.py \
    --simulation ../experiments/convergence/csc_templates/bully/5nodes.csc \
    --duration 300 \
    --output ../results/bully/test_run \
    --algorithm bully

# Or run with ring algorithm
python3 run_experiment.py \
    --simulation ../experiments/convergence/csc_templates/ring/5nodes.csc \
    --duration 300 \
    --output ../results/ring/test_run \
    --algorithm ring
```

This will:
1. Run the Cooja simulation for 300 seconds
2. Capture all log output
3. Extract metrics to CSV
4. Generate summary statistics

## Running Experiments

### Option 1: Master Experiment Runner

Run experiments for any algorithm using the master script:

```bash
cd examples/leader-election/experiments

# Run all experiments for bully algorithm
./run_all_experiments.sh --algorithm bully --experiments convergence,fault_tolerance,noise,network_partition

# Run convergence experiments for all algorithms
./run_all_experiments.sh --algorithm all --experiments convergence --trials 100

# Dry run to preview commands
./run_all_experiments.sh --algorithm bully --dry-run
```

**Options:**

| Option | Description | Default |
|--------|-------------|---------|
| `--algorithm, -a` | Algorithm (bully, ring, prasle, adaptive-prasle, all) | all |
| `--trials, -t` | Number of trials per configuration | 10 |
| `--nodes, -n` | Comma-separated node counts | 5,10,50,100 |
| `--experiments, -e` | Comma-separated experiment types | convergence |
| `--topology` | Network topology for PraSLE (clique, ring, line, mesh) | clique |
| `--parallel, -p` | Number of parallel jobs | auto-detect |
| `--dry-run` | Preview commands without executing | - |

### Running PraSLE with Different Topologies

PraSLE supports multiple network topologies. Each topology has different convergence characteristics:

| Topology | K Rounds | Convergence Time (100 nodes) | Description |
|----------|----------|------------------------------|-------------|
| `clique` | K=2 (constant) | ~1.3s | Fully connected - O(1) |
| `ring` | K=(N+1)/2 | ~25s | Circular ring - O(N/2) |
| `line` | K=N | ~50s | Linear chain - O(N) |
| `mesh` | K≈2√N | ~10s | 2D grid - O(√N) |

**Running experiments with different topologies:**

```bash
cd examples/leader-election/experiments

# Run PraSLE with ring topology
./run_all_experiments.sh --algorithm prasle --topology ring --experiments convergence

# Run PraSLE with mesh topology, specific node counts
./run_all_experiments.sh --algorithm prasle --topology mesh --nodes 10,50 --trials 50

# Run PraSLE with line topology (longest convergence time)
./run_all_experiments.sh --algorithm prasle --topology line --nodes 5,10 --experiments convergence
```

**Note:** Non-clique topologies will automatically:
- Generate appropriate CSC templates with restricted radio ranges
- Set K values proportional to network diameter
- Adjust experiment durations accordingly

Results for different topologies are saved in separate directories:
- `results/prasle/` - clique topology (default)
- `results/prasle-ring/` - ring topology
- `results/prasle-mesh/` - mesh topology
- `results/prasle-line/` - line topology

### Option 2: Individual Experiment Scripts

#### Convergence Experiments

```bash
cd examples/leader-election/experiments/convergence

# Run 100 trials with 50 nodes for bully algorithm
./run_convergence_trials.sh bully 50 100

# Run 50 trials with 10 nodes for ring algorithm, 90s duration
./run_convergence_trials.sh ring 10 50 90

# Run with fixed seed for reproducibility
./run_convergence_trials.sh prasle 50 100 60 4 --fixed-seed
```

**Parameters:**

| Parameter | Description | Default |
|-----------|-------------|---------|
| algorithm | Algorithm to test (required) | - |
| node_count | Number of nodes: 5, 10, 50, or 100 (required) | - |
| trials | Number of trial runs | 100 |
| duration | Duration per trial in seconds | 60 |
| parallel_jobs | Number of parallel jobs | auto-detect |
| --fixed-seed | Use same seed for all trials | random seeds |

**Output:** `results/{algorithm}/convergence_trials/{nodes}nodes_{timestamp}/`

#### Fault Tolerance Experiments

```bash
cd examples/leader-election/experiments/fault_tolerance

# Run crash recovery test with 50 nodes
./run_crash_trials.sh bully 50 100

# Crash leader after 30 seconds, run for 120 seconds total
./run_crash_trials.sh ring 10 50 30 120

# Custom parallel jobs
./run_crash_trials.sh prasle 100 100 60 180 8
```

**Parameters:**

| Parameter | Description | Default |
|-----------|-------------|---------|
| algorithm | Algorithm to test (required) | - |
| node_count | Number of nodes: 5, 10, 50, or 100 (required) | - |
| trials | Number of trial runs | 100 |
| crash_time | Time to crash leader (seconds) | 60 |
| duration | Total duration per trial (seconds) | 120 |
| parallel_jobs | Number of parallel jobs | auto-detect |

**Output:** `results/{algorithm}/fault_tolerance/{nodes}nodes_crash{time}s_{timestamp}/`

#### Noise Experiments

```bash
cd examples/leader-election/experiments/noise

# Run with 50% packet success rate
./run_noise_trials.sh bully 50 50 100

# Run with 70% packet success rate
./run_noise_trials.sh ring 10 70 50 90

# Run with 90% packet success rate (light noise)
./run_noise_trials.sh prasle 100 90 100 120 8
```

**Noise Levels:**

| Level | Success Ratio | Description |
|-------|---------------|-------------|
| 50 | 0.5 | 50% packet delivery (heavy noise) |
| 70 | 0.7 | 70% packet delivery |
| 90 | 0.9 | 90% packet delivery (light noise) |

**Output:** `results/{algorithm}/noise/{nodes}nodes_noise{level}_{timestamp}/`

#### Network Partition Experiments

```bash
cd examples/leader-election/experiments/network_partition

# Run partition test with 10 nodes
./run_partition_trials.sh bully 10 100

# Run with 50 nodes
./run_partition_trials.sh ring 50 50 90
```

**Partition Configuration:**

| Nodes | Partition A | Partition B |
|-------|-------------|-------------|
| 5 | Nodes 1-2 | Nodes 3-5 |
| 10 | Nodes 1-5 | Nodes 6-10 |
| 50 | Nodes 1-25 | Nodes 26-50 |
| 100 | Nodes 1-50 | Nodes 51-100 |

**Output:** `results/{algorithm}/network_partition/{nodes}nodes_partition_{timestamp}/`

## Random Seeds vs Fixed Seeds

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

Example with all trial scripts:
```bash
# Convergence trials
./experiments/convergence/run_convergence_trials.sh bully 50 --fixed-seed

# Crash recovery trials
./experiments/fault_tolerance/run_crash_trials.sh ring 50 --fixed-seed

# Noise resilience trials
./experiments/noise/run_noise_trials.sh prasle 50 90 --fixed-seed

# Partition trials
./experiments/network_partition/run_partition_trials.sh adaptive-prasle 50 --fixed-seed
```

## Analyzing Results

### Using the Parser Script

Analyze metrics from a single experiment:

```bash
cd examples/leader-election/scripts

# Print summary to console
python3 parse_metrics.py ../results/bully/test/metrics.csv --summary -a bully

# Export detailed analysis
python3 parse_metrics.py ../results/ring/test/metrics.csv \
    --output ../results/ring/test/analysis \
    -a ring
```

This generates:
- `convergence.csv`: Convergence time per node
- `message_overhead.csv`: Message counts per node
- `elections.csv`: Election statistics per node

### Example Summary Output

```
======================================================================
 LEADER ELECTION ALGORITHM METRICS ANALYSIS
======================================================================
Algorithm: bully

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

### Experiment-Specific Analysis

#### Fault Tolerance Analysis

```bash
cd examples/leader-election/experiments/fault_tolerance

# Analyze crash recovery results
python3 analyze_recovery.py -i ../../results/bully/fault_tolerance/50nodes_crash60s_*/crash_recovery_times.csv

# Specify algorithm explicitly
python3 analyze_recovery.py -i results.csv -a ring -o output_dir/

# Skip plot generation
python3 analyze_recovery.py -i results.csv --no-plots
```

#### Noise Analysis

```bash
cd examples/leader-election/experiments/noise

# Analyze noise experiment results
python3 analyze_noise.py -i ../../results/bully/noise/50nodes_noise50_*/noise_convergence_times.csv

# With algorithm specification
python3 analyze_noise.py -i results.csv -a prasle -o output_dir/
```

#### Network Partition Analysis

```bash
cd examples/leader-election/experiments/network_partition

# Analyze partition results
python3 analyze_partition.py -i ../../results/bully/network_partition/50nodes_partition_*/partition_times.csv

# With algorithm specification
python3 analyze_partition.py -i results.csv -a ring -o output_dir/
```

## Visualization

### Generate All Plots

```bash
cd examples/leader-election/experiments

# Generate plots for all algorithms and experiments
./generate_all_plots.sh

# Generate plots for specific algorithm
./generate_all_plots.sh --algorithm bully

# Generate specific experiment plots
./generate_all_plots.sh --experiments convergence,fault_tolerance
```

### Individual Plot Scripts

```bash
# Convergence distribution plot
cd experiments/convergence
./create-convergence-plot.sh -a bully

# Fault tolerance plots
cd experiments/fault_tolerance
./create-fault-tolerance-plot.sh -a ring

# Noise plots
cd experiments/noise
./create-noise-plot.sh -a prasle

# Partition plots
cd experiments/network_partition
./create-partition-plot.sh -a adaptive-prasle
```

### Plot Types Generated

- `convergence.png`: Convergence time per node (bar chart)
- `message_overhead.png`: Message overhead by type (stacked bar chart)
- `elections.png`: Election statistics (grouped bar chart)
- `state_distribution.png`: Time in each state (stacked bar chart)
- `leader_timeline.png`: Leader changes over time (line plot)
- `message_timeline.png`: Message activity over time (dual line plots)

## Quick Reference: All Available Commands

### Convergence Experiments

| Command | Description |
|---------|-------------|
| `./run_convergence_trials.sh bully 50` | 100 trials with 50 nodes |
| `./run_convergence_trials.sh ring 100 200` | 200 trials with 100 nodes |
| `./run_convergence_trials.sh prasle 5 50 90` | 50 trials, 5 nodes, 90s duration |
| `./run_convergence_trials.sh adaptive-prasle 50 100 60 4` | 100 trials, 4 parallel jobs |
| `./run_convergence_trials.sh bully 50 --fixed-seed` | Fixed seed (reproducible) |
| `./create-convergence-plot.sh -a bully` | Generate plots |

### Fault Tolerance Experiments

| Command | Description |
|---------|-------------|
| `./run_crash_trials.sh bully 50` | 100 trials, crash at 60s, 120s total |
| `./run_crash_trials.sh ring 100 200` | 200 trials with 100 nodes |
| `./run_crash_trials.sh prasle 10 50 30 90` | 50 trials, crash at 30s, 90s total |
| `./run_crash_trials.sh adaptive-prasle 50 100 60 120 4` | 100 trials, 4 parallel jobs |
| `./create-fault-tolerance-plot.sh -a ring` | Generate plots |

### Noise Experiments

| Command | Description |
|---------|-------------|
| `./run_noise_trials.sh bully 50 90` | 100 trials, 90% success (10% loss) |
| `./run_noise_trials.sh ring 100 70 200` | 200 trials, 70% success |
| `./run_noise_trials.sh prasle 10 50 50 90` | 50 trials, 50% success, 90s |
| `./run_noise_trials.sh adaptive-prasle 50 90 100 60 4` | 100 trials, 4 parallel jobs |
| `./create-noise-plot.sh -a prasle` | Generate plots |

### Network Partition Experiments

| Command | Description |
|---------|-------------|
| `./run_partition_trials.sh bully 50` | 100 trials, 50 nodes (25+25 split) |
| `./run_partition_trials.sh ring 100 200` | 200 trials, 100 nodes |
| `./run_partition_trials.sh prasle 10 50 90` | 50 trials, 10 nodes, 90s duration |
| `./run_partition_trials.sh adaptive-prasle 50 100 60 4` | 100 trials, 4 parallel jobs |
| `./create-partition-plot.sh -a ring` | Generate plots |

## Experiment Design Recommendations

### For Baseline Metrics

Run multiple trials to account for randomness:

```bash
# Run 5 trials for each node count
for nodes in 5 10 50 100; do
    for trial in {1..5}; do
        python3 scripts/run_experiment.py \
            --simulation experiments/convergence/csc_templates/bully/${nodes}nodes.csc \
            --duration 600 \
            --output results/bully/trials/${nodes}nodes_trial_${trial} \
            --algorithm bully
    done
done
```

Then compute aggregate statistics across trials.

### For Scalability Studies

The multi-node experiment suite is designed to evaluate scalability:

```bash
# Run convergence trials for all node counts
for nodes in 5 10 50 100; do
    ./experiments/convergence/run_convergence_trials.sh bully $nodes
done
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
- The algorithm being compared
- Network size (when studying scalability)
- Specific parameters being evaluated

### Statistical Validity

For publishable results:
- Run at least **10-100 trials** per configuration
- Compute mean, median, and standard deviation
- Use appropriate statistical tests (e.g., t-test, ANOVA)
- Report confidence intervals

---

## Expected Algorithm Behavior

### Bully Algorithm

Theoretical complexity:
- **Message complexity**: O(n²) per election in worst case
- **Time complexity**: O(n) rounds for convergence
- **Convergence**: Deterministic, guaranteed in asynchronous networks

### Ring Algorithm

Theoretical complexity:
- **Message complexity**: O(n) messages per election (single ring traversal)
- **Time complexity**: O(n) message delays for convergence
- **Convergence**: Deterministic, requires ring topology

### PraSLE Algorithm

Theoretical complexity:
- **Message complexity**: O(n) per round (probabilistic)
- **Time complexity**: O(log n) expected rounds for convergence
- **Convergence**: Probabilistic with high probability, self-stabilizing

Compare experimental results against these theoretical bounds to validate implementation correctness.

---

## Statistical Analysis Recommendations

For publishable results:
- Run at least **10-100 trials** per configuration
- Compute mean, median, and standard deviation
- Use appropriate statistical tests (e.g., t-test, ANOVA)
- Report confidence intervals

### Scalability Analysis

When analyzing scalability, focus on:

1. **Convergence Time vs Node Count**
   - Does convergence time increase linearly, quadratically, or logarithmically?
   - What is the convergence time for 5 vs 100 nodes?

2. **Message Overhead vs Node Count**
   - Total messages sent/received per node
   - Does overhead increase as O(n) or O(n^2)?

3. **Election Frequency vs Node Count**
   - More nodes = more potential failures
   - How does election frequency scale?

4. **Network Stability vs Node Count**
   - Leader change frequency
   - Time to stabilize after cold start

### Algorithm Comparison

Keep these parameters constant across algorithm comparisons:
- Network topology (dense single-hop for all node counts)
- Radio model and parameters (100m range)
- Simulation duration
- Timing parameters (unless studying their effect)

Vary only:
- The algorithm being compared
- Network size (when studying scalability)
- Specific parameters being evaluated

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
- Check algorithm source: `#define ENABLE_METRICS 1`
- Rebuild: `make ALGORITHM=bully TARGET=cooja clean && make ALGORITHM=bully TARGET=cooja`
- Verify simulation file loads in Cooja GUI
- Check Cooja output log for errors

### Python Dependencies Missing

**Error**: `ImportError: No module named 'pandas'`

**Solution**:
```bash
pip install matplotlib pandas numpy
# or
pip3 install matplotlib pandas numpy
```

### Simulation Hangs

**Problem**: Simulation doesn't complete

**Solutions**:
- Check simulation duration in .csc file
- Verify nodes are properly configured
- Run in Cooja GUI to debug
- Check system resources (memory, CPU)

### CSC Templates Not Found

**Error**: Missing CSC template for algorithm/node count

**Solution**:
```bash
cd examples/leader-election/scripts
python3 generate_csc.py --experiment all --output ../experiments/
```

### Algorithm Build Fails

**Problem**: Compilation errors when building algorithm

**Solutions**:
- Ensure Contiki-NG is properly installed
- Check that the `CONTIKI` path in Makefile is correct
- Verify you're using a valid algorithm name: bully, ring, prasle, adaptive-prasle
- Clean and rebuild: `make ALGORITHM=<algo> TARGET=cooja clean && make ALGORITHM=<algo> TARGET=cooja`

## Cross-Algorithm Comparison

After running experiments for all algorithms:

```bash
cd examples/leader-election/experiments/comparison

# Run cross-algorithm comparison analysis
python3 compare_algorithms.py
```

This generates comparison plots showing:
- Convergence time across algorithms
- Message overhead comparison
- Scalability trends
- Statistical comparisons with error bars

## Key Metrics by Experiment Type

### Convergence Experiment Key Metrics

1. **Initial Convergence Time**
   - Time from simulation start to first leader election
   - Measured in milliseconds
   - Should be consistent across trials

2. **Message Overhead**
   - Total messages exchanged during election
   - Compare against theoretical O(n²) or O(n) bounds

3. **Election Count**
   - Number of elections before stabilization
   - Should be minimal (1-3 elections typically)

### Fault Tolerance Key Metrics

1. **Failure Detection Time**
   - Time from leader crash to detection
   - Should match configured timeout (e.g., ~20s for Bully)

2. **Re-Election Time**
   - Time from detection to new leader election
   - Similar to initial convergence time

3. **Total Recovery Time**
   - End-to-end: crash → detection → re-election
   - Critical for service availability
   - Formula: `timeout + convergence_time`

4. **Success Rate**
   - Percentage of trials with successful recovery
   - Should be 100% for robust algorithm

### Noise Experiment Key Metrics

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

4. **Variance/Consistency**
   - Standard deviation and CV (coefficient of variation)
   - Higher noise → higher variance expected

### Network Partition Key Metrics

1. **Convergence Time per Partition**
   - How quickly does each partition elect its leader?
   - Should be similar to normal election (no interference)

2. **Leader Correctness**
   - Did each partition elect the highest-priority node?
   - Expected: 100% correct leader selection

3. **Split-Brain Detection**
   - Did both partitions elect independent leaders?
   - Expected: 100% split-brain (by design)

---

## Run All Experiments (Complete Suite)

### Complete Evaluation Script (Recommended)

The `run_complete_evaluation.sh` script is the recommended way to run a full evaluation:

```bash
cd examples/leader-election/experiments

# Run full evaluation (all 10 algorithm variants, all experiments)
./run_complete_evaluation.sh

# Quick test with 10 trials
./run_complete_evaluation.sh -t 10

# Run specific experiments for specific algorithms
./run_complete_evaluation.sh -e convergence -a bully,ring,adaptive-prasle -t 50

# Generate charts only (from existing results)
./run_complete_evaluation.sh --skip-experiments

# Run experiments only (no chart generation)
./run_complete_evaluation.sh --skip-charts
```

**Key Features:**
- Supports all 10 algorithm variants (including PraSLE and Adaptive-PraSLE topology variants)
- **Automatically generates missing CSC templates** - no manual setup required
- Generates comparison charts after experiments complete
- Provides detailed progress reporting

**Supported Algorithms (10 total):**
- `bully`, `ring`
- `prasle`, `prasle-line`, `prasle-ring`, `prasle-mesh`
- `adaptive-prasle`, `adaptive-prasle-line`, `adaptive-prasle-ring`, `adaptive-prasle-mesh`

### Basic Experiment Runner

For simpler use cases, use `run_all_experiments.sh`:

```bash
cd examples/leader-election/experiments

# Run ALL experiments for a specific algorithm
./run_all_experiments.sh --algorithm bully

# Run ALL experiments for all algorithms
./run_all_experiments.sh --algorithm all

# Run with custom number of trials
./run_all_experiments.sh --algorithm bully --trials 50

# Run only specific node counts
./run_all_experiments.sh --algorithm ring --nodes 5,10

# Run only specific experiments
./run_all_experiments.sh --algorithm prasle --experiments convergence,noise

# Preview what would run (dry run)
./run_all_experiments.sh --algorithm bully --dry-run

# Full customization
./run_all_experiments.sh --algorithm all --trials 200 --nodes 50,100 --parallel 8
```

**Manual execution** (if you prefer running individually):

```bash
cd examples/leader-election

# Run all convergence trials for all algorithms and node counts
for algo in bully ring prasle adaptive-prasle; do
    for nodes in 5 10 50 100; do
        ./experiments/convergence/run_convergence_trials.sh $algo $nodes
    done
done

# Run all crash recovery trials
for algo in bully ring prasle adaptive-prasle; do
    for nodes in 5 10 50 100; do
        ./experiments/fault_tolerance/run_crash_trials.sh $algo $nodes
    done
done

# Run all noise trials
for algo in bully ring prasle adaptive-prasle; do
    for nodes in 5 10 50 100; do
        for noise in 90 70 50; do
            ./experiments/noise/run_noise_trials.sh $algo $nodes $noise
        done
    done
done

# Run all partition trials
for algo in bully ring prasle adaptive-prasle; do
    for nodes in 5 10 50 100; do
        ./experiments/network_partition/run_partition_trials.sh $algo $nodes
    done
done
```

### Generate All Plots

```bash
cd examples/leader-election/experiments

# Generate ALL plots for all algorithms (auto-detects most recent results)
./generate_all_plots.sh

# Generate plots for specific algorithm
./generate_all_plots.sh --algorithm bully

# Generate only specific experiment plots
./generate_all_plots.sh --experiments convergence,noise
```

---

## Next Steps

After collecting baseline metrics across all algorithms and node counts:

1. **Analyze scalability**: Compare metrics across 5, 10, 50, and 100 node configurations for each algorithm

2. **Compare algorithms**: Analyze across all implemented algorithms
   - Convergence time vs network size
   - Message overhead vs network size
   - Stability and fault tolerance
   - Scalability characteristics

3. **Identify trade-offs**: Document algorithm-specific strengths and weaknesses
   - Bully: Simple, deterministic, but O(n²) messages
   - Ring: O(n) messages, but requires ring topology
   - PraSLE: Self-stabilizing, probabilistic convergence

4. **Implement extensions**: Consider algorithm enhancements
   - Energy-aware leader election
   - Link-quality-aware priority
   - Adaptive timeouts based on network conditions

5. **Evaluate extensions**: Compare extended versions vs baselines
   - Does extension improve convergence time?
   - What is the overhead cost?
   - How does it affect fault tolerance?

6. **Publication preparation**:
   - Generate publication-quality plots
   - Compute confidence intervals
   - Perform statistical significance tests
   - Document methodology and results

---

## References

- **Bully Algorithm**: Garcia-Molina, H. (1982). "Elections in a Distributed Computing System"
- **Ring Algorithm**: Chang, E. and Roberts, R. (1979). "An Improved Algorithm for Decentralized Extrema-Finding in Circular Configurations of Processes"
- **PraSLE**: Self-stabilizing probabilistic leader election for wireless sensor networks
- **Contiki-NG Documentation**: https://github.com/contiki-ng/contiki-ng
- **Cooja Simulator Guide**: https://docs.contiki-ng.org/en/develop/doc/tutorials/Running-Contiki-NG-in-Cooja.html
