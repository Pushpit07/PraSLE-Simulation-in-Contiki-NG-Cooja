# Bully Leader Election Algorithm

The Bully algorithm is a classic distributed leader election algorithm where the node with the highest ID always becomes the coordinator.

## Algorithm Overview

The Bully algorithm works as follows:

1. **Election Initiation**: When a node detects that the current leader has failed (or on startup), it initiates an election by sending ELECTION messages to all nodes with higher IDs.

2. **Response Handling**: Nodes with higher IDs respond with ANSWER messages and start their own elections.

3. **Coordinator Selection**: If no higher-priority node responds within a timeout, the initiating node declares itself as the coordinator and broadcasts a COORDINATOR message.

4. **Leader Monitoring**: The elected leader periodically sends ALIVE messages to indicate it's still functioning.

## Message Types

| Type | Code | Description |
|------|------|-------------|
| ELECTION | 1 | Sent to nodes with higher IDs to initiate election |
| ANSWER | 2 | Response to ELECTION from higher-priority nodes |
| COORDINATOR | 3 | Broadcast announcement of the new leader |
| ALIVE | 4 | Periodic heartbeat from the current leader |

## Node States

| State | Description |
|-------|-------------|
| NORMAL | Regular operation with a known leader |
| ELECTION | Currently participating in an election process |
| WAITING_COORDINATOR | Waiting for coordinator announcement after receiving ANSWER |

## Network Stack

This implementation uses **IPv6 with RPL Lite routing** for communication:

- **Transport**: UDP on port 8765
- **Network**: IPv6 with link-local multicast (`ff02::1`)
- **Routing**: RPL Lite (lightweight routing protocol for IoT)
- **MAC**: CSMA (Carrier Sense Multiple Access)

**Note**: Link-local multicast (`ff02::1`) only reaches nodes in direct radio range.

## Paper Reference

The Bully algorithm was introduced by Hector Garcia-Molina:

