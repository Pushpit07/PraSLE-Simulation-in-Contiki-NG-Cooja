# Bully Algorithm Experiments Guide

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

These metrics align with the evaluation criteria in the thesis exposé for comparison with PraSLE and other leader election protocols.

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

# Run a 5-minute experiment
python3 run_experiment.py \
    --simulation ../bully-cooja.csc \
    --duration 300 \
    --output ../results/test_run1
```

This will:
1. Run the Cooja simulation for 300 seconds
2. Capture all log output
3. Extract metrics to CSV
4. Generate summary statistics

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

### Option 2: Automated Scenario Suite

Run multiple pre-configured scenarios:

```bash
cd scripts
./run_scenarios.sh results/baseline_2025
```

This runs:
1. **Scenario 1** (5 min): Cold start election
2. **Scenario 2** (15 min): Extended runtime for stability
3. **Scenario 3** (30 min): Long-term monitoring

Each scenario produces:
- Raw Cooja logs
- Extracted metrics CSV
- Summary statistics
- Analysis files
- Visualization plots

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
for i in {1..5}; do
    python3 scripts/run_experiment.py \
        --simulation bully-cooja.csc \
        --duration 600 \
        --output results/baseline_trial_$i
done
```

Then compute aggregate statistics across trials.

### For Comparison Studies

Keep these parameters constant across experiments:
- Network size (number of nodes)
- Network topology
- Radio model and parameters
- Simulation duration
- Timing parameters (unless studying their effect)

Vary only the algorithm or specific parameters being studied.

### Statistical Validity

For publishable results:
- Run at least **5-10 trials** per configuration
- Compute mean, median, and standard deviation
- Use appropriate statistical tests (e.g., t-test, ANOVA)
- Report confidence intervals

## Next Steps

After collecting Bully baseline metrics:

1. **Implement PraSLE**: Implement PraSLE algorithm in Contiki-NG
2. **Run PraSLE experiments**: Use same experimental setup
3. **Compare results**: Convergence time, message overhead, stability
4. **Implement extensions**: Energy-aware, link-quality-aware, adaptive timeouts
5. **Evaluate extensions**: Compare extended PraSLE vs baseline PraSLE vs Bully

## References

- Thesis Exposé: See Section 6 "Evaluation" for metrics definitions
- Bully Algorithm: [README.md](README.md) for algorithm description
- Contiki-NG Documentation: https://docs.contiki-ng.org/
- Cooja Simulator: https://docs.contiki-ng.org/en/develop/doc/tutorials/Running-Contiki-NG-in-Cooja.html
