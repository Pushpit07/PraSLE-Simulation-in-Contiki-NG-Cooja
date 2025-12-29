# Leader Election Experiments Guide

This document describes how to run and analyze experiments for all leader election algorithms in the framework.

## Table of Contents

1. [Experiment Types](#experiment-types)
2. [Overview](#overview)
3. [Metrics Collected](#metrics-collected)
4. [Quick Start](#quick-start)
5. [Running Experiments](#running-experiments)
6. [Analyzing Results](#analyzing-results)
7. [Visualization](#visualization)
8. [Troubleshooting](#troubleshooting)

## Supported Algorithms

| Algorithm | Description | Network Stack |
|-----------|-------------|---------------|
| `bully` | Classic Bully algorithm with highest-ID wins | IPv6/UDP |
| `ring` | Ring-based leader election | nullnet |
| `prasle` | PraSLE self-stabilizing algorithm | nullnet |
| `adaptive-prasle` | Customized PraSLE variant | nullnet |

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
| `--parallel, -p` | Number of parallel jobs | auto-detect |
| `--dry-run` | Preview commands without executing | - |

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

## References

- Bully Algorithm: Garcia-Molina, H. (1982). "Elections in a Distributed Computing System"
- PraSLE: Self-stabilizing probabilistic leader election
- Contiki-NG Documentation: https://github.com/contiki-ng/contiki-ng
- Cooja Simulator Guide: Contiki-NG Wiki
