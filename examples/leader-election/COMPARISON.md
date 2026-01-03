# Leader Election Algorithm Comparison

This document provides a comprehensive comparison of the leader election algorithms implemented in this framework: **Bully**, **Ring**, and **PraSLE** (with Adaptive-PraSLE variant).

## Algorithm Overview

| Aspect | Bully | Ring | PraSLE |
|--------|-------|------|--------|
| Type | Event-driven | Event-driven | Round-based |
| Communication | Broadcast | Point-to-point (logical ring) | Broadcast to neighbors |
| Network Stack | IPv6/UDP | IPv6/UDP | NullNet |
| Self-Stabilizing | No | No | Yes |
| Leader Selection | Highest ID wins | Highest ID wins | Lowest ranking value wins |

## Theoretical Complexity

| Metric | Bully | Ring | PraSLE |
|--------|-------|------|--------|
| Message Complexity | O(n²) worst case | O(n) per election | O(K × n × d) |
| Time Complexity | O(1) rounds | O(n) hops | O(K) rounds |
| Space per Node | O(1) | O(1) | O(d) neighbors |

Where:
- n = number of nodes
- K = number of rounds (PraSLE parameter)
- d = average node degree (number of neighbors)

## Parameter Configuration

### Timing Parameters Comparison

| Parameter | Bully | Ring | Comparable? |
|-----------|-------|------|-------------|
| ELECTION_TIMEOUT | 5s | 5s | ✓ Yes |
| COORDINATOR_TIMEOUT | 10s | 6s | ⚠ Different |
| ALIVE_INTERVAL | 8s | 4s | ⚠ Different |
| RANDOM_DELAY_MAX | 5s | 5s | ✓ Yes |

### PraSLE Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| K_ROUNDS | 10 | Number of convergence rounds |
| T_SECONDS | 1.0 | Round interval (seconds) |
| MAX_NEIGHBORS | 8 | Maximum tracked neighbors |

### Parameter Rationale

**Why Ring has faster heartbeats:**
- Ring messages traverse O(n) hops sequentially
- Faster heartbeats (4s) compensate for longer message propagation time
- Coordinator timeout (6s) is 1.5x heartbeat interval

**Why Bully has slower heartbeats:**
- Bully uses broadcast - all nodes receive messages immediately
- Slower heartbeats (8s) reduce network traffic
- Coordinator timeout (10s) is 1.25x heartbeat interval

## Failure Detection Time

| Algorithm | Detection Time | Formula |
|-----------|----------------|---------|
| Bully | 10s | COORDINATOR_TIMEOUT |
| Ring | 6s | COORDINATOR_TIMEOUT |
| PraSLE | N/A | Self-stabilizing (no explicit failure detection) |

**Note**: For fair comparison of failure detection, consider that:
- Bully detects failure in 10s but recovers faster (O(1) rounds)
- Ring detects failure in 6s but recovery is O(n) message hops

## Network Stack Differences

### Bully & Ring (IPv6/UDP)

```
Application
    ↓
Simple UDP (port 8765)
    ↓
IPv6 with RPL-Lite routing
    ↓
CSMA MAC
    ↓
Radio (UDGM in Cooja)
```

**Features:**
- Link-local multicast (ff02::1) for broadcast
- RPL provides multi-hop routing capability
- Reliable timer operation in UDP callbacks

### PraSLE (NullNet)

```
Application
    ↓
NullNet (minimal network layer)
    ↓
CSMA MAC
    ↓
Radio (UDGM in Cooja)
```

**Features:**
- No routing overhead
- Direct broadcast to neighbors
- Lighter weight than IPv6 stack

## Message Structure Comparison

### Bully Message (7 bytes)

```c
typedef struct {
  uint8_t type;        // ELECTION, ANSWER, COORDINATOR, ALIVE
  uint16_t node_id;    // Sender's ID (priority)
  uint16_t target_id;  // Target (0=broadcast)
  uint16_t sequence;   // Duplicate detection
} bully_msg_t;
```

### Ring Message (13 bytes)

