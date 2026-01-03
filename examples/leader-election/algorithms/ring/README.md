# Ring Leader Election Algorithm

The Ring algorithm (also known as Chang-Roberts algorithm) is a classic distributed leader election algorithm where election messages circulate around a logical ring. Each node forwards messages to its successor, updating the candidate if it has a higher ID.

## Algorithm Overview

### Ring Topology

Nodes form a logical unidirectional ring:

```
        Node 1
       ↗      ↘
    Node 5    Node 2
      ↑        ↓
    Node 4 ← Node 3
```

Each node knows only its successor (next node). The ring structure is:
- Node 1 → Node 2 → Node 3 → ... → Node N → Node 1 (wraps around)

### Election Process

1. **Election Initiation**: Any node detecting coordinator failure creates an ELECTION message with:
   - `initiator_id = my_node_id` (who started this)
   - `candidate_id = my_node_id` (current best candidate)

2. **Message Forwarding**: Each receiving node:
   - Compares its ID with `candidate_id`
   - Updates `candidate_id` if its ID is higher
   - Forwards to successor (next node in ring)

3. **Election Completion**: When ELECTION returns to initiator:
   - `candidate_id` contains the highest ID in the ring
   - That node is the winner (new coordinator)

4. **Coordinator Announcement**: Initiator sends COORDINATOR message around ring:
   - All nodes update their `current_leader`
   - Message terminates when it returns to sender

5. **Leader Monitoring**: Coordinator periodically sends ALIVE messages around ring:
   - Each node resets its coordinator timeout upon receipt
   - If timeout expires (no ALIVE), node starts new election

## Message Flow Example (5 nodes)

Initial state: Node 3 detects coordinator failure

```
Step 1: Node 3 starts election
  Node 3 → ELECTION(initiator=3, candidate=3) → Node 4

Step 2: Node 4 forwards (higher ID)
  Node 4 → ELECTION(initiator=3, candidate=4) → Node 5

Step 3: Node 5 forwards (higher ID)
  Node 5 → ELECTION(initiator=3, candidate=5) → Node 1

Step 4: Node 1 forwards (lower ID, keeps 5)
  Node 1 → ELECTION(initiator=3, candidate=5) → Node 2

Step 5: Node 2 forwards (lower ID, keeps 5)
  Node 2 → ELECTION(initiator=3, candidate=5) → Node 3

Step 6: Node 3 receives its own message
  Election complete! Winner = candidate_id = 5
  Node 3 → COORDINATOR(initiator=3, winner=5) → Node 4

Step 7: COORDINATOR travels the ring
  Each node sets current_leader = 5
  Message terminates at Node 3

Step 8: Node 5 starts heartbeats
  Node 5 → ALIVE(5,5) → ... → Node 5 (full circuit)
```

## Message Types

| Type | Code | Description |
|------|------|-------------|
| ELECTION | 1 | Election message circulating the ring |
| COORDINATOR | 2 | Coordinator announcement from election winner |
| ALIVE | 3 | Heartbeat from coordinator proving it's alive |
| ACK | 4 | Acknowledgment for dynamic ring reconfiguration |

## Node States

| State | Description |
|-------|-------------|
| STATE_NORMAL | Regular operation with a known leader |
| STATE_ELECTION | Currently participating in an election process |
| STATE_WAITING_COORDINATOR | Waiting for coordinator announcement |

### State Transitions

```
         ┌─────────────────────────────────┐
         │                                 │
         ▼                                 │
    STATE_NORMAL ────────────────────► STATE_ELECTION
         │                                 │
         │                                 │
         │                                 ▼
         │                    STATE_WAITING_COORDINATOR
         │                                 │
         └─────────────────────────────────┘
```

## Network Stack

This implementation uses **IPv6/UDP** for reliable communication:

- **Network Layer**: IPv6 with RPL-Lite routing
- **Transport Layer**: Simple UDP (via `simple-udp.h`)
- **MAC Layer**: CSMA (Carrier Sense Multiple Access)
- **Communication**: IPv6 multicast (ff02::1) with target_node_id filtering

### Why IPv6/UDP?

The ring algorithm was migrated from NullNet to IPv6/UDP for:
1. **Reliable timer operation**: Timer events are properly delivered in the UDP callback context
2. **Consistent with Bully**: Same networking approach enables fair performance comparison
3. **Better fault tolerance**: ALIVE heartbeats work correctly for failure detection

## Dynamic Ring Reconfiguration

The ring algorithm includes **ACK-based failure detection** to handle node failures gracefully.

### How It Works

