# Master's Thesis: Adaptive and Fault-Tolerant Leader Election in Resource-Constrained IoT and Robotic Clusters

This repository contains the simulation framework for the master's thesis on adaptive and fault-tolerant leader election in resource-constrained IoT and robotic clusters. It implements and evaluates multiple leader election algorithms on top of [Contiki-NG](https://github.com/contiki-ng/contiki-ng) using the [Cooja network simulator](https://docs.contiki-ng.org/en/develop/doc/tutorials/Cooja-simulations.html).

**Full source code:** [https://github.com/Pushpit07/PraSLE-Simulation-in-Contiki-NG-Cooja](https://github.com/Pushpit07/PraSLE-Simulation-in-Contiki-NG-Cooja)

## Algorithms

The framework implements and compares four leader election algorithms for resource-constrained IoT networks:

| Algorithm | Description |
|-----------|-------------|
| **Bully** | Classic highest-ID-wins approach with O(n^2) message complexity |
| **Ring** | Chang-Roberts ring-based election with O(n) message complexity |
| **PraSLE** | Practical Self-Stabilizing Leader Election (round-based, self-stabilizing) |
| **Adaptive-PraSLE** | Enhanced PraSLE with RTT-based timeout adaptation for varying network conditions |

PraSLE and Adaptive-PraSLE additionally support multiple **network topologies**: clique (fully connected), ring, line, and mesh.

## Directory Structure

```
examples/leader-election/
  algorithms/         # Algorithm implementations (bully, ring, prasle, adaptive-prasle)
  common/             # Shared metrics collection code
  experiments/        # Experiment runners and CSC simulation templates
  scripts/            # Python automation and analysis tools
  runners/            # Shell wrapper scripts for each algorithm
  results/            # Output data and comparison charts
  build/              # Compiled binaries
```

Detailed documentation is available inside the framework:
- [examples/leader-election/README.md](examples/leader-election/README.md) -- full framework guide
- [examples/leader-election/EXPERIMENTS.md](examples/leader-election/EXPERIMENTS.md) -- detailed experiment guide
- [examples/leader-election/COMPARISON.md](examples/leader-election/COMPARISON.md) -- algorithm comparison and parameter details

## Prerequisites

- **Contiki-NG** development environment (this repository)
- **Cooja simulator** (included with Contiki-NG)
- **Java** (required by Cooja)
- **Python 3** with `matplotlib` and `numpy` (for analysis scripts)
- **Bash** shell

Refer to the [Contiki-NG getting started guide](https://docs.contiki-ng.org/en/develop/doc/getting-started/index.html) for setting up the development environment and Cooja.

## Running Leader Election Experiments

### 1. Build an Algorithm

```bash
cd examples/leader-election

# Interactive mode -- presents a menu to select the algorithm
./build.sh

# Or specify the algorithm directly
./build.sh bully
./build.sh ring
./build.sh prasle
./build.sh adaptive-prasle
./build.sh all            # Build all algorithms
```

**Build options** (passed via environment or Makefile variables):

| Variable | Default | Description |
|----------|---------|-------------|
| `FAST_MODE` | `1` | Reduced timeouts for faster simulation |
| `PARAM_CASE` | `1` | Parameter set: `1` = original paper, `2` = standardized |
| `TOPOLOGY` | *(clique)* | PraSLE topology: `ring`, `line`, `mesh` |
| `NETWORK_SIZE` | - | Network size for K-value calculation |

### 2. Run a Quick Experiment

```bash
# Collect metrics for a single algorithm (60-second default)
./create-metrics.sh bully

# Specify duration and output name
./create-metrics.sh ring 120 my_test
```

### 3. Run the Full Evaluation Suite

The master script automates everything -- building, running experiments across all algorithms and node counts, and generating charts:

```bash
cd examples/leader-election

# Run the complete evaluation (all algorithms, all experiments, all node counts)
./experiments/run_complete_evaluation.sh

# Customize the run
./experiments/run_complete_evaluation.sh \
  --trials 10 \
  --algorithms "bully ring prasle adaptive-prasle" \
  --nodes "5 10 50 100" \
  --experiments "convergence fault_tolerance noise network_partition" \
  --parallel
```

### 4. Run Individual Experiment Types

```bash
# Convergence (cold-start election time)
./experiments/convergence/run_convergence_trials.sh

# Fault tolerance (leader crash and recovery)
./experiments/fault_tolerance/run_crash_trials.sh

# Network noise (packet loss resilience at 50%, 70%, 90% success rates)
./experiments/noise/run_noise_trials.sh

# Network partition (split-brain scenarios)
./experiments/network_partition/run_partition_trials.sh
```

### 5. Analyze Results and Generate Charts

```bash
# Quick analysis
./experiments/analyze.sh bully

# Generate comparison charts (Python)
python3 scripts/plot_node_comparison_charts.py
python3 scripts/plot_recovery_comparison_charts.py
python3 scripts/plot_noise_comparison_charts.py
python3 scripts/plot_partition_comparison_charts.py
python3 scripts/compare_algorithms.py
```

### Results Structure

Results are organized under `examples/leader-election/results/`:

```
results/case1/                          # Parameter set 1 (original paper)
  bully/convergence/5nodes_<timestamp>/
    convergence_times.csv               # Summary metrics
    trial_1/
      cooja_output.log                  # Raw simulation log
      metrics.csv                       # Parsed metrics
    trial_2/
    ...
```

## Experiment Types

| Experiment | What It Measures |
|------------|-----------------|
| **Convergence** | Cold-start election time under ideal conditions |
| **Fault Tolerance** | Recovery time after leader crash |
| **Noise** | Convergence under packet loss (50%, 70%, 90% success rates) |
| **Network Partition** | Split-brain detection and recovery |

Experiments are run across node counts of 5, 10, 50, and 100, with multiple trials per configuration for statistical significance.

## About Contiki-NG

This project is built on [Contiki-NG](https://github.com/contiki-ng/contiki-ng), an open-source OS for next-generation IoT devices, which provides IPv6/6LoWPAN, 6TiSCH, RPL, and CoAP networking stacks.

- Contiki-NG documentation: https://docs.contiki-ng.org/
- Contiki-NG releases: https://github.com/contiki-ng/contiki-ng/releases

## License

Unless explicitly stated otherwise, Contiki-NG sources are distributed under the terms of the [3-clause BSD license](LICENSE.md).
