# Adaptive PraSLE (Customized Self-Stabilizing Leader Election)

Adaptive PraSLE is a customizable variant of the PraSLE self-stabilizing leader election algorithm. It provides a framework for implementing custom ranking functions and adaptive behaviors.

## Overview

This implementation serves as a template for extending the base PraSLE algorithm with:
- Custom ranking functions
- Adaptive timeout mechanisms
- Additional message types
- Modified convergence criteria

**Current State**: The algorithm is currently identical to standard PraSLE. Modify it to implement your customizations.

## Customization Points

### 1. Ranking Function

The ranking function determines which node becomes leader. Modify `get_ranking_value()`:

```c
static uint16_t get_ranking_value(void) {
  // Default: use node ID (lowest ID wins)
  return my_node_id;

  // Customization ideas:
  // return battery_sensor_value();        // Prefer high-battery nodes
  // return MAX_BATTERY - battery_level(); // Prefer low-battery nodes
  // return compute_power_metric();        // Prefer high-compute nodes
  // return rssi_average();                // Prefer well-connected nodes
  // return energy_remaining * connectivity_score; // Combined metric
}
```

### 2. Convergence Criteria

Modify when the algorithm considers itself converged:

```c
static void check_convergence(void) {
  // Default: converged when mini/leaderi stable

  // Customization ideas:
  // - Require stability for N consecutive rounds
  // - Check neighbor agreement before declaring convergence
  // - Add timeout-based convergence
}
```

### 3. Neighbor Selection

Customize which nodes are considered neighbors:

```c
static void init_neighbors(void) {
  // Default: based on NETWORK_TOPOLOGY

  // Customization ideas:
  // - Dynamic neighbor discovery
  // - RSSI-based neighbor selection
  // - Periodic neighbor refresh
}
```

### 4. Additional Message Types

Add new message types for extended functionality:

```c
// Example: Add leader health status
typedef struct {
  uint16_t min_value;
  uint16_t leader_id;
  uint16_t sender_id;
  uint16_t battery_level;   // New field
  int16_t rssi;             // New field
} adaptive_prasle_msg_t;
```

## Algorithm Parameters

Same as standard PraSLE:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `K_ROUNDS` | 10 | Number of rounds |
| `T_SECONDS` | 1.0 | Round duration |
| `MAX_NEIGHBORS` | 8 | Maximum neighbors per node |
| `N_MAX` | 100 | Maximum network size |
| `NETWORK_SIZE` | 10 | Actual number of nodes |

## Implementation Ideas

### Energy-Aware Leader Election

```c
static uint16_t get_ranking_value(void) {
  // Nodes with more energy have lower ranking (preferred)
  uint16_t battery = read_battery_sensor();
  return MAX_BATTERY - battery;
}
```

Benefits:
- Balances energy consumption across network
- Leaders change as battery depletes
- Extends network lifetime

### Connectivity-Aware Leader Election

```c
static uint16_t get_ranking_value(void) {
  // Nodes with better connectivity have lower ranking
  int avg_rssi = compute_average_rssi_to_neighbors();
  return MAX_RSSI - avg_rssi;
}
```

Benefits:
- Leader is well-connected hub
- Reduces message delays
- Improves reliability

### Adaptive Timeout

```c
// Adjust T_SECONDS based on network conditions
static float get_adaptive_t_value(void) {
  float base_t = T_SECONDS;

  // Increase timeout during high network load
  if (message_loss_rate > 0.3) {
    return base_t * 2.0;
  }

  // Decrease timeout in stable conditions
  if (consecutive_successful_rounds > 10) {
    return base_t * 0.5;
  }

  return base_t;
}
```

### Hybrid Ranking

```c
static uint16_t get_ranking_value(void) {
  // Combine multiple factors
  uint16_t battery_score = battery_level() / 10;     // 0-100 -> 0-10
  uint16_t connectivity_score = rssi_quality() / 10; // 0-100 -> 0-10
  uint16_t compute_score = compute_power() / 10;     // 0-100 -> 0-10

  // Weighted combination (lower is better)
  return (battery_score * 3) + (connectivity_score * 2) + compute_score;
}
```

## Network Stack

Same as standard PraSLE:
- **Network**: nullnet
- **MAC**: CSMA
- **Topology**: Configurable

## Message Structure

```c
typedef struct {
  uint16_t min_value;    // Ranking value (mini)
  uint16_t leader_id;    // Leader ID (leaderi)
  uint16_t sender_id;    // ID of sending node
} prasle_msg_t;
```

Extend as needed for custom functionality.

## Configuration

Set parameters via project-conf.h:

```c
#define PRASLE_K_ROUNDS 10
#define PRASLE_T_SECONDS 1.0
#define PRASLE_NETWORK_SIZE 50
```

## Comparison with Standard PraSLE

| Aspect | Standard PraSLE | Adaptive PraSLE |
|--------|-----------------|-----------------|
| Ranking | Node ID | Customizable |
| Timeouts | Fixed | Can be adaptive |
| Messages | (min, leader) | Extensible |
| Convergence | K rounds | Customizable |

## Testing Recommendations

When implementing customizations:

1. **Baseline Comparison**: Compare with standard PraSLE first
2. **Convergence Verification**: Ensure algorithm still converges
3. **Fault Injection**: Test self-stabilization is preserved
4. **Performance Metrics**: Measure convergence time, message overhead
5. **Edge Cases**: Test with various network sizes and topologies

## References

- Conard, A. & Ebnenasir, A. (2021). "A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices"
- Base PraSLE implementation: See [prasle/README.md](../prasle/README.md)
