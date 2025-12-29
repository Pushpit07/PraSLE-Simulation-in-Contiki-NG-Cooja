# PraSLE (Practical Self-Stabilizing Leader Election) Algorithm

PraSLE is a self-stabilizing leader election algorithm designed for resource-constrained IoT devices. It guarantees convergence from any arbitrary initial state.

## Algorithm Overview

Based on "A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices" (Conard & Ebnenasir, 2021).

### Key Concepts

1. **Round-based Algorithm**: Operates in discrete rounds of duration T
2. **(min, leader) Pairs**: Each node maintains a pair of values
3. **Lexicographic Comparison**: Nodes adopt the lexicographically smaller pair
4. **K Rounds**: After K rounds (where K >= network diameter), all nodes converge

### Algorithm Steps

1. **Initialization**:
   - Each node i starts with `(mini, leaderi) = (ranking_valuei, i)`
   - The ranking value can be node ID, battery level, etc.

2. **Each Round**:
   - Node broadcasts its `(mini, leaderi)` pair to neighbors
   - Node receives pairs from neighbors
   - Node adopts the lexicographically smallest pair

3. **Convergence**:
   - After K rounds, all nodes agree on the same leader
   - The leader is the node with the minimum ranking value

## Message Types

PraSLE uses a single message type containing:

| Field | Description |
|-------|-------------|
| `min_value` | Current minimum ranking value |
| `leader_id` | Current leader ID |
| `sender_id` | ID of sending node |

## Network Stack

This implementation uses **nullnet** for lightweight communication:

- **Network**: nullnet (minimal network layer)
- **MAC**: CSMA (Carrier Sense Multiple Access)
- **Topology**: Configurable (ring, line, mesh, clique)

## Algorithm Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `K_ROUNDS` | 10 | Number of rounds (should be >= network diameter) |
| `T_SECONDS` | 1.0 | Maximum network latency / round duration |
| `MAX_NEIGHBORS` | 8 | Maximum number of neighbors per node |
| `N_MAX` | 100 | Maximum number of nodes in network |
| `NETWORK_SIZE` | 10 | Actual number of nodes |

## Message Structure

```c
typedef struct {
  uint16_t min_value;    // Ranking value (mini)
  uint16_t leader_id;    // Leader ID (leaderi)
  uint16_t sender_id;    // ID of sending node
} prasle_msg_t;
```

## Neighbor Information

```c
typedef struct {
  uint16_t node_id;
  uint16_t min_value;
  uint16_t leader_id;
  bool valid;
} neighbor_info_t;
```

## Self-Stabilizing Properties

### Definition
A self-stabilizing algorithm eventually reaches a legitimate state from **any** arbitrary initial state, without external intervention.

### PraSLE Guarantees

1. **Convergence**: All nodes eventually agree on the same leader
2. **Closure**: Once converged, the system remains in a legitimate state
3. **Fault Tolerance**: Recovers automatically from transient faults

### Why Self-Stabilization Matters

- No need for careful initialization
- Automatic recovery from memory corruption
- Resilient to node restarts
- Handles dynamic network changes

## Supported Topologies

| Topology | Value | Description |
|----------|-------|-------------|
| RING | 1 | Ring structure with 2 neighbors each |
| LINE | 2 | Linear chain with 2 neighbors (except ends) |
| MESH | 3 | 2D grid with 4 neighbors each |
| CLIQUE | 4 | Fully connected (all nodes are neighbors) |

Configure in `prasle-config.h`:
```c
#define NETWORK_TOPOLOGY TOPOLOGY_CLIQUE
```

## Ranking Function

The ranking value determines election priority:

```c
static uint16_t get_ranking_value(void) {
  // Current implementation: use node ID
  return my_node_id;

  // Alternative implementations:
  // return battery_level();       // Prefer high-battery nodes
  // return 100 - compute_power(); // Prefer low-compute nodes
  // return rssi_quality();        // Prefer well-connected nodes
}
```

## Convergence Time

**Theoretical bound**: K * T seconds

Where:
- K = number of rounds (should be >= network diameter)
- T = round duration

**Example**: With K=10 and T=1.0s, convergence takes at most 10 seconds.

## Configuration

Set parameters via project-conf.h:

```c
#define PRASLE_K_ROUNDS 10
#define PRASLE_T_SECONDS 1.0
#define PRASLE_NETWORK_SIZE 50
```

Or directly in prasle-config.h.

## Algorithm Comparison

| Property | PraSLE | Bully | Ring |
|----------|--------|-------|------|
| Self-stabilizing | Yes | No | No |
| Message complexity | O(K * n) | O(n^2) | O(n) |
| Requires topology knowledge | Neighbors | IDs of higher nodes | Ring structure |
| Handles arbitrary faults | Yes | Limited | No |

## Testing Scenarios

### 1. Normal Convergence
1. Start all nodes
2. Observe round-by-round value propagation
3. Verify all nodes converge to same leader

### 2. Fault Injection
1. Wait for convergence
2. Corrupt a node's `(mini, leaderi)` values
3. Observe automatic recovery within K rounds

### 3. Dynamic Membership
1. Start with subset of nodes
2. Add new nodes during operation
3. Verify system re-stabilizes to correct leader

### 4. Leader Failure
1. Wait for convergence
2. Stop the current leader
3. Observe re-convergence to new leader

## References

- Conard, A. & Ebnenasir, A. (2021). "A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices"
- Dolev, S. (2000). "Self-Stabilization"