```c
typedef struct {
  uint8_t type;           // ELECTION, COORDINATOR, ALIVE, ACK
  uint16_t initiator_id;  // Who started message chain
  uint16_t candidate_id;  // Best candidate so far
  uint16_t sequence;      // Election round number
  uint16_t target_node_id;// Next node in ring
  uint16_t sender_node_id;// For ACK routing
  uint16_t ack_sequence;  // ACK correlation
} ring_msg_t;
```

### PraSLE Message (6 bytes)

```c
typedef struct {
  uint16_t min_value;   // Ranking value
  uint16_t leader_id;   // Current leader belief
  uint16_t sender_id;   // Sender node ID
} prasle_msg_t;
```

## Fault Tolerance Mechanisms

### Bully

| Mechanism | Description |
|-----------|-------------|
| ALIVE heartbeats | Coordinator sends periodic heartbeats |
| Coordinator timeout | Nodes detect missing heartbeats |
| ANSWER suppression | Higher-priority nodes suppress lower elections |
| Coordinator re-announcement | Fast partition healing |

### Ring

| Mechanism | Description |
|-----------|-------------|
| ALIVE heartbeats | Coordinator sends heartbeat around ring |
| ACK-based failure detection | Detect failed nodes via missing ACKs |
| Dynamic ring reconfiguration | Skip unreachable nodes |
| Node recovery probing | Periodically check if nodes recovered |

### PraSLE

| Mechanism | Description |
|-----------|-------------|
| Self-stabilization | Converges from any initial state |
| Round-based | Continuous state exchange |
| No explicit failure detection | System naturally reconverges |

## Scalability Characteristics

### Small Networks (5-10 nodes)

| Algorithm | Convergence Time | Messages |
|-----------|------------------|----------|
| Bully | ~5-10s | O(n²) ≈ 25-100 |
| Ring | ~10-15s | O(n) ≈ 5-10 |
| PraSLE | K×T = 10s | O(K×n×d) ≈ 200-400 |

### Medium Networks (50 nodes)

| Algorithm | Convergence Time | Messages |
|-----------|------------------|----------|
| Bully | ~10-15s | O(n²) ≈ 2500 |
| Ring | ~30-45s | O(n) ≈ 50 |
| PraSLE | K×T = 10s | O(K×n×d) ≈ 2000-4000 |

### Large Networks (100 nodes)

| Algorithm | Convergence Time | Messages |
|-----------|------------------|----------|
| Bully | ~15-30s | O(n²) ≈ 10000 |
| Ring | ~60-90s | O(n) ≈ 100 |
| PraSLE | K×T = 10s | O(K×n×d) ≈ 4000-8000 |

**Notes:**
- Ring uses 2s backoff cap for 100-node scalability
- PraSLE convergence time is fixed regardless of network size (K rounds)
- Bully message count grows quadratically

## Radio Range Requirements

| Node Count | Bully | Ring | PraSLE |
|------------|-------|------|--------|
| 5 nodes | 150.0 | 150.0 | 150.0 |
| 10 nodes | 150.0 | 150.0 | 150.0 |
| 50 nodes | 350.0 | 350.0 | 350.0 |
| 100 nodes | 550.0* | 400.0** | 550.0 |

*Bully needs full connectivity for broadcast
**Ring uses 400.0 to enable partition testing (gap is 448 units)

## Experiment Types

All algorithms are tested with the same experiment framework:

| Experiment | Description | Success Criteria |
|------------|-------------|------------------|
| Convergence | Initial leader election | Single leader elected |
| Fault Tolerance | Leader crash recovery | New leader elected |
| Noise (50/70/90%) | Packet loss conditions | Leader maintained |
| Network Partition | Split-brain scenarios | Each partition has leader |

## Experiment Durations

| Node Count | Bully | Ring | PraSLE |
|------------|-------|------|--------|
| 5-10 nodes | 60s | 60s | 60s |
| 50 nodes | 60s | 120s | 60s |
| 100 nodes | 60s | 180s | 60s |

