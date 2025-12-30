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

## Partition Healing Mechanisms

This implementation includes two partition healing mechanisms for robustness:

### Mechanism 1: Coordinator Re-announcement

- **When**: Coordinator receives ELECTION from any node
- **Action**: Re-broadcasts COORDINATOR message around ring
- **Purpose**: Quickly inform nodes that missed original COORDINATOR announcement
- **Benefit**: Fast convergence after partition heals (< ALIVE_INTERVAL)

### Mechanism 2: ALIVE-based Coordinator Adoption

- **When**: Node receives ALIVE from higher-priority node
- **Conditions**:
  - No known leader (`current_leader == 0`)
  - OR waiting for coordinator (`STATE_WAITING_COORDINATOR`)
  - OR sender has higher priority than current leader
- **Action**: Adopt ALIVE sender as coordinator immediately
- **Purpose**: Discover higher-priority coordinators from other partitions
- **Benefit**: Automatic convergence without new election

## Network Stack

This implementation uses **NullNet** for lightweight communication:

- **Network Layer**: NullNet (minimal, no routing overhead)
- **MAC Layer**: CSMA (Carrier Sense Multiple Access)
- **Topology**: Logical ring (simulated via target_node_id filtering)
- **Communication**: Broadcast with filtering (simulates point-to-point)

## Timing Configuration

Timing is consistent across all algorithms (Bully, Ring, PraSLE).

### Normal Mode (Default)

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| `ELECTION_TIMEOUT` | 5 seconds | Wait for election responses/completion |
| `COORDINATOR_TIMEOUT` | 10 seconds | ~1.25x ALIVE_INTERVAL for failure detection |
| `ALIVE_INTERVAL` | 8 seconds | Balance failure detection with network traffic |
| `RANDOM_DELAY_MAX` | 5 seconds | Stagger startup to prevent synchronized elections |
| `RING_SIZE` | 10 | Number of nodes in the ring |

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

## Message Structure

```c
typedef struct {
  uint8_t type;             /* MSG_ELECTION, MSG_COORDINATOR, MSG_ALIVE */
  uint16_t initiator_id;    /* Node that started this message chain */
  uint16_t candidate_id;    /* Current best candidate (highest ID) */
  uint16_t sequence;        /* Election sequence number */
  uint16_t target_node_id;  /* Next node in ring to process message */
} ring_msg_t;  /* Total: 9 bytes */
```

### Field Descriptions

- **type**: Message type identifier (1-3)
- **initiator_id**: Node that originally started this message chain
  - ELECTION: Node that started the election
  - COORDINATOR: Node that sends the announcement (election winner)
  - ALIVE: The coordinator
- **candidate_id**: Current best candidate (highest ID seen so far)
  - Updated as ELECTION message traverses the ring
  - When message returns to initiator, this is the winner
- **sequence**: Election sequence number for distinguishing election rounds
- **target_node_id**: Which node should process this message (ring successor)

## Ring Topology

Nodes are organized in a logical ring:
```
Node 1 → Node 2 → Node 3 → ... → Node N → Node 1
```

The successor is computed as:
```c
next_node = (node_id >= RING_SIZE) ? 1 : node_id + 1;
```

### Initial Election Strategy

Only the node with the highest ID (RING_SIZE) starts the initial election:
- Ensures exactly one election at boot time
- Minimizes message overhead
- That node will win anyway (highest priority)

## Algorithm Characteristics

### Advantages

- **Simple**: Easy to understand and implement
- **Low message overhead**: O(n) messages per election (vs O(n²) for Bully)
- **Guaranteed convergence**: Always elects highest-ID node
- **Deterministic**: Same result regardless of who initiates

### Limitations

- **Topology requirement**: Requires knowledge of ring structure
- **Single point of failure**: If a node fails, ring is broken
- **Sequential**: Messages must traverse full ring (latency)
- **All-or-nothing**: Can't complete election with partial ring

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
| Failure resilience | Ring break fatal | More resilient |
| Network traffic | Lower | Higher |
| Latency | O(n) hops | O(1) rounds |

## Design Features

1. **Ring-based message circulation** - O(n) message complexity
2. **Heartbeat-based failure detection** - ALIVE messages prove leader is functioning
3. **Partition healing** - Coordinator re-announcement and ALIVE-based adoption
4. **Timer management** - Proper reset on message receipt
5. **Target-based filtering** - Simulates point-to-point on broadcast network
6. **Sequence numbers** - Distinguish election rounds
7. **Metrics integration** - CSV output for experiment analysis
8. **Fast mode** - Reduced timeouts for testing

## Testing Scenarios

### 1. Normal Election

1. Start all nodes (Cooja simulation)
2. Observe highest-ID node initiating election
3. Watch ELECTION message travel the ring
4. Verify COORDINATOR announcement
5. Confirm leader heartbeats

### 2. Leader Failure

1. Run stable network with elected leader
2. Stop the coordinator node
3. Observe coordinator timeout detection
4. Watch new election initiation
5. Verify new leader election

### 3. Partition Healing

1. Create two partitions (temporarily)
2. Let each partition elect a leader
3. Heal the partition
4. Observe partition healing mechanisms:
   - ALIVE-based adoption of higher-priority leader
   - OR Coordinator re-announcement on ELECTION receipt

### 4. Ring Break (Edge Case)

1. Stop a non-leader node
2. Observe election message getting stuck
3. Watch election timeout and retry
4. Note: Ring algorithm cannot recover from permanent ring break

## Configuration

Set these in `ring-config.h` or override via `project-conf.h`:

```c
#define RING_SIZE 10                    // Number of nodes in ring
#define ELECTION_TIMEOUT  (12 * CLOCK_SECOND)
#define COORDINATOR_TIMEOUT (20 * CLOCK_SECOND)
#define ALIVE_INTERVAL (12 * CLOCK_SECOND)
```

## Building

```bash
# Build Ring algorithm
make ALGORITHM=ring TARGET=cooja

# Build with fast mode
make ALGORITHM=ring TARGET=cooja FAST_MODE=1

# Clean build
make ALGORITHM=ring TARGET=cooja clean
```

## References

- Chang, E. & Roberts, R. (1979). "An Improved Algorithm for Decentralized Extrema-Finding in Circular Configurations of Processes"
- Tel, G. (2000). "Introduction to Distributed Algorithms" - Chapter on Leader Election