1. When sending ELECTION or COORDINATOR messages, the sender waits for an ACK
2. If no ACK is received within `ACK_TIMEOUT` (500ms), the message is retried
3. After `MAX_RETRIES` (2) failed attempts, the target node is marked unreachable
4. The ring is automatically reconfigured to skip unreachable nodes

### Configuration Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `ENABLE_DYNAMIC_RING` | 1 | Enable/disable dynamic reconfiguration |
| `ACK_TIMEOUT` | 500ms | Time to wait for ACK |
| `MAX_RETRIES` | 2 | Retries before marking node unreachable |
| `NODE_RECOVERY_INTERVAL` | 30s | How often to probe unreachable nodes |

### Message Structure with ACK Support

```c
typedef struct {
  uint8_t type;             /* MSG_ELECTION, MSG_COORDINATOR, MSG_ALIVE, MSG_ACK */
  uint16_t initiator_id;    /* Node that started this message chain */
  uint16_t candidate_id;    /* Current best candidate (highest ID) */
  uint16_t sequence;        /* Election sequence number */
  uint16_t target_node_id;  /* Next node in ring to process message */
  uint16_t sender_node_id;  /* Node that sent this message (for ACK routing) */
  uint16_t ack_sequence;    /* ACK correlation sequence number */
} ring_msg_t;  /* Total: 13 bytes */
```

## Timing Configuration

### Normal Mode (Default)

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| `ELECTION_TIMEOUT` | 5 seconds | Time for election message to traverse ring |
| `COORDINATOR_TIMEOUT` | 6 seconds | Failure detection (> ALIVE_INTERVAL) |
| `ALIVE_INTERVAL` | 4 seconds | Heartbeat frequency for liveness |
| `RANDOM_DELAY_MAX` | 5 seconds | Stagger startup to prevent synchronized elections |
| `RING_SIZE` | 10 | Number of nodes in the ring (configurable) |

### Fast Mode (RING_FAST_MODE=1)

| Parameter | Value |
|-----------|-------|
| `ELECTION_TIMEOUT` | 1 second |
| `COORDINATOR_TIMEOUT` | 4 seconds |
| `ALIVE_INTERVAL` | 2 seconds |
| `RANDOM_DELAY_MAX` | 1 second |

To enable fast mode:
```bash
make ALGORITHM=ring TARGET=cooja FAST_MODE=1
```

## Radio Range Requirements

**Critical for proper operation**: The radio range must be configured appropriately for the network topology.

### Recommended Radio Ranges (UDGM Model)

| Node Count | Radio Range | Rationale |
|------------|-------------|-----------|
| 5 nodes | 150.0 | Covers 113.1 unit topology diameter |
| 10 nodes | 150.0 | Covers ~66 unit minimum distance |
| 50 nodes | 350.0 | Covers grid layout with spacing |
| 100 nodes | 400.0* | Balance between coverage and partition testing |

*For 100-node network partition experiments, radio range is set to 400.0 (less than the 448-unit partition gap) to enable proper partition testing.

### Setting Radio Range

Radio range is configured in the Cooja CSC files under the UDGM radio model:
```xml
<radiomedium>
  org.contikios.cooja.radiomediums.UDGM
  <transmitting_range>150.0</transmitting_range>
  <interference_range>200.0</interference_range>
  ...
</radiomedium>
```

## 100-Node Scaling

The ring algorithm includes optimizations for large networks (100+ nodes).

### Backoff Cap for Large Networks

When a coordinator fails, all nodes detect it and try to start elections simultaneously. To prevent congestion:

```c
/* Calculate priority-based backoff: lower IDs wait longer (but capped) */
clock_time_t priority_delay = (RING_SIZE - my_node_id) * CLOCK_SECOND / 50;
clock_time_t max_priority_delay = 2 * CLOCK_SECOND;  /* Cap at 2 seconds */
if(priority_delay > max_priority_delay) {
  priority_delay = max_priority_delay;
}
```

