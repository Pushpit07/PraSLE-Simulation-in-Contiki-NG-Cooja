# Leader Election Algorithm Framework

A comprehensive framework for evaluating leader election algorithms in wireless sensor networks using Contiki-NG and the Cooja simulator.

## Table of Contents

- [Quick Start](#quick-start)
- [Overview](#overview)
- [Leader Election Concepts](#leader-election-concepts)
- [Supported Algorithms](#supported-algorithms)
- [Directory Structure](#directory-structure)
- [Building](#building)
  - [Fast Mode (Quick Testing)](#fast-mode-quick-testing)
  - [Understanding Timing Parameters](#understanding-timing-parameters)
  - [Opening in Cooja GUI](#opening-in-cooja-gui)
- [Running Experiments](#running-experiments)
  - [Master Experiment Runner](#master-experiment-runner)
  - [Convergence Experiments](#convergence-experiments)
  - [Fault Tolerance Experiments](#fault-tolerance-experiments)
  - [Noise Experiments](#noise-experiments)
  - [Network Partition Experiments](#network-partition-experiments)
- [Analysis Scripts](#analysis-scripts)
- [Plot Generation](#plot-generation)
- [CSC Template Generation](#csc-template-generation)
- [Results Structure](#results-structure)
- [Examples](#examples)
- [Quick Metrics Collection](#quick-metrics-collection)
- [Network Partitioning and Split-Brain Behavior](#network-partitioning-and-split-brain-behavior)
- [Testing Scenarios](#testing-scenarios)
- [Educational Value](#educational-value)
- [Advanced: Multi-Hop Routing](#advanced-multi-hop-routing)
- [References](#references)

## Additional Documentation

- **[EXPERIMENTS.md](EXPERIMENTS.md)** - Detailed experiment guide with metrics definitions, statistical analysis, and troubleshooting

---

## Quick Start

Get up and running in 3 steps:

### Step 1: Build an Algorithm

```bash
cd examples/leader-election

# Interactive mode - shows menu to select algorithm
./build.sh

# Or specify directly
./build.sh bully              # Build Bully algorithm
./build.sh ring               # Build Ring algorithm
./build.sh prasle             # Build PraSLE algorithm
./build.sh adaptive-prasle    # Build Adaptive-PraSLE algorithm
./build.sh all                # Build all algorithms
./build.sh clean              # Clean all builds
```

### Step 2: Run a Quick Experiment

```bash
# Collect metrics with a quick test (60 seconds default)
./create-metrics.sh bully

# Or specify duration and output name
./create-metrics.sh bully 300              # 5-minute experiment
./create-metrics.sh ring 120 my_test       # 2-minute experiment with custom name
./create-metrics.sh prasle 60              # 1-minute prasle test
```

### Step 3: Analyze Results

```bash
# Quick analysis with statistics and plots
./experiments/analyze.sh bully

# Or view raw results manually
cat results/bully/metrics/summary.txt
cat results/bully/metrics/metrics.csv

# Generate plots (requires matplotlib)
python3 scripts/plot_results.py results/bully/metrics/
```

### Open in Cooja GUI (Optional)

To observe the simulation visually:

```bash
# Start Cooja
cd ../../tools/cooja
./gradlew run

# In Cooja: File → Open Simulation → Browse to:
# examples/leader-election/experiments/convergence/csc_templates/bully/5nodes.csc
```

Or use quickstart:

```bash
java -jar ../../tools/cooja/dist/cooja.jar \
    -quickstart=experiments/convergence/csc_templates/bully/5nodes.csc
```

---

## Overview

This framework provides tools for:
- Running multiple leader election algorithms under various network conditions
- Collecting metrics (convergence time, message overhead, leader correctness)
- Statistical analysis across multiple trials
- Visualization and comparison of algorithm performance

## Leader Election Concepts

### What is Leader Election?

**Leader election** is a fundamental problem in distributed systems where a group of nodes must agree on a single node to act as the coordinator (leader). The leader typically:
- Coordinates group activities
- Makes decisions on behalf of the group
- Distributes tasks to other nodes
- Acts as a central point for data aggregation

### Why Leader Election Matters

In wireless sensor networks and IoT applications, leader election is critical for:
- **Data aggregation**: Collecting data from multiple sensors
- **Time synchronization**: Coordinating clocks across the network
- **Resource management**: Allocating shared resources
- **Fault tolerance**: Recovering from node failures

### Common Message Types

Most leader election algorithms use similar message types:

| Type | Purpose | Example |
|------|---------|---------|
| **Election** | Initiate an election process | "I'm starting an election" |
| **Response/Answer** | Reply to election messages | "I have higher priority" |
| **Coordinator/Leader** | Announce the new leader | "I am the new leader" |
| **Heartbeat/Alive** | Prove the leader is alive | "I'm still functioning" |

### Common Node States

Leader election algorithms typically use these states:

| State | Description |
|-------|-------------|
| **Normal/Follower** | Regular operation with a known leader |
| **Election/Candidate** | Participating in an election process |
| **Waiting** | Waiting for election results |
| **Leader** | Acting as the coordinator |

### Network Stack

All algorithms in this framework use the **IPv6/UDP stack**:

- **Transport**: Simple UDP (via `simple-udp.h`) on port 8765
- **Network**: IPv6 with link-local multicast (`ff02::1`)
- **Routing**: RPL Lite (lightweight routing protocol for IoT)
- **MAC**: CSMA (Carrier Sense Multiple Access)

**Note**: Link-local multicast (`ff02::1`) only reaches nodes in direct radio range. For multi-hop communication, see the [Advanced: Multi-Hop Routing](#advanced-multi-hop-routing) section.

### Network Topology Requirements

**Important**: The Bully algorithm uses **broadcast** messages for leader election, which requires **single-hop (full-mesh) connectivity** for correct operation. This means all nodes must be within radio range of each other.

**What happens with limited radio range:**
- If radio range only covers nearby neighbors (not all nodes), election messages don't reach distant nodes
- Distant high-priority nodes cannot participate in elections they cannot hear
- The elected leader may not be the globally highest-ID node, only the highest reachable node
- This is **expected behavior**, not a bug - it demonstrates algorithm limitations in multi-hop networks

**CSC template configuration:**
- For **5/10 node** configurations: Default radio range (45m) provides full-mesh connectivity
- For **50/100 node** configurations: Radio range is set to cover the entire grid diagonal (~300-380m)
- This ensures all nodes can hear each other for correct Bully algorithm operation

**For multi-hop networks** where full-mesh connectivity is not possible:
- Consider using the Ring algorithm (messages traverse hop-by-hop around the ring)
- Or implement application-level flooding (see [Advanced: Multi-Hop Routing](#advanced-multi-hop-routing))
- Or accept partition-tolerant behavior where each reachable group elects its own leader

## Supported Algorithms

| Algorithm | Description | Network Stack | Documentation |
|-----------|-------------|---------------|---------------|
| `bully` | Classic Bully algorithm with highest-ID wins | IPv6/UDP | [README](algorithms/bully/README.md) |
| `ring` | Chang-Roberts ring-based leader election | IPv6/UDP | [README](algorithms/ring/README.md) |
| `prasle` | PraSLE self-stabilizing algorithm | IPv6/UDP | [README](algorithms/prasle/README.md) |
| `adaptive-prasle` | Adaptive PraSLE with RTT-based timeouts | IPv6/UDP | [README](algorithms/adaptive-prasle/README.md) |

### Adaptive-PraSLE Features

Adaptive-PraSLE extends the base PraSLE algorithm with intelligent timeout management:

| Feature | Description |
|---------|-------------|
| **RTT-based Timeouts** | Dynamically adjusts timeouts based on measured network round-trip times |
| **Jacobson/Karels Algorithm** | Uses TCP-style RTT estimation with smoothed RTT and variance tracking |
| **Neighbor Activity Detection** | Monitors neighbor responsiveness to detect network conditions |
| **Configurable Safety Margins** | Adjustable timeout multipliers for different reliability requirements |

**Topology Support:** Both PraSLE and Adaptive-PraSLE support multiple network topologies:
- `clique` (default) - Fully connected, K=2 rounds
- `line` - Linear chain, K=N rounds
- `ring` - Circular ring, K=N/2 rounds
- `mesh` - 2D grid, K≈2√N rounds

## Directory Structure

```
leader-election/
├── algorithms/                    # Algorithm implementations
│   ├── bully/
│   ├── ring/
│   ├── prasle/
│   └── adaptive-prasle/
├── common/                        # Shared code
│   ├── election-common.h
│   ├── election-metrics.h
│   └── election-metrics.c
├── experiments/                   # Experiment runners and CSC templates
│   ├── run_complete_evaluation.sh # Full evaluation with auto CSC generation (recommended)
│   ├── run_all_experiments.sh     # Basic experiment runner
│   ├── generate_all_plots.sh      # Master plot generator
│   ├── convergence/
│   │   ├── run_convergence_trials.sh
│   │   ├── create-convergence-plot.sh
│   │   └── csc_templates/{algorithm}/
│   ├── fault_tolerance/
│   │   ├── run_crash_trials.sh
│   │   ├── analyze_recovery.py
│   │   ├── create-fault-tolerance-plot.sh
│   │   └── csc_templates/{algorithm}/
│   ├── noise/
│   │   ├── run_noise_trials.sh
│   │   ├── analyze_noise.py
│   │   ├── create-noise-plot.sh
│   │   └── csc_templates/{algorithm}/
│   ├── network_partition/
│   │   ├── run_partition_trials.sh
│   │   ├── analyze_partition.py
│   │   ├── create-partition-plot.sh
│   │   └── csc_templates/{algorithm}/
│   └── comparison/
│       └── compare_algorithms.py
├── scripts/                       # Utility scripts
│   ├── generate_csc.py            # CSC template generator
│   ├── run_experiment.py          # Single experiment runner
│   ├── parse_metrics.py           # Metrics parser
│   ├── plot_results.py            # Results plotter
│   └── plot_convergence_distribution.py
├── results/                       # Experiment results (generated)
│   └── {algorithm}/
│       ├── convergence/
│       ├── fault_tolerance/
│       ├── noise/
│       └── network_partition/
└── Makefile                       # Build system
```

---

## Building

Build a specific algorithm:

```bash
# Build bully algorithm for Cooja
make ALGORITHM=bully TARGET=cooja

# Build ring algorithm
make ALGORITHM=ring TARGET=cooja

# Build prasle algorithm
make ALGORITHM=prasle TARGET=cooja

# Build adaptive-prasle algorithm
make ALGORITHM=adaptive-prasle TARGET=cooja

# Clean build
make clean
```

### Fast Mode (Quick Testing)

For faster convergence times during testing and simulation, use FAST_MODE:

```bash
# Build with fast timing mode (~1-2 second convergence instead of 5-10 seconds)
make ALGORITHM=bully TARGET=cooja FAST_MODE=1

# Normal mode (default, conservative timeouts for real wireless networks)
make ALGORITHM=bully TARGET=cooja
```

**Timing Comparison:**

| Parameter | Normal Mode | Fast Mode |
|-----------|-------------|-----------|
| Election Timeout | 5 seconds | 1 second |
| Coordinator Timeout | 10 seconds | 4 seconds |
| Alive Interval | 8 seconds | 2 seconds |
| Random Delay Max | 5 seconds | 1 second |
| **Expected Convergence** | 5-10 seconds | 1-2 seconds |

**When to use each mode:**
- **Fast Mode**: Quick testing, simulation, development iterations
- **Normal Mode**: Real hardware deployments, realistic wireless network conditions

### Understanding Timing Parameters

Each timing parameter in the Bully algorithm serves a specific purpose. Here's why these values were chosen:

#### 1. RANDOM_DELAY_MAX (Normal: 5s, Fast: 1s)

**Purpose**: Prevents "election storms" when all nodes start simultaneously.

Without this delay, if all nodes boot at the same time:
1. All nodes would broadcast ELECTION messages simultaneously
2. All nodes would respond with ANSWER messages simultaneously
3. Network congestion causes packet loss
4. Nodes timeout and restart elections → infinite loop

**How it works**: Each node waits a random time (0 to RANDOM_DELAY_MAX) before starting its first election. This staggers the elections so the highest-ID node can "win" before lower-ID nodes even start.

#### 2. ELECTION_TIMEOUT (Normal: 5s, Fast: 1s)

**Purpose**: Time to wait for ANSWER messages after sending an ELECTION.

When a node starts an election:
1. It broadcasts ELECTION to all higher-ID nodes
2. It waits ELECTION_TIMEOUT for any ANSWER responses
3. If no ANSWER received → it wins and becomes coordinator
4. If ANSWER received → it backs down and waits for COORDINATOR message

**Why 5 seconds in normal mode**: Wireless networks have:
- Packet loss requiring retransmissions
- Variable latency due to CSMA backoff
- Potential interference

5 seconds provides enough margin for messages to be delivered reliably.

#### 3. ALIVE_INTERVAL (Normal: 8s, Fast: 2s)

**Purpose**: How often the coordinator sends heartbeat messages.

Trade-offs:
- **Too frequent** (e.g., 1s): Wastes bandwidth, drains battery, increases congestion
- **Too infrequent** (e.g., 30s): Slow failure detection, long periods without confirmation

**Why 8 seconds**: Balances:
- Reasonable failure detection time (~10-18 seconds worst case)
- Low network overhead (~7.5 messages/minute)
- Battery efficiency for sensor networks

#### 4. COORDINATOR_TIMEOUT (Normal: 10s, Fast: 4s)

**Purpose**: How long to wait before declaring the coordinator dead.

**Relationship to ALIVE_INTERVAL**: Should be slightly longer than ALIVE_INTERVAL to account for:
- Network delays
- Occasional packet loss
- Clock drift between nodes

**Why 10 seconds (1.25× ALIVE_INTERVAL)**:
- If heartbeat arrives on time (8s), we have 2s margin
- If one heartbeat is lost, we detect failure after ~10s (miss one, timeout waiting for next)
- Tight enough for quick failover, loose enough to avoid false positives

#### Timing Diagram

```
Normal Mode Election Timeline:
═══════════════════════════════════════════════════════════════════════

Node Startup:
├─────────────────────────────────────────┤
0s                                      5s (RANDOM_DELAY_MAX)
   └── Random delay before first election

Election Phase:
├─────────────────────────────────────────┤
0s                                      5s (ELECTION_TIMEOUT)
   └── Send ELECTION, wait for ANSWER responses

Coordinator Heartbeat:
├────────────────────────────────────────────────────────────────┤
0s              8s              16s             24s
│               │               │               │
└── ALIVE ──────┴── ALIVE ──────┴── ALIVE ──────┴── ALIVE ...
    (ALIVE_INTERVAL = 8 seconds between heartbeats)

Failure Detection:
├──────────────────────────────────────────────────────────────────┤
0s        8s                    18s
│         │                     │
│   Last ALIVE received         │
│         │                     └── COORDINATOR_TIMEOUT expires (10s after last ALIVE)
│         │                         → Start new election
│         └── Expected ALIVE (missed due to coordinator crash)

Total Worst-Case Recovery Time:
├──────────────────────────────────────────────────────────────────┤
0s                              10s                           15s
│                               │                             │
Coordinator crashes             Timeout detected              New leader elected
                                │                             │
                                └── 5s RANDOM_DELAY + 5s ELECTION_TIMEOUT
                                    (But often faster due to staggered delays)
```

### Opening in Cooja GUI

To manually run and observe the simulation in Cooja's graphical interface:

**Available CSC files** (located in `experiments/convergence/csc_templates/{algorithm}/`):

| File | Description |
|------|-------------|
| `5nodes.csc` | Basic 5-node simulation |
| `10nodes.csc` | Basic 10-node simulation |
| `50nodes.csc` | Basic 50-node simulation |
| `100nodes.csc` | Basic 100-node simulation |
| `*-crash.csc` | Leader crash scenarios |
| `*-noise{50,70,90}.csc` | Packet loss scenarios |
| `*-partition.csc` | Network partition scenarios |

**Option 1: Open Cooja GUI first**
```bash
# Build the algorithm first
cd examples/leader-election
make ALGORITHM=bully TARGET=cooja

# Launch Cooja
cd ../../tools/cooja
./gradlew run

# In Cooja: File → Open Simulation → Browse to:
# examples/leader-election/experiments/convergence/csc_templates/bully/5nodes.csc
```

**Option 2: Direct quickstart**
```bash
# Build first
cd examples/leader-election
make ALGORITHM=bully TARGET=cooja

# Launch with CSC file directly
java -jar ../../tools/cooja/dist/cooja.jar \
    -quickstart=experiments/convergence/csc_templates/bully/5nodes.csc
```

---

## Running Experiments

### Master Experiment Runner

The `run_all_experiments.sh` script orchestrates all experiment types.

```bash
cd experiments/

# Show help
./run_all_experiments.sh --help

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

**Experiment Configurations:**

When running experiments, configurations are calculated as: `algorithms × node_counts × experiments`

Example for `./run_all_experiments.sh -a bully -e convergence,fault_tolerance,noise,network_partition`:

| # | Algorithm | Nodes | Experiment |
|---|-----------|-------|------------|
| 1 | bully | 5 | convergence |
| 2 | bully | 5 | fault_tolerance |
| 3 | bully | 5 | noise |
| 4 | bully | 5 | network_partition |
| 5 | bully | 10 | convergence |
| 6 | bully | 10 | fault_tolerance |
| 7 | bully | 10 | noise |
| 8 | bully | 10 | network_partition |
| 9 | bully | 50 | convergence |
| 10 | bully | 50 | fault_tolerance |
| 11 | bully | 50 | noise |
| 12 | bully | 50 | network_partition |
| 13 | bully | 100 | convergence |
| 14 | bully | 100 | fault_tolerance |
| 15 | bully | 100 | noise |
| 16 | bully | 100 | network_partition |

This gives **16 configurations** (1 algorithm × 4 node counts × 4 experiments).

**Note:** The noise experiment runs 3 sub-experiments per configuration (50%, 70%, 90% packet success rates), so actual trial counts are higher than reported.

### Complete Evaluation Script (Recommended)

The `run_complete_evaluation.sh` script is the recommended way to run a full evaluation. It:
- Supports all 10 algorithm variants (including PraSLE and Adaptive-PraSLE topology variants)
- **Automatically generates missing CSC templates** (no manual setup required)
- Generates comparison charts after experiments complete
- Provides detailed progress reporting

```bash
cd experiments/

# Show help
./run_complete_evaluation.sh --help

# Run full evaluation (all algorithms, all experiments) - takes many hours
./run_complete_evaluation.sh

# Quick test with 10 trials for 5 and 10 nodes only
./run_complete_evaluation.sh -t 10 -n 5,10

# Run only convergence experiments for specific algorithms
./run_complete_evaluation.sh -e convergence -a bully,ring,adaptive-prasle -t 50

# Generate charts from existing results (skip experiments)
./run_complete_evaluation.sh --skip-experiments

# Run experiments only, no chart generation
./run_complete_evaluation.sh --skip-charts
```

**Options:**

| Option | Description | Default |
|--------|-------------|---------|
| `--trials, -t` | Number of trials per configuration | 10 |
| `--parallel, -p` | Number of parallel jobs | auto-detect |
| `--experiments, -e` | Comma-separated experiments (or "all") | all |
| `--algorithms, -a` | Comma-separated algorithms (or "all") | all |
| `--nodes, -n` | Comma-separated node counts | 5,10,50,100 |
| `--skip-experiments` | Skip running experiments, only generate charts | - |
| `--skip-charts` | Skip chart generation, only run experiments | - |
| `--dry-run` | Preview commands without executing | - |

**Supported Algorithms (10 total):**

| Base Algorithm | Topology Variants |
|----------------|-------------------|
| `bully` | - |
| `ring` | - |
| `prasle` | `prasle` (clique), `prasle-line`, `prasle-ring`, `prasle-mesh` |
| `adaptive-prasle` | `adaptive-prasle` (clique), `adaptive-prasle-line`, `adaptive-prasle-ring`, `adaptive-prasle-mesh` |

**Auto-Generated CSC Templates:** The script automatically detects and generates missing CSC configuration files for any algorithm/topology/experiment combination. This means you can run experiments for any configuration without manual CSC file creation.

---

### Convergence Experiments

Measures the time for all nodes to agree on a leader.

```bash
cd experiments/convergence/

# Usage: ./run_convergence_trials.sh <algorithm> <node_count> [trials] [duration] [parallel_jobs] [--fixed-seed]

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

**Output:** `results/{algorithm}/convergence/{nodes}nodes_{timestamp}/`

---

### Fault Tolerance Experiments

Tests leader crash recovery - the current leader is crashed and re-election is measured.

```bash
cd experiments/fault_tolerance/

# Usage: ./run_crash_trials.sh <algorithm> <node_count> [trials] [crash_time] [duration] [parallel_jobs] [--fixed-seed]

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

---

### Noise Experiments

Tests algorithm resilience under network packet loss.

```bash
cd experiments/noise/

# Usage: ./run_noise_trials.sh <algorithm> <node_count> <noise_level> [trials] [duration] [parallel_jobs] [--fixed-seed]

# Run with 50% packet success rate
./run_noise_trials.sh bully 50 50 100

# Run with 70% packet success rate
./run_noise_trials.sh ring 10 70 50 90

# Run with 90% packet success rate (heavy noise)
./run_noise_trials.sh prasle 100 90 100 120 8
```

**Noise Levels:**

| Level | Success Ratio | Description |
|-------|---------------|-------------|
| 50 | 0.5 | 50% packet delivery |
| 70 | 0.3 | 30% packet delivery |
| 90 | 0.1 | 10% packet delivery (heavy noise) |

**Parameters:**

| Parameter | Description | Default |
|-----------|-------------|---------|
| algorithm | Algorithm to test (required) | - |
| node_count | Number of nodes: 5, 10, 50, or 100 (required) | - |
| noise_level | Noise level: 50, 70, or 90 (required) | - |
| trials | Number of trial runs | 100 |
| duration | Duration per trial (seconds) | 60 |
| parallel_jobs | Number of parallel jobs | auto-detect |

**Output:** `results/{algorithm}/noise/{nodes}nodes_noise{level}_{timestamp}/`

---

### Network Partition Experiments

Tests split-brain scenarios where the network is divided into two isolated partitions.

```bash
cd experiments/network_partition/

# Usage: ./run_partition_trials.sh <algorithm> <node_count> [trials] [duration] [parallel_jobs] [--fixed-seed]

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

---

## Analysis Scripts

### Quick Analysis (Recommended)

Use the `analyze.sh` script for easy analysis of experiment results:

```bash
cd examples/leader-election/experiments

# Interactive mode - shows menu to select algorithm and experiment
./analyze.sh

# Analyze all experiments for an algorithm
./analyze.sh bully              # Analyze all bully results
./analyze.sh ring               # Analyze all ring results

# Analyze specific experiment
./analyze.sh bully convergence        # Analyze bully convergence
./analyze.sh ring fault_tolerance     # Analyze ring fault tolerance
./analyze.sh prasle noise             # Analyze prasle noise experiments

# Analyze all algorithms
./analyze.sh all                      # Analyze everything
./analyze.sh all convergence          # Analyze convergence for all algorithms
```

The script automatically:
- Finds the latest results for each algorithm/experiment
- Generates statistical summaries (mean, std dev, median, min, max)
- Creates plots where applicable
- Saves analysis outputs alongside the original data

### Fault Tolerance Analysis

```bash
cd experiments/fault_tolerance/

# Analyze crash recovery results
python3 analyze_recovery.py -i ../../results/bully/fault_tolerance/50nodes_crash60s_*/crash_recovery_times.csv

# Specify algorithm explicitly
python3 analyze_recovery.py -i results.csv -a ring -o output_dir/

# Skip plot generation
python3 analyze_recovery.py -i results.csv --no-plots
```

### Noise Analysis

```bash
cd experiments/noise/

# Analyze noise experiment results
python3 analyze_noise.py -i ../../results/bully/noise/50nodes_noise50_*/noise_convergence_times.csv

# With algorithm specification
python3 analyze_noise.py -i results.csv -a prasle -o output_dir/
```

### Network Partition Analysis

```bash
cd experiments/network_partition/

# Analyze partition results
python3 analyze_partition.py -i ../../results/bully/network_partition/50nodes_partition_*/partition_times.csv

# With algorithm specification
python3 analyze_partition.py -i results.csv -a ring -o output_dir/
```

### Metrics Parser

```bash
cd scripts/

# Parse metrics from Cooja log
python3 parse_metrics.py -i cooja_output.log -o metrics.csv -a bully

# Verbose output
python3 parse_metrics.py -i cooja_output.log -v
```

---

## Plot Generation

### Generate All Plots

```bash
cd experiments/

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
cd experiments/convergence/
./create-convergence-plot.sh -a bully

# Fault tolerance plots
cd experiments/fault_tolerance/
./create-fault-tolerance-plot.sh -a ring

# Noise plots
cd experiments/noise/
./create-noise-plot.sh -a prasle

# Partition plots
cd experiments/network_partition/
./create-partition-plot.sh -a adaptive-prasle
```

### Convergence Distribution Plot

```bash
cd scripts/

# Generate convergence time distribution plot
python3 plot_convergence_distribution.py -i convergence_times.csv -o output.png -a bully

# Custom title
python3 plot_convergence_distribution.py -i data.csv --title "Custom Title" --dpi 150
```

---

## CSC Template Generation

Generate Cooja simulation configuration files.

### Automatic Generation (Recommended)

The `run_complete_evaluation.sh` script **automatically generates missing CSC templates** when running experiments. This is the recommended approach as it handles all configurations automatically:

```bash
cd experiments/

# CSC templates are auto-generated as needed - no manual setup required
./run_complete_evaluation.sh -a bully,prasle-line -e convergence -t 10
```

### Manual Generation

For manual CSC generation, use the `generate_csc.py` script:

```bash
cd scripts/

# Generate all CSC templates for all algorithms and experiments
python3 generate_csc.py --experiment all --output ../experiments/

# Generate convergence templates for bully
python3 generate_csc.py --algorithm bully --experiment convergence --output ../experiments/convergence/csc_templates/

# Generate for PraSLE with specific topology
python3 generate_csc.py --algorithm prasle --topology line --experiment convergence --output ../experiments/convergence/csc_templates/

# Generate fault tolerance templates
python3 generate_csc.py --experiment fault_tolerance --output ../experiments/fault_tolerance/csc_templates/

# Generate noise templates (all noise levels)
python3 generate_csc.py --experiment noise --output ../experiments/noise/csc_templates/

# Generate specific noise level
python3 generate_csc.py --experiment noise --noise-level 50 --output ../experiments/noise/csc_templates/

# Generate partition templates
python3 generate_csc.py --experiment network_partition --output ../experiments/network_partition/csc_templates/

# Generate for specific node count
python3 generate_csc.py --algorithm bully --nodes 50 --experiment convergence --output ./
```

**Options:**

| Option | Description | Default |
|--------|-------------|---------|
| `--algorithm, -a` | Algorithm (bully, ring, prasle, adaptive-prasle, all) | all |
| `--topology` | Network topology for PraSLE/Adaptive-PraSLE (clique, line, ring, mesh) | clique |
| `--nodes, -n` | Node count (5, 10, 50, 100) | all |
| `--experiment, -e` | Experiment type | convergence |
| `--noise-level` | Noise level for noise experiments (50, 70, 90) | all |
| `--output, -o` | Output directory (required) | - |

**Topology-Aware Output Directories:** When generating CSC templates with a non-clique topology, the output directory includes the topology name:
- `prasle` with `--topology line` → `prasle-line/`
- `adaptive-prasle` with `--topology ring` → `adaptive-prasle-ring/`

---

## Results Structure

After running experiments, results are organized as:

```
results/
├── bully/
│   ├── convergence/
│   │   └── 50nodes_20251229_120000/
│   │       ├── convergence_times.csv
│   │       ├── trial_1/
│   │       │   └── metrics.csv
│   │       └── ...
│   ├── fault_tolerance/
│   │   └── 50nodes_crash60s_20251229_130000/
│   │       ├── crash_recovery_times.csv
│   │       ├── recovery_summary.csv
│   │       └── recovery_*.png
│   ├── noise/
│   │   └── 50nodes_noise50_20251229_140000/
│   │       ├── noise_convergence_times.csv
│   │       ├── noise_summary.csv
│   │       └── noise_*.png
│   └── network_partition/
│       └── 50nodes_partition_20251229_150000/
│           ├── partition_times.csv
│           ├── partition_summary.csv
│           └── partition_*.png
├── ring/
│   └── ...
├── prasle/
│   └── ...
└── adaptive-prasle/
    └── ...
```

---

## Examples

### Complete Experiment Workflow

```bash
# 1. Build all algorithms
make ALGORITHM=bully TARGET=cooja
make ALGORITHM=ring TARGET=cooja
make ALGORITHM=prasle TARGET=cooja
make ALGORITHM=adaptive-prasle TARGET=cooja

# 2. Run convergence experiments for all algorithms
cd experiments/
./run_all_experiments.sh -a all -e convergence -t 100 -n 5,10,50

# 3. Run fault tolerance for bully only
./run_all_experiments.sh -a bully -e fault_tolerance -t 50

# 4. Generate all plots
./generate_all_plots.sh

# 5. Compare algorithms
cd comparison/
python3 compare_algorithms.py
```

### Quick Test (5 trials)

```bash
cd experiments/

# Quick test with 5 trials
./run_all_experiments.sh -a bully -e convergence -t 5 -n 5

# Check results
ls -la ../results/bully/convergence/
```

### Running Single Experiment Manually

```bash
cd scripts/

# Run a single Cooja simulation
python3 run_experiment.py \
    --simulation ../experiments/convergence/csc_templates/bully/10nodes.csc \
    --duration 60 \
    --output ../results/test_run \
    --algorithm bully
```

---

## Quick Metrics Collection

The `create-metrics.sh` script provides a simple wrapper for running experiments without typing long commands. It's ideal for quick tests and collecting metrics for a single algorithm.

```bash
# Usage
./create-metrics.sh <algorithm> [duration] [output_name]
```

**Arguments:**

| Argument | Description | Default |
|----------|-------------|---------|
| `algorithm` | Algorithm to test: bully, ring, prasle, adaptive-prasle (required) | - |
| `duration` | Duration in seconds | 60 |
| `output_name` | Output directory name | metrics |

**Examples:**

```bash
# Run 60-second experiment with default output
./create-metrics.sh bully

# Run 5-minute experiment
./create-metrics.sh ring 300

# Run 2-minute experiment with custom output name
./create-metrics.sh prasle 120 my_test
```

**Output Location:** `results/{algorithm}/{output_name}/`

The script will generate:
- `summary.txt` - Human-readable experiment summary
- `metrics.csv` - Detailed metrics data

**View Results:**

```bash
cat results/bully/metrics/summary.txt
cat results/bully/metrics/metrics.csv
```

---

## Environment Variables

| Variable | Description | Used By |
|----------|-------------|---------|
| `COOJA_TIMEOUT` | Simulation timeout in ms | CSC scripts |
| `CRASH_TIME` | Leader crash time in ms | Fault tolerance CSC |

---

## Requirements

- Contiki-NG with Cooja simulator
- Python 3.6+
- Python packages: `matplotlib`, `numpy`
- Bash shell
- GNU Make

---

## Troubleshooting

### CSC Templates Not Found

```bash
# Regenerate all CSC templates
cd scripts/
python3 generate_csc.py --experiment all --output ../experiments/
```

### Simulation Hangs

- Check that the algorithm builds correctly: `make ALGORITHM=bully TARGET=cooja`
- Verify Cooja is installed and accessible
- Check simulation timeout settings

### No Results Generated

- Ensure parallel jobs don't exceed system capacity
- Check disk space for results
- Verify write permissions in results directory

---

## Network Partitioning and Split-Brain Behavior

### What is Network Partitioning?

A **network partition** (or "split-brain") occurs when nodes cannot communicate with each other due to:
- Physical distance exceeding radio range
- RF interference or obstacles
- Link failures
- Intentional network segmentation

### Partition Behavior

When the network partitions, leader election algorithms exhibit **correct distributed algorithm behavior**:

1. **Each partition independently elects its own coordinator**
   - Partition 1: Nodes {1, 2, 4} → Highest-ID node becomes coordinator
   - Partition 2: Nodes {3, 5, 6} → Highest-ID node becomes coordinator
   - Each partition elects the highest-priority node available to it

2. **Multiple coordinators exist simultaneously**
   - This is **not a bug** - it's expected behavior for partitioned networks
   - Each coordinator only manages nodes within its partition
   - Nodes cannot detect coordinators they cannot communicate with

3. **Partition healing**
   - When partitions reconnect, nodes discover higher-priority coordinators
   - System converges to a single coordinator (the globally highest-priority node)
   - Different algorithms have different healing mechanisms

### Example Partition Scenario

**Initial Network** (all connected):
```
[1]---[2]---[3]
             |
[4]---[5]---[6]
```
- Single coordinator: Node 6 (highest priority)

**After Partition** (link 3-6 breaks):
```
Partition A:        Partition B:
[1]---[2]---[3]     [6]
                     |
[4]---[5]           (isolated)
```
- Partition A coordinator: Node 5
- Partition B coordinator: Node 6

**After Healing** (link 3-6 restored):
```
[1]---[2]---[3]
             |
[4]---[5]---[6]
```
- System converges back to Node 6 as single coordinator

### Why This is Correct Behavior

Leader election algorithms are designed to handle partitions this way:

- **Availability over consistency**: Each partition can continue operating independently
- **Eventual consistency**: When partitions heal, the system converges to a single leader
- **No false assumptions**: Nodes don't assume unreachable nodes are "dead" - they just elect from reachable nodes

---

## Testing Scenarios

### Scenario 1: Normal Operation (All Nodes Connected)

**Setup**: Start all nodes within radio range of each other

**Expected Behavior**:
1. Random startup delays stagger initial elections
2. Multiple elections may occur as nodes discover higher-priority nodes
3. Highest-ID node becomes coordinator (Normal mode: ~5-10s, Fast mode: ~1-2s)
4. Logs show heartbeat messages from coordinator periodically
5. All nodes receive and acknowledge heartbeat messages
6. NO further election messages after system stabilizes
7. System remains stable indefinitely

**Success Criteria**: Single coordinator (highest ID), periodic heartbeats, no election storms

> **Tip**: Use `FAST_MODE=1` during development for quicker iteration. See [Fast Mode](#fast-mode-quick-testing).

---

### Scenario 2: Leader Failure

**Setup**: Wait for stable coordinator sending heartbeat messages

**Steps**:
1. Stop/kill the coordinator using Cooja (right-click → Stop node)
2. Wait and observe logs

**Expected Behavior**:
1. Heartbeat messages from coordinator stop
2. After timeout period, nodes detect failure
3. Nodes log timeout and start new election
4. Next highest-ID node wins election and becomes new coordinator
5. New coordinator begins sending heartbeat messages
6. System stabilizes with new leader

**Success Criteria**: Clean failover within timeout period, no election storms

---

### Scenario 3: Network Partition (Split-Brain)

**Setup**: Position nodes in two separate groups out of radio range

**Expected Behavior**:
1. Each partition independently elects a coordinator
2. Each partition shows heartbeat activity from its coordinator
3. Nodes in one partition show NO activity from the other partition
4. Both partitions operate independently and stably

**Success Criteria**: Two independent coordinators, each partition stable

---

### Scenario 4: Partition Healing

**Setup**: Continue from partitioned network

**Steps**:
1. Move nodes back into radio range of each other
2. Observe convergence behavior

**Expected Behavior**:
1. Nodes discover higher-priority coordinator from other partition
2. System converges to single coordinator
3. All nodes acknowledge the same leader

**Success Criteria**: Fast convergence, all nodes acknowledge single coordinator

---

### Scenario 5: Multiple Cascading Failures

**Setup**: Start with all nodes, wait for highest-ID node to become coordinator

**Steps**:
1. Stop highest-ID node → Wait for next node to become coordinator
2. Stop that node → Wait for next node to become coordinator
3. Continue stopping nodes
4. Restart original highest-ID node → Wait for system to reconverge

**Expected Behavior**:
1. Each failure triggers election within timeout period
2. Next highest-priority node becomes coordinator
3. When original leader restarts, it initiates election
4. System recognizes it as highest priority and elects it
5. Final state: Original leader is coordinator

**Success Criteria**: Clean transitions at each failure, original leader reclaims leadership when restarted

---

## Educational Value

This framework demonstrates:

- **Distributed algorithms** in IoT networks
- **Leader election** mechanisms and trade-offs
- **Fault tolerance** in distributed systems
- **Message passing** using Contiki-NG
- **Network simulation** with Cooja
- **Statistical analysis** of algorithm performance
- **Scalability evaluation** across network sizes

---

## Advanced: Multi-Hop Routing

The current implementations use link-local multicast which only reaches directly reachable neighbors. To enable **true multi-hop routing** where all nodes can communicate regardless of physical distance:

### Option A: Configure RPL Root + Site-Local Multicast

1. **Designate one node as RPL DODAG root** (typically the highest-priority node):
   ```c
   #include "net/routing/rpl-lite/rpl.h"

   // In PROCESS_THREAD, after UDP initialization:
   if (my_node_id == HIGHEST_ID) {
     rpl_dag_root_init_dag_immediately();
     LOG_INFO("Configured as RPL DODAG root\n");
   }
   ```

2. **Change multicast address to site-local**:
   ```c
   // In send_message() and broadcast_message():
   uip_ipaddr_t dest_addr;
   uip_ip6addr(&dest_addr, 0xff05, 0, 0, 0, 0, 0, 0, 0x0001);  // ff05::1
   ```

3. **Add delay for RPL convergence**:
   ```c
   // Wait 30-60 seconds after startup before starting elections
   etimer_set(&rpl_wait_timer, 60 * CLOCK_SECOND);
   PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rpl_wait_timer));
   ```

### Option B: Application-Level Flooding

Implement hop-by-hop message flooding:

1. **Track recently seen messages** (sequence tracking already implemented)
2. **Re-broadcast once**: When receiving a message for the first time, re-broadcast it
3. **Stop propagation**: Don't re-broadcast messages already seen

This is simpler but generates more network traffic.

### Option C: Accept Partition-Tolerant Behavior (Current)

Keep the current implementation where:
- Partitions elect independent coordinators
- Suitable for networks where partitions are expected
- Lower complexity and overhead

---

## Timing Configuration Reference

This section documents the timing parameters for each algorithm, including paper references and the rationale for our implementation values.

### Algorithm Paper References

| Algorithm | Paper | DOI |
|-----------|-------|-----|
| **Bully** | Garcia-Molina, H. (1982). "Elections in a Distributed Computing System," IEEE Trans. on Computers, vol. C-31, no. 1, pp. 48-59 | [10.1109/TC.1982.1675885](https://doi.org/10.1109/TC.1982.1675885) |
| **Ring** | Chang, E. & Roberts, R. (1979). "An Improved Algorithm for Decentralized Extrema-Finding in Circular Configurations of Processes," Communications of the ACM, vol. 22, no. 5, pp. 281-283 | [10.1145/359104.359108](https://doi.org/10.1145/359104.359108) |
| **PraSLE** | Conard, M. & Ebnenasir, A. (2021). "A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices," 17th European Dependable Computing Conference (EDCC), pp. 127-134 | [10.1109/EDCC53658.2021.00025](https://doi.org/10.1109/EDCC53658.2021.00025) |

### Timing Parameter T in Original Papers

The fundamental timing parameter **T** (maximum message delay) is treated differently by each algorithm's original paper:

| Algorithm | T Definition in Paper | Paper Location |
|-----------|----------------------|----------------|
| **Bully** | "T: an upper bound on the time required to send a message from any process to any other." No concrete value specified. | Section III, p.50 |
| **Ring** | **T not defined.** Assumes synchronous, reliable communication. | N/A |
| **PraSLE** | "T := 1.0" (default maximum network latency in seconds). | Algorithm 1, Line 8 |

**Implementation Note**: Since the Bully and Ring papers do not specify concrete T values, we chose **T = 2 seconds** as a practical adaptation for 802.15.4 wireless networks, accounting for typical single-hop delays of 50-200ms in real IoT deployments.

### Timing Configuration Comparison

| Parameter | Bully (Normal) | Bully (Fast) | Ring (Normal) | Ring (Fast) | PraSLE (Normal) | PraSLE (Fast) |
|-----------|----------------|--------------|---------------|-------------|-----------------|---------------|
| **T (base delay)** | 2s | 1s | 2s | 1s | 1.0s | 0.1s |
| **ELECTION_TIMEOUT** | 4s (2T) | 1s | 5s (N×T) | 1s | K×T | K×T |
| **COORDINATOR_TIMEOUT** | 4s (2T) | 3s | 4s | 3s | N/A | N/A |
| **ALIVE_INTERVAL** | 2s (T) | 1s | 2s | 2s | N/A | N/A |
| **RANDOM_DELAY_MAX** | 2s | 1s | 2s | 1s | N/A | N/A |

**Notes:**
- **Bully timing model**: Based on Garcia-Molina's model where T is the upper bound for message delivery. Election timeout = 2T (wait for ANSWER) and coordinator timeout = 2T (tolerate one missed heartbeat).
- **Ring timing model**: Election message must traverse the entire ring (N hops). Election timeout = N×T for worst case.
- **PraSLE timing model**: T is the round duration. Total convergence time = K×T where K is the number of rounds (≥ network diameter).

### FAST_MODE vs NORMAL_MODE

- **NORMAL_MODE** (default): Conservative timeouts designed for real wireless network deployments with packet loss and interference.
- **FAST_MODE** (`FAST_MODE=1`): Reduced timeouts for quick testing and simulation. Enables faster iteration during development.

**When to use each mode:**
- Use **NORMAL_MODE** for realistic performance evaluation and real hardware deployments
- Use **FAST_MODE** for rapid testing, CI/CD pipelines, and development iterations

**Note on PraSLE FAST_MODE**: The T=0.1s value comes from the paper's Table I (IoT-Lab experiments), where authors used reduced timing for testbed evaluation.

---

## References

- **Bully Algorithm**: Garcia-Molina, H. (1982). "Elections in a Distributed Computing System," IEEE Transactions on Computers, vol. C-31, no. 1, pp. 48-59. DOI: [10.1109/TC.1982.1675885](https://doi.org/10.1109/TC.1982.1675885)
- **Ring Algorithm**: Chang, E. & Roberts, R. (1979). "An Improved Algorithm for Decentralized Extrema-Finding in Circular Configurations of Processes," Communications of the ACM, vol. 22, no. 5, pp. 281-283. DOI: [10.1145/359104.359108](https://doi.org/10.1145/359104.359108)
- **PraSLE Algorithm**: Conard, M. & Ebnenasir, A. (2021). "A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices," 17th European Dependable Computing Conference (EDCC), pp. 127-134. DOI: [10.1109/EDCC53658.2021.00025](https://doi.org/10.1109/EDCC53658.2021.00025)
- **Contiki-NG Documentation**: https://github.com/contiki-ng/contiki-ng
- **Cooja Simulator Guide**: Contiki-NG Wiki

---

## License

Part of the Contiki-NG project. See main repository for license details.
