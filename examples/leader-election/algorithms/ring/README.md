# Ring Leader Election Algorithm

The Ring algorithm is a classic distributed leader election algorithm where election messages circulate around a logical ring. Each node forwards the message to its successor, updating the candidate if it has a higher ID.

## Algorithm Overview

1. **Election Initiation**: Any node can start an election by sending an ELECTION message to its successor in the ring.

2. **Message Forwarding**: Each node compares its ID with the current candidate:
   - If its ID is higher, it becomes the new candidate
   - The message is forwarded to the next node in the ring

3. **Election Completion**: When the ELECTION message returns to the initiator:
   - The candidate in the message is the winner (highest ID)
   - The winner broadcasts a COORDINATOR message around the ring

4. **Leader Monitoring**: The coordinator periodically sends ALIVE messages around the ring.

## Message Types

| Type | Code | Description |
|------|------|-------------|
| ELECTION | 1 | Election message circulating the ring |
| COORDINATOR | 2 | Coordinator announcement |
| ALIVE | 3 | Heartbeat from coordinator |

## Node States

| State | Description |
|-------|-------------|
| NORMAL | Regular operation with a known leader |
| ELECTION | Currently participating in an election process |
| WAITING_COORDINATOR | Waiting for coordinator announcement |

## Network Stack

This implementation uses **nullnet** for lightweight communication:

- **Network**: nullnet (minimal network layer)
- **MAC**: CSMA (Carrier Sense Multiple Access)
- **Topology**: Logical ring

## Timing Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `ELECTION_TIMEOUT` | 8 seconds | Time to wait before starting new election |
| `COORDINATOR_TIMEOUT` | 15 seconds | Time to wait for coordinator or detect leader failure |
| `ALIVE_INTERVAL` | 10 seconds | Interval for leader heartbeat messages |
| `RANDOM_DELAY_MAX` | 3 seconds | Maximum random startup delay |
| `RING_SIZE` | 10 | Number of nodes in the ring |

## Message Structure

```c
typedef struct {
  uint8_t type;
  uint16_t initiator_id;    // Node that started the election
  uint16_t candidate_id;    // Current best candidate (highest ID)
  uint16_t sequence;
  uint16_t target_node_id;  // Which node should process this message
} ring_msg_t;
```

## Ring Topology

Nodes are organized in a logical ring:
- Node 1 -> Node 2 -> Node 3 -> ... -> Node N -> Node 1

The successor of node `i` is computed as:
```c
next_node = (node_id >= RING_SIZE) ? 1 : node_id + 1;
```

## Algorithm Characteristics

### Advantages
- Simple and easy to understand
- Guaranteed convergence to highest-ID node
- Message complexity: O(n) per election round

### Limitations
- Requires knowledge of ring topology
- Single point of failure in ring structure
- All messages must traverse entire ring

### Complexity

| Metric | Complexity |
|--------|------------|
| Message | O(n) per election |
| Time | O(n) rounds |
| Space | O(1) per node |

## Configuration Parameters

Set these in `ring-config.h` or via project-conf.h:

```c
#define RING_SIZE 10           // Number of nodes in ring
#define ELECTION_TIMEOUT  (8 * CLOCK_SECOND)
#define COORDINATOR_TIMEOUT (15 * CLOCK_SECOND)
#define ALIVE_INTERVAL (10 * CLOCK_SECOND)
```

## Testing Scenarios

### 1. Normal Election
1. Start all nodes
2. Observe election message traveling around the ring
3. Verify highest-ID node becomes coordinator

### 2. Leader Failure
1. Stop the coordinator node
2. Observe timeout and new election initiation
3. Verify new highest-ID node becomes coordinator

### 3. Ring Break
1. Stop a node in the middle of the ring
2. Observe election failure due to broken ring
3. This demonstrates ring algorithm's vulnerability to node failures

## References

- Chang, E. & Roberts, R. (1979). "An Improved Algorithm for Decentralized Extrema-Finding in Circular Configurations of Processes"