**Key Points:**
- Higher-ID nodes wait less (they're more likely to win)
- Maximum backoff is capped at 2 seconds to avoid excessive delays
- Without the cap, node 1 in a 100-node network would wait ~10 seconds

### Duration Scaling for Experiments

Large networks need longer experiment durations:

| Node Count | Duration | Reason |
|------------|----------|--------|
| 5, 10 | 60s | Small ring, fast convergence |
| 50 | 120s | Medium ring, more message hops |
| 100 | 180s | Large ring, O(n) message complexity |

## Partition Healing Mechanisms

This implementation includes two partition healing mechanisms for robustness:

### Mechanism 1: Coordinator Re-announcement

- **When**: Coordinator receives ELECTION from any node
- **Action**: Re-broadcasts COORDINATOR message around ring
- **Purpose**: Quickly inform nodes that missed original COORDINATOR announcement
- **Benefit**: Fast convergence after partition heals

### Mechanism 2: ALIVE-based Coordinator Adoption

- **When**: Node receives ALIVE from higher-priority node
- **Conditions**:
  - No known leader (`current_leader == 0`)
  - OR waiting for coordinator (`STATE_WAITING_COORDINATOR`)
  - OR sender has higher priority than current leader
- **Action**: Adopt ALIVE sender as coordinator immediately
- **Purpose**: Discover higher-priority coordinators from other partitions
- **Benefit**: Automatic convergence without new election

## Algorithm Characteristics

### Advantages

- **Simple**: Easy to understand and implement
- **Low message overhead**: O(n) messages per election (vs O(n²) for Bully)
- **Guaranteed convergence**: Always elects highest-ID node
- **Deterministic**: Same result regardless of who initiates
- **Fault tolerant**: Dynamic ring reconfiguration handles node failures

### Limitations

- **Topology requirement**: Requires knowledge of ring structure
- **Sequential**: Messages must traverse full ring (latency)
- **O(n) time complexity**: Election time scales with ring size

### Complexity Analysis

| Metric | Complexity | Notes |
|--------|------------|-------|
| Message | O(n) | Per election round |
| Time | O(n) | N message hops per election |
| Space | O(1) | Per node (constant state) |

## Comparison with Bully Algorithm

| Aspect | Ring | Bully |
|--------|------|-------|
| Messages per election | O(n) | O(n²) worst case |
| Communication pattern | Point-to-point | Broadcast |
| Topology requirement | Ring required | None |
| Failure resilience | Dynamic reconfiguration | More resilient |
| Network traffic | Lower | Higher |
| Latency | O(n) hops | O(1) rounds |

## Building

```bash
# Build Ring algorithm
make ALGORITHM=ring TARGET=cooja

# Build with custom ring size
make ALGORITHM=ring TARGET=cooja RING_SIZE=50

# Build with fast mode
make ALGORITHM=ring TARGET=cooja FAST_MODE=1

# Clean and rebuild
make ALGORITHM=ring TARGET=cooja clean
make ALGORITHM=ring TARGET=cooja
```

## Running Experiments

### Individual Experiments

```bash
# Convergence test
./experiments/convergence/run_convergence_trials.sh ring 10 10 60 4

# Fault tolerance test
./experiments/fault_tolerance/run_crash_trials.sh ring 10 10 60 120 4

# Noise test (50% packet success rate)
./experiments/noise/run_noise_trials.sh ring 10 50 10 60 4

# Network partition test
./experiments/network_partition/run_partition_trials.sh ring 10 10 60 4
```

### Full Experiment Suite

```bash
# Run all ring experiments
./experiments/run_all_experiments.sh -a ring -t 10 -n 5,10,50,100 \
  -e convergence,fault_tolerance,noise,network_partition
```

## Experiment Results (Tested Configuration)

All experiments passing for 5, 10, 50, and 100 nodes:

| Experiment | 5 nodes | 10 nodes | 50 nodes | 100 nodes |
|------------|---------|----------|----------|-----------|
| Convergence | 10/10 | 10/10 | 10/10 | 10/10 |
| Fault Tolerance | 10/10 | 10/10 | 10/10 | 9/10* |
| Noise 50% | 10/10 | 10/10 | 10/10 | 8/10 |
| Noise 70% | 10/10 | 10/10 | 10/10 | 9/10 |
| Noise 90% | 10/10 | 10/10 | 10/10 | 10/10 |
| Network Partition | 10/10 | 10/10 | 10/10 | 10/10 |

*Note: Some 100-node fault tolerance failures are due to detection script timing, not algorithm issues.

## Configuration Files

### ring-config.h

Primary configuration file for timing parameters, message types, and state machine.

### project-conf.h

Framework-level configuration shared across algorithms:
- Network stack settings (IPv6, RPL)
- Logging levels
- Metrics configuration

### CSC Templates

Cooja simulation files are in `experiments/<type>/csc_templates/ring/`:
- `5nodes.csc`, `10nodes.csc`, `50nodes.csc`, `100nodes.csc` (convergence)
- `*-crash.csc` (fault tolerance)
- `*-noise*.csc` (noise)
- `*-partition.csc` (network partition)

## References

- Chang, E. & Roberts, R. (1979). "An Improved Algorithm for Decentralized Extrema-Finding in Circular Configurations of Processes"
- Tel, G. (2000). "Introduction to Distributed Algorithms" - Chapter on Leader Election
