# Case 2: Standardized Parameters

This directory contains experiment results using **standardized parameters** across all algorithms.

## Parameter Configuration

All algorithms use identical timing parameters to enable fair cross-algorithm comparison:

### All Algorithms (Standardized)
- **Heartbeat/Round Period (T)**: 2.0s
- **Coordinator Timeout**: 4.0s (Bully/Ring only)
- **Election Timeout**: 4.0s (Bully), 5.0s (Ring)
- **Startup Delay Max**: 2.0s

### Algorithm-Specific Notes

| Parameter | Bully | Ring | PraSLE | Adaptive-PraSLE |
|-----------|-------|------|--------|-----------------|
| Heartbeat/T | 2.0s | 2.0s | 2.0s | 2.0s |
| Failure Detection | 4.0s | 4.0s | N/A | N/A |
| Election Timeout | 4.0s | 5.0s | K rounds | K rounds |
| Startup Delay | 2.0s | 2.0s | 2.0s | 2.0s |

Note: K_ROUNDS (PraSLE) remains topology-dependent as it cannot be standardized.

## Rationale

By setting T=2.0s for PraSLE/Adaptive-PraSLE (matching the Bully/Ring heartbeat interval), we ensure:
1. **Equal message rate**: All algorithms broadcast at the same frequency
2. **Fair overhead comparison**: Message counts reflect algorithm efficiency, not tuning
3. **Comparable convergence**: Time differences reflect algorithmic design, not parameter choices

## Use Case

Case 2 results are useful for:
- Fair comparison of algorithm efficiency (message overhead per convergence)
- Evaluating which algorithm design is fundamentally more efficient
- Academic/thesis comparisons where parameter tuning shouldn't influence results

## Comparison with Case 1

| Metric | Case 1 | Case 2 |
|--------|--------|--------|
| PraSLE T value | 1.0s (original) | 2.0s (standardized) |
| PraSLE convergence | Faster | Slower |
| PraSLE messages/sec | Higher | Lower (matches Bully/Ring) |
| Fair message comparison | No | Yes |

## Expected Behavior Differences

In Case 2 compared to Case 1:
- **PraSLE/Adaptive-PraSLE**: Slower convergence (2x T value) but identical message rate to Bully/Ring
- **Bully/Ring**: Identical behavior (already at standardized values)

## Directory Structure

```
case2/
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

## Running Case 2 Experiments

```bash
# Run complete evaluation with standardized parameters
PARAM_CASE=2 ./experiments/run_complete_evaluation.sh

# Or use the --param-case flag
./experiments/run_complete_evaluation.sh --param-case 2
```

## Build Verification

To verify standardized parameters are active:

```bash
# Build with PARAM_CASE=2 and check compiler output
make clean && make ALGORITHM=prasle TARGET=cooja PARAM_CASE=2

# The STANDARDIZED_PARAMS flag should be defined, setting T=2.0
```
