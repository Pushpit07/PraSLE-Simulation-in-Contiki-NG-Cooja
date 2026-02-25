# Case 1: Original Paper Parameters

This directory contains experiment results using **original paper parameters** for each algorithm.

## Parameter Configuration

Each algorithm uses the timing parameters specified in its original publication:

### Bully Algorithm
- **ALIVE_INTERVAL**: 2.0s (heartbeat period)
- **COORDINATOR_TIMEOUT**: 4.0s (failure detection)
- **ELECTION_TIMEOUT**: 4.0s (election response timeout)
- **RANDOM_DELAY_MAX**: 2.0s (startup jitter)

Reference: Garcia-Molina (1982), "Elections in a Distributed Computing System"

### Ring Algorithm
- **ALIVE_INTERVAL**: 2.0s (heartbeat period)
- **COORDINATOR_TIMEOUT**: 4.0s (failure detection)
- **ELECTION_TIMEOUT**: 5.0s (ring traversal timeout)
- **RANDOM_DELAY_MAX**: 2.0s (startup jitter)

Reference: Chang & Roberts (1979), "An Improved Algorithm for Decentralized Extrema-Finding"

### PraSLE Algorithm
- **T**: 1.0s (round period)
- **K**: topology-dependent (number of rounds)
- **STARTUP_DELAY_MAX**: 1.0s (startup jitter)

Reference: Conard & Ebnenasir (2021), "A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices", EDCC 2021

### Adaptive-PraSLE Algorithm
- **T**: 1.0s (round period)
- **K**: topology-dependent (number of rounds)
- **STARTUP_DELAY_MAX**: 1.0s (startup jitter)

Note: Adaptive-PraSLE extends PraSLE with dynamic parameter adaptation.

## Use Case

Case 1 results are useful for:
- Validating algorithm implementations against published benchmarks
- Comparing performance under each algorithm's "optimal" configuration
- Understanding algorithm behavior with tuned parameters

## Comparison with Case 2

In Case 1, PraSLE/Adaptive-PraSLE broadcast twice as frequently (T=1s) as Bully/Ring (ALIVE_INTERVAL=2s), which may give them an unfair advantage in convergence time comparisons but higher message overhead.

For fair cross-algorithm comparison with identical broadcast rates, see **Case 2** results.

## Directory Structure

```
case1/
├── bully/
│   ├── convergence/
│   ├── fault_tolerance/
│   ├── noise/
│   └── network_partition/
├── ring/
├── prasle/
├── prasle-line/
├── prasle-ring/
├── prasle-mesh/
├── adaptive-prasle/
├── adaptive-prasle-line/
├── adaptive-prasle-ring/
├── adaptive-prasle-mesh/
└── comparison_charts/
```

## Running Case 1 Experiments

```bash
# Run complete evaluation with original parameters
PARAM_CASE=1 ./experiments/run_complete_evaluation.sh

# Or use the --param-case flag
./experiments/run_complete_evaluation.sh --param-case 1
```