> H. Garcia-Molina, "Elections in a Distributed Computing System,"
> IEEE Transactions on Computers, vol. C-31, no. 1, pp. 48-59, Jan. 1982.
> DOI: [10.1109/TC.1982.1675885](https://doi.org/10.1109/TC.1982.1675885)

### Timing Parameter T in the Paper

The paper defines the timing parameter **T** in Section III, page 50:

> "T: an upper bound on the time required to send a message from any process to any other."

The paper does **not** specify a concrete value for T, as it depends on the network characteristics. Our implementation uses **T = 2 seconds** as a practical adaptation for 802.15.4 wireless sensor networks.

## Timing Configuration

### Normal Mode (Default)

Based on Garcia-Molina's timing model where T is the upper bound for message delivery:

| Parameter | Value | Derivation | Description |
|-----------|-------|------------|-------------|
| `ELECTION_TIMEOUT` | 4s | 2T | Time to wait for ANSWER messages during election |
| `COORDINATOR_TIMEOUT` | 4s | 2T | Time to wait for COORDINATOR announcement or detect leader failure |
| `ALIVE_INTERVAL` | 2s | T | Interval for leader heartbeat messages |
| `RANDOM_DELAY_MAX` | 2s | T | Maximum random startup delay |

### Fast Mode (BULLY_FAST_MODE=1)

Reduced timeouts for quick testing and simulation:

| Parameter | Value | Description |
|-----------|-------|-------------|
| `ELECTION_TIMEOUT` | 1s | Faster response waiting |
| `COORDINATOR_TIMEOUT` | 3s | Faster failure detection |
| `ALIVE_INTERVAL` | 1s | More frequent heartbeats |
| `RANDOM_DELAY_MAX` | 1s | Faster startup |

To enable fast mode:
```bash
make ALGORITHM=bully TARGET=cooja FAST_MODE=1
```

**Important**: `COORDINATOR_TIMEOUT` should be > `ALIVE_INTERVAL` to prevent false-positive leader failures.

## Message Structure

```c
typedef struct {
  uint8_t type;        // Message type (1-4)
  uint16_t node_id;    // Sender's node ID (used as priority)
  uint16_t target_id;  // Target node ID (0 = broadcast)
  uint16_t sequence;   // Election sequence number
} bully_msg_t;
```

## Network Partitioning and Split-Brain Behavior

### Partition Behavior

When the network partitions, this implementation exhibits **correct distributed algorithm behavior**:

1. **Each partition independently elects its own coordinator**
   - Partition 1: Nodes {1, 2, 4} -> Node 4 becomes coordinator
   - Partition 2: Nodes {3, 5, 6} -> Node 6 becomes coordinator

2. **Multiple coordinators exist simultaneously**
   - This is **not a bug** - it's expected behavior for partitioned networks
   - Each coordinator only manages nodes within its partition

3. **Partition healing**
   - Two mechanisms for fast partition healing:
     - **Coordinator re-announcement**: Coordinators re-broadcast COORDINATOR when receiving ELECTION messages
     - **ALIVE-based adoption**: Nodes adopt higher-priority coordinators discovered via ALIVE messages
   - When partitions reconnect, nodes quickly discover the higher-priority coordinator

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
- Nodes in Partition A receive ALIVE from Node 6
- System converges back to Node 6 as single coordinator

## Partition Healing Mechanisms

### Mechanism 1: Coordinator Re-announcement

**When**: A coordinator receives an ELECTION message from a lower-priority node

**Action**: The coordinator immediately re-broadcasts COORDINATOR (in addition to sending ANSWER)

**Purpose**: Allows nodes that missed the original COORDINATOR announcement to immediately recognize the current leader

### Mechanism 2: ALIVE-based Coordinator Adoption

**When**: A node receives an ALIVE message from a higher-priority node

**Action**: The node adopts the ALIVE sender as coordinator if:
1. `sender_id > my_node_id` (sender has higher priority)
2. AND at least one of:
   - `current_leader == 0` (no known leader)
   - `state == STATE_WAITING_COORDINATOR`
   - `sender_id > current_leader` (sender has higher priority than current leader)

## Implementation Details

### Timer Event Handling

The election timer handler checks for both states to ensure proper handling:
```c
if (state == STATE_ELECTION || state == STATE_WAITING_COORDINATOR) {
  // Handle timer expiry regardless of current state
}
```

### Duplicate Message Detection

- **ELECTION messages**: Filtered using per-node sequence tracking
- **ANSWER messages**: NOT filtered (multiple ANSWER messages with same sequence are valid)
- **COORDINATOR messages**: NOT filtered (ELECTION and COORDINATOR can share same sequence)
- **ALIVE messages**: NOT filtered (heartbeats must always reset coordinator timeout timer)

### Coordinator Validation

When receiving a COORDINATOR message, nodes validate the coordinator's priority:
```c
if (sender_id >= my_node_id) {
  // Accept coordinator
} else {
  // Reject and start own election
}
```

## Testing Scenarios

### Test Scenario 1: Normal Operation (All Nodes Connected)

**Setup**: Start all nodes within radio range of each other

**Expected Behavior**:
1. Random startup delays (0-5 seconds) stagger initial elections
2. Multiple elections may occur as nodes discover higher-priority nodes
3. Highest-ID node becomes coordinator after ~5-10 seconds
4. Logs show ALIVE messages from coordinator every 8 seconds
5. All nodes receive and acknowledge ALIVE messages
6. NO further election messages after system stabilizes
7. System remains stable indefinitely

**Success Criteria**: Single coordinator (highest ID), periodic ALIVE messages, no election storms

---

### Test Scenario 2: Leader Failure

**Setup**: Wait for stable coordinator (highest-ID node sending ALIVE messages)

**Steps**:
1. Stop/kill the coordinator using Cooja (right-click -> Stop node)
2. Wait and observe logs

**Expected Behavior**:
1. ALIVE messages from coordinator stop
2. After ~10 seconds (COORDINATOR_TIMEOUT), nodes detect failure
3. Nodes log "Coordinator timeout - no ALIVE received, starting election"
4. Next highest-ID node wins election and becomes new coordinator
5. New coordinator begins sending ALIVE messages every 8 seconds
6. System stabilizes with new leader

**Success Criteria**: Clean failover within 10-15 seconds, no election storms

---

### Test Scenario 3: Network Partition (Split-Brain)

**Setup**: Position nodes in two separate groups out of radio range

**Example Partition**:
- **Group A**: Nodes 1, 2, 4 (close together)
- **Group B**: Nodes 3, 5, 6 (close together, far from Group A)

**Expected Behavior**:
1. Each partition independently elects a coordinator
   - Group A: Node 4 becomes coordinator
   - Group B: Node 6 becomes coordinator
2. Group A logs show Node 4 sending/receiving ALIVE messages
3. Group B logs show Node 6 sending/receiving ALIVE messages
4. Nodes in Group A show NO activity from nodes in Group B (and vice versa)
5. Both partitions operate independently and stably

**Success Criteria**: Two independent coordinators, each partition stable, no cross-partition communication

---

### Test Scenario 4: Partition Healing

**Setup**: Continue from Test Scenario 3 (partitioned network)

**Test Variant A - Active Discovery (Mechanism 1)**:

**Steps**:
1. Move a node from Group A into Group B's range
2. Observe logs showing coordinator re-announcement mechanism

**Expected Behavior**:
1. Moved node's coordinator_timer expires (no ALIVE from its old leader)
2. Node starts election
3. Coordinator in new partition receives ELECTION
4. **Mechanism 1 triggers**: Coordinator re-announces status
5. Moved node immediately receives COORDINATOR and adopts new leader
6. Moved node begins acknowledging ALIVE messages

**Convergence Time**: < 5 seconds after first contact

**Test Variant B - Passive Discovery (Mechanism 2)**:

**Steps**:
1. Move a node into range while coordinator is sending periodic ALIVE

**Expected Behavior**:
1. Node receives ALIVE from higher-priority coordinator
2. **Mechanism 2 triggers**: Node adopts coordinator via ALIVE discovery
3. Node immediately begins acknowledging ALIVE messages

**Convergence Time**: < 8 seconds (within one ALIVE_INTERVAL)

**Success Criteria**: Fast convergence, no temporary split-brain, all nodes acknowledge single coordinator

---

### Test Scenario 5: Multiple Cascading Failures

**Setup**: Start with all nodes, wait for highest-ID node to become coordinator

**Steps**:
1. Stop highest-ID node -> Wait for next node to become coordinator
2. Stop that node -> Wait for next node to become coordinator
3. Continue stopping nodes
4. Restart original highest-ID node -> Wait for system to reconverge

**Expected Behavior**:
1. Each failure triggers election within 10 seconds
2. Next highest-priority node becomes coordinator
3. When original leader restarts, it initiates election
4. System recognizes it as highest priority and elects it
5. Final state: Original leader is coordinator

**Success Criteria**: Clean transitions at each failure, original leader reclaims leadership when restarted

## References

- Garcia-Molina, H. (1982). "Elections in a Distributed Computing System," IEEE Transactions on Computers, vol. C-31, no. 1, pp. 48-59. DOI: [10.1109/TC.1982.1675885](https://doi.org/10.1109/TC.1982.1675885)