Ring needs longer durations due to O(n) message complexity.

## When to Use Each Algorithm

### Use Bully When:
- Fast convergence is critical
- Network is fully connected (all nodes in radio range)
- Message overhead is acceptable
- Simple implementation preferred
- No topology constraints

### Use Ring When:
- Low message overhead is important
- Network has ring-like topology naturally
- Willing to accept O(n) convergence time
- Need deterministic message count
- Node failures are rare

### Use PraSLE When:
- Self-stabilization is required
- Network may start in arbitrary state
- Continuous operation without coordinator needed
- Fixed convergence time regardless of network size
- Lower priority (ID) nodes should become leader

## Algorithm Recommendations by Scenario

| Scenario | Recommended | Reason |
|----------|-------------|--------|
| Small IoT cluster | Bully | Fast, simple |
| Large sensor network | PraSLE | Scalable, self-stabilizing |
| Sequential pipeline | Ring | Matches natural topology |
| Unreliable network | PraSLE | Self-healing |
| Real-time requirements | Bully | Fastest convergence |

## Metrics Collected

All algorithms track the same metrics for fair comparison:

| Metric | Description |
|--------|-------------|
| convergence_time | Time to elect initial leader |
| messages_sent | Total messages transmitted |
| messages_received | Total messages received |
| elections_started | Number of election initiations |
| elections_won | Number of times became leader |
| timeouts_detected | Coordinator failure detections |
| heartbeats_sent | ALIVE messages sent (Bully/Ring) |
| heartbeats_received | ALIVE messages received (Bully/Ring) |

## Running Comparative Experiments

### Run All Algorithms

```bash
./experiments/run_all_experiments.sh -a all -t 10 -n 5,10,50,100 \
  -e convergence,fault_tolerance,noise,network_partition
```

### Run Specific Algorithm

```bash
# Bully only
./experiments/run_all_experiments.sh -a bully -t 10 -n 5,10,50,100

# Ring only
./experiments/run_all_experiments.sh -a ring -t 10 -n 5,10,50,100

# PraSLE only
./experiments/run_all_experiments.sh -a prasle -t 10 -n 5,10,50,100
```

### Build Commands

```bash
# Build all
make all-algorithms

# Build specific algorithm
make ALGORITHM=bully TARGET=cooja
make ALGORITHM=ring TARGET=cooja
make ALGORITHM=prasle TARGET=cooja
make ALGORITHM=adaptive-prasle TARGET=cooja
```

## Tested Results Summary

### Convergence (10 trials each)

| Algorithm | 5 nodes | 10 nodes | 50 nodes | 100 nodes |
|-----------|---------|----------|----------|-----------|
| Bully | 10/10 | 10/10 | 10/10 | 10/10 |
| Ring | 10/10 | 10/10 | 10/10 | 10/10 |
| PraSLE | TBD | TBD | TBD | TBD |

### Fault Tolerance (10 trials each)

| Algorithm | 5 nodes | 10 nodes | 50 nodes | 100 nodes |
|-----------|---------|----------|----------|-----------|
| Bully | 10/10 | 10/10 | 10/10 | 10/10 |
| Ring | 10/10 | 10/10 | 10/10 | 9/10* |
| PraSLE | TBD | TBD | TBD | TBD |

*Some 100-node failures are detection script timing issues, not algorithm issues.

## Key Takeaways

1. **Bully** is best for small, fully-connected networks where fast convergence matters
2. **Ring** is best when message efficiency is critical and topology supports it
3. **PraSLE** is best for large, unreliable networks requiring self-stabilization
4. All algorithms use the same metrics framework for fair comparison
5. Radio range must be configured appropriately for each network size
6. Experiment durations should be scaled for Ring algorithm in large networks

## References

- Garcia-Molina, H. (1982). "Elections in a Distributed Computing System" [Bully]
- Chang, E. & Roberts, R. (1979). "An Improved Algorithm for Decentralized Extrema-Finding" [Ring]
- Conard & Ebnenasir (2021). "A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices" [PraSLE]
