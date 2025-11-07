# Bully Leader Election Algorithm in Contiki-NG

This example implements the **Bully Leader Election Algorithm** in Contiki-NG for IoT networks. The bully algorithm is a distributed algorithm for leader election in distributed systems where nodes can fail and recover.

## Algorithm Overview

The Bully algorithm works as follows:

1. **Election Initiation**: When a node detects that the current leader has failed (or on startup), it initiates an election by sending ELECTION messages to all nodes with higher IDs.

2. **Response Handling**: Nodes with higher IDs respond with ANSWER messages and start their own elections.

3. **Coordinator Selection**: If no higher-priority node responds within a timeout, the initiating node declares itself as the coordinator and broadcasts a COORDINATOR message.

4. **Leader Monitoring**: The elected leader periodically sends ALIVE messages to indicate it's still functioning.

## Files Description

- **`bully-node.c`**: Main implementation of the bully algorithm
- **`Makefile`**: Build configuration for the project
- **`project-conf.h`**: Project-specific configuration settings
- **`bully-cooja.csc`**: Cooja simulation configuration with 6 nodes
- **`README.md`**: This documentation file

## Message Types

The implementation uses four types of messages:

1. **ELECTION (1)**: Sent to nodes with higher IDs to initiate election
2. **ANSWER (2)**: Response to ELECTION messages from higher-priority nodes
3. **COORDINATOR (3)**: Broadcast announcement of the new leader
4. **ALIVE (4)**: Periodic heartbeat from the current leader

## Node States

Each node can be in one of three states:

- **NORMAL**: Regular operation with a known leader
- **ELECTION**: Currently participating in an election process
- **WAITING_COORDINATOR**: Waiting for coordinator announcement after receiving ANSWER

## Network Stack

This implementation uses **IPv6 with RPL Lite routing** for communication:

- **Transport**: UDP on port 8765
- **Network**: IPv6 with link-local multicast (`ff02::1`)
- **Routing**: RPL Lite (lightweight routing protocol for IoT)
- **MAC**: CSMA (Carrier Sense Multiple Access)

**Note**: Link-local multicast (`ff02::1`) only reaches nodes in direct radio range. For true multi-hop routing across the entire network, you would need to:
1. Configure one node as RPL DODAG root
2. Use site-local multicast (`ff05::1`) or application-level flooding
3. Wait for RPL topology to converge before starting elections

The current implementation accepts **partition-tolerant behavior** where separated network segments elect independent coordinators.

## Network Partitioning and Split-Brain Behavior

### What is Network Partitioning?

A **network partition** (or "split-brain") occurs when nodes cannot communicate with each other due to:
- Physical distance exceeding radio range
- RF interference or obstacles
- Link failures
- Intentional network segmentation

### Partition Behavior

When the network partitions, this implementation exhibits **correct distributed algorithm behavior**:

1. **Each partition independently elects its own coordinator**
   - Partition 1: Nodes {1, 2, 4} → Node 4 becomes coordinator
   - Partition 2: Nodes {3, 5, 6} → Node 6 becomes coordinator
   - Each partition elects the highest-priority node available to it

2. **Multiple coordinators exist simultaneously**
   - This is **not a bug** - it's expected behavior for partitioned networks
   - Each coordinator only manages nodes within its partition
   - Nodes cannot detect coordinators they cannot communicate with

3. **Partition healing**
   - This implementation includes two mechanisms for fast partition healing:
     - **Coordinator re-announcement**: Coordinators re-broadcast COORDINATOR when receiving ELECTION messages
     - **ALIVE-based adoption**: Nodes adopt higher-priority coordinators discovered via ALIVE messages
   - When partitions reconnect, nodes quickly discover the higher-priority coordinator
   - System converges to a single coordinator (the globally highest-priority node) within seconds
   - See "Partition Healing Mechanisms" section for detailed explanation

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
```
[1]---[2]---[3]
             |
[4]---[5]---[6]
```
- Nodes in Partition A receive ALIVE from Node 6
- Node 5 detects higher-priority coordinator
- System converges back to Node 6 as single coordinator

### Why This is Correct Behavior

The Bully algorithm (and most leader election algorithms) are designed to handle partitions this way:

- **Availability over consistency**: Each partition can continue operating independently
- **Eventual consistency**: When partitions heal, the system converges to a single leader
- **No false assumptions**: Nodes don't assume unreachable nodes are "dead" - they just elect from reachable nodes

### Observing Partition Behavior in Logs

When viewing simulation logs, you may see:
```
Node 6: Broadcasting ALIVE
Node 3: Received ALIVE from node 6 - Leader 6 is alive
Node 5: Received ALIVE from node 6 - Leader 6 is alive
(Nodes 1, 2, 4 show no activity - they're in a different partition)

Node 4: No coordinator announcement received, starting new election
Node 4: Broadcasting COORDINATOR
Node 1: New coordinator: node 4
Node 2: New coordinator: node 4
```

This indicates Partition A (nodes 1, 2, 4) and Partition B (nodes 3, 5, 6) operating independently.

### Testing Partition Behavior

In Cooja simulator:
1. **Create a partition**: Move nodes physically apart or reduce radio range
2. **Observe dual coordinators**: Each partition elects its own leader
3. **Heal the partition**: Move nodes back into range
4. **Observe convergence**: Lower-priority coordinator detects higher-priority leader and steps down

## Timing Configuration

The following timing parameters can be adjusted in `bully-node.c`:

- `ELECTION_TIMEOUT`: 5 seconds - Time to wait for ANSWER messages during election
- `COORDINATOR_TIMEOUT`: 20 seconds - Time to wait for COORDINATOR announcement or detect leader failure
- `ALIVE_INTERVAL`: 8 seconds - Interval for leader heartbeat messages
- `RANDOM_DELAY_MAX`: 5 seconds - Maximum random startup delay

**Important**: `COORDINATOR_TIMEOUT` must be > 2× `ALIVE_INTERVAL` to prevent false-positive leader failures.

## Prerequisites

1. **Contiki-NG**: Ensure you have Contiki-NG properly installed
2. **Cooja Simulator**: Part of the Contiki-NG tools
3. **Java**: Required for running Cooja

## Building the Simulation

### Step 1: Navigate to the project directory
```bash
cd examples/bully
```

### Step 2: Compile the project
```bash
make bully-node.cooja TARGET=cooja
```

This will compile the bully-node application for the Cooja simulator.

## Running the Simulation

### Step 1: Start Cooja
From the Contiki-NG root directory:
```bash
cd tools/cooja
./gradlew run
```

### Step 2: Load the simulation
1. In Cooja, go to **File → Open Simulation**
2. Navigate to `examples/bully/bully-cooja.csc`
3. Click **Open**

### Step 3: Start the simulation
Click the **Start** button in the Simulation Control window.

## Simulation Setup

The provided simulation includes:

- **6 nodes** arranged in a 2×3 grid
- **Node IDs**: 1, 2, 3, 4, 5, 6 (node 6 has the highest priority)
- **Radio range**: 50 meters transmission, 100 meters interference
- **Network topology**: All nodes can communicate with each other

## Expected Behavior

### Connected Network (All Nodes in Range)

1. **Initial Election**: All nodes start simultaneously and initiate elections after random delays
2. **Leader Selection**: Node 6 (highest ID) becomes the leader after ~5-10 seconds
3. **Steady State**: Node 6 sends periodic ALIVE messages every 8 seconds
4. **Failure Simulation**: Pause/kill node 6 to trigger a new election
5. **Recovery**: Node 5 should become the new leader within ~20 seconds

### Partitioned Network (Nodes Out of Range)

1. **Multiple Coordinators**: Each partition elects its own coordinator
   - If nodes {1,2,4} are isolated: Node 4 becomes coordinator
   - If nodes {3,5,6} are isolated: Node 6 becomes coordinator
2. **Independent Operation**: Each partition operates independently with its own leader
3. **Partition Healing**: When connectivity restores, system converges to single coordinator (Node 6)

**Note**: Multiple coordinators in a partitioned network is **correct behavior**, not a bug. See the "Network Partitioning and Split-Brain Behavior" section for details.

## Observing the Simulation

### Log Window
The Log Listener window shows detailed information about:
- Election processes
- Message exchanges
- Leader announcements
- State transitions

### Timeline Window
The Timeline window displays:
- Radio activity (TX/RX)
- LED states
- Timing relationships between nodes

### Network Visualization
The Visualizer window shows:
- Node positions
- Radio ranges
- Message transmissions

## Testing Scenarios

### 1. Normal Election Process
1. Start the simulation
2. Observe that node 6 becomes the leader
3. Check for periodic ALIVE messages

### 2. Leader Failure
1. Right-click on node 6 → "Mote Tools" → "Stop node"
2. Observe the election process
3. Verify that node 5 becomes the new leader

### 3. Multiple Failures
1. Stop nodes 6 and 5
2. Observe that node 4 becomes the leader
3. Restart node 6 and watch it reclaim leadership

### 4. Network Partitioning
1. Move nodes out of radio range
2. Observe multiple leaders in different partitions
3. Merge partitions and watch leader resolution


## Testing Recommendations

### Test Scenario 1: Normal Operation (All Nodes Connected)
**Setup**: Start all 6 nodes within radio range of each other

**Expected Behavior**:
1. Random startup delays (0-5 seconds) stagger initial elections
2. Multiple elections may occur as nodes discover higher-priority nodes
3. Node 6 becomes coordinator after ~5-10 seconds
4. Logs show ALIVE messages from Node 6 every 8 seconds
5. All nodes receive and acknowledge ALIVE messages
6. NO further election messages after system stabilizes
7. System remains stable indefinitely

**Success Criteria**: Single coordinator (Node 6), periodic ALIVE messages, no election storms

### Test Scenario 2: Leader Failure
**Setup**: Wait for stable coordinator (Node 6 sending ALIVE messages)

**Steps**:
1. Stop/kill Node 6 using Cooja (right-click → Stop node)
2. Wait and observe logs

**Expected Behavior**:
1. ALIVE messages from Node 6 stop
2. After ~20 seconds (COORDINATOR_TIMEOUT), nodes detect failure
3. Nodes log "Coordinator timeout - no ALIVE received, starting election"
4. Node 5 wins election and becomes new coordinator
5. Node 5 begins sending ALIVE messages every 8 seconds
6. System stabilizes with Node 5 as leader

**Success Criteria**: Clean failover to Node 5 within 20-25 seconds, no election storms

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

### Test Scenario 4: Partition Healing
**Setup**: Continue from Test Scenario 3 (partitioned network with Node 4 as coordinator in Group A, Node 6 as coordinator in Group B)

**Test Variant A - Active Discovery (Mechanism 1)**:
**Steps**:
1. Move Node 4 from Group A into Group B's range (near Node 5 or Node 6)
2. Observe logs showing coordinator re-announcement mechanism

**Expected Behavior**:
1. Node 4's coordinator_timer expires (no ALIVE from Node 6 for 20s)
2. Node 4 starts election: `ID:4 Starting election (sequence X)`
3. Node 5 (coordinator in new partition) receives ELECTION
4. **Mechanism 1 triggers**: Node 5 re-announces: `ID:5 Re-announcing coordinator status to help partition healing`
5. Node 4 immediately receives COORDINATOR: `ID:4 New coordinator: node 5`
6. Node 4 begins acknowledging ALIVE: `ID:4 Leader 5 is alive`

**Convergence Time**: < 5 seconds after first contact

**Test Variant B - Passive Discovery (Mechanism 2)**:
**Steps**:
1. Manually set Node 4's `current_leader = 0` (simulating it cleared its leader)
2. Move Node 4 into Group B's range while Node 5 is sending periodic ALIVE

**Expected Behavior**:
1. Node 4 receives ALIVE from Node 5
2. **Mechanism 2 triggers**: Node 4 adopts Node 5: `ID:4 Adopting node 5 as coordinator (discovered via ALIVE)`
3. Node 4 immediately begins acknowledging: `ID:4 Leader 5 is alive`

**Convergence Time**: < 8 seconds (within one ALIVE_INTERVAL)

**Success Criteria**:
- Variant A: Coordinator re-announcement log visible, convergence < 5s
- Variant B: ALIVE adoption log visible, convergence < 8s
- Both: No temporary split-brain, no unnecessary elections, all nodes acknowledge single coordinator

### Test Scenario 5: Multiple Cascading Failures
**Setup**: Start with all nodes, wait for Node 6 to become coordinator

**Steps**:
1. Stop Node 6 → Wait for Node 5 to become coordinator
2. Stop Node 5 → Wait for Node 4 to become coordinator
3. Stop Node 4 → Wait for Node 3 to become coordinator
4. Restart Node 6 → Wait for system to reconverge

**Expected Behavior**:
1. Each failure triggers election within 20 seconds
2. Next highest-priority node becomes coordinator
3. When Node 6 restarts, it initiates election
4. System recognizes Node 6 as highest priority and elects it
5. Final state: Node 6 is coordinator

**Success Criteria**: Clean transitions at each failure, Node 6 reclaims leadership when restarted


## Customization

### Adding More Nodes
1. Edit `bully-cooja.csc`
2. Add new `<mote>` entries with unique IDs
3. Position them within radio range

### Changing Timing Parameters
1. Edit `bully-node.c`
2. Modify the `#define` statements for timing
3. Recompile the project

### Network Topology
1. Modify node positions in the `.csc` file
2. Adjust radio ranges for different topologies
3. Create isolated groups to test partitioning

## Troubleshooting

### Compilation Issues
- Ensure you're in the correct directory
- Check that Contiki-NG is properly installed
- Verify that the `CONTIKI` path in Makefile is correct

### Simulation Not Starting
- Check Java installation
- Ensure Cooja can find the compiled `.cooja` file
- Verify that all mote interfaces are supported

### No Elections Occurring
- Check that IPv6 and UDP are properly configured
- Verify `UDP_PORT` matches across all nodes (default: 8765)
- Ensure nodes are within radio range (link-local multicast only reaches direct neighbors)
- Check that `simple_udp_register()` succeeded during initialization
- Verify radio settings in project-conf.h

## Implementation Details

### Node ID Assignment
Node IDs are derived from the linkaddr, ensuring unique identifiers across the network.

### Message Structure
```c
typedef struct {
  uint8_t type;        // Message type (1-4)
  uint16_t node_id;    // Sender's node ID (used as priority)
  uint16_t target_id;  // Target node ID (0 = broadcast)
  uint16_t sequence;   // Election sequence number
} bully_msg_t;
```

The `target_id` field enables directed messages (e.g., ANSWER responses) while still using broadcast transmission.

### State Machine
The implementation uses a simple state machine to track election progress and ensure proper message handling.

## Key Implementation Details

### Timer Event Handling

The implementation includes a critical fix for Contiki-NG timer event handling:

**Problem**: When a node receives an ANSWER message and transitions to `STATE_WAITING_COORDINATOR`, the `election_timer` may still be running. When this timer expires, the original code checked `if (state == STATE_ELECTION)`, which would fail, causing the event to be consumed without proper handling. This left the event loop in an inconsistent state, causing nodes to stop processing events entirely.

**Solution**: The election timer handler now checks for both states:
```c
if (state == STATE_ELECTION || state == STATE_WAITING_COORDINATOR) {
  // Handle timer expiry regardless of current state
}
```

This ensures timer events are always properly handled, keeping the Contiki-NG event loop functioning correctly.

### Duplicate Message Detection

The implementation uses sequence number tracking to detect and filter duplicate messages:

- **ELECTION messages**: Filtered using per-node sequence tracking
- **ANSWER messages**: NOT filtered (multiple ANSWER messages with same sequence are valid)
- **COORDINATOR messages**: NOT filtered (ELECTION and COORDINATOR can share same sequence)
- **ALIVE messages**: NOT filtered (heartbeats must always reset coordinator timeout timer)

Rationale: Within a single election, multiple message types share the same sequence number. Filtering ANSWER/COORDINATOR messages would cause nodes to reject valid messages.

### Coordinator Validation

When receiving a COORDINATOR message, nodes validate the coordinator's priority:

```c
if (sender_id >= my_node_id) {
  // Accept coordinator
} else {
  // Reject and start own election
}
```

This ensures only higher-or-equal priority nodes can be coordinators, maintaining algorithm correctness.

### Timer Reset on ALIVE Messages

The `coordinator_timer` is reset every time an ALIVE message is received from the current leader:

```c
if (sender_id == current_leader) {
  etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
}
```

This is the core failure detection mechanism - as long as ALIVE messages arrive within 20 seconds, the timer keeps resetting. When the coordinator fails, ALIVE messages stop, the timer expires, and a new election is triggered.

### Partition Healing Mechanisms

This implementation includes two complementary mechanisms for fast partition healing when nodes move into or out of radio range:

#### Mechanism 1: Coordinator Re-announcement

**When**: A coordinator receives an ELECTION message from a lower-priority node

**Action**: The coordinator immediately re-broadcasts COORDINATOR (in addition to sending ANSWER)

**Purpose**: Allows nodes that missed the original COORDINATOR announcement to immediately recognize the current leader

**Example Scenario**:
```
t=0:   Node 5 is coordinator in Partition A (nodes 3, 5)
       Node 4 is out of range
t=10:  Node 4 moves into range, detects no coordinator
t=20:  Node 4 starts election, broadcasts ELECTION
t=20:  Node 5 receives ELECTION, sends ANSWER + re-broadcasts COORDINATOR
t=20:  Node 4 receives COORDINATOR, immediately adopts Node 5 as leader
```

**Without this mechanism**: Node 4 would wait up to COORDINATOR_TIMEOUT (20 seconds) before timing out and potentially electing itself, causing temporary split-brain even though Node 5 is reachable.

**Log Pattern**:
```
ID:4  Starting election (sequence 2)
ID:4  Broadcasting ELECTION
ID:5  Received ELECTION from node 4 (seq 2)
ID:5  Sending ANSWER to node 4
ID:5  Re-announcing coordinator status to help partition healing
ID:5  Broadcasting COORDINATOR
ID:4  Received COORDINATOR from node 5 (seq 2)
ID:4  New coordinator: node 5
```

#### Mechanism 2: ALIVE-based Coordinator Adoption

**When**: A node receives an ALIVE message from a higher-priority node

**Action**: The node adopts the ALIVE sender as coordinator if specific conditions are met

**Conditions** (ALL must be true):
1. `sender_id > my_node_id` (sender has higher priority than us)
2. At least ONE of:
   - `current_leader == 0` (we have no known leader)
   - `state == STATE_WAITING_COORDINATOR` (we're waiting for announcement)
   - `sender_id > current_leader` (sender has higher priority than our current leader)

**Purpose**: Provides passive coordinator discovery without requiring explicit COORDINATOR messages or ELECTION exchanges

**Example Scenario**:
```
t=0:   Node 6 is coordinator, Node 4 has current_leader = 6
t=10:  Node 4 moves out of Node 6's range
t=30:  Node 4's coordinator_timer expires (no ALIVE from Node 6)
t=30:  Node 4 clears current_leader (now 0)
t=35:  Node 4 moves into Node 5's range
t=40:  Node 5 broadcasts ALIVE (as coordinator of Partition A)
t=40:  Node 4 receives ALIVE from Node 5
       Checks: 5 > 4? Yes. current_leader == 0? Yes.
       → Adopts Node 5 immediately
```

**Without this mechanism**: Node 4 would need to start an election, triggering Mechanism 1. Mechanism 2 provides faster, passive discovery when the node isn't actively looking for a coordinator.

**Log Pattern**:
```
ID:4  Received ALIVE from node 5 (seq 2)
ID:4  Adopting node 5 as coordinator (discovered via ALIVE)
ID:4  (subsequent ALIVE messages)
ID:4  Received ALIVE from node 5 (seq 2)
ID:4  Leader 5 is alive
```

#### How Both Mechanisms Work Together

The two mechanisms are complementary:

- **Mechanism 1 (Re-announcement)**: Fast convergence when a node *actively searches* for a coordinator by starting an election
- **Mechanism 2 (ALIVE adoption)**: Fast convergence when a node *passively discovers* a coordinator by hearing ALIVE messages

Together, they ensure robust partition healing regardless of whether the node initiates an election or simply listens to ongoing ALIVE heartbeats.

**Comparison**:

| Scenario | Without Mechanisms | With Mechanism 1 | With Mechanism 2 | Both |
|----------|-------------------|------------------|------------------|------|
| Node moves into coordinator range and starts election | 20s timeout → self-elect | Immediate (< 1s) | 20s timeout | Immediate (< 1s) |
| Node moves into coordinator range, passively hears ALIVE | 20s timeout → self-elect | 20s timeout | Immediate (< 8s) | Immediate (< 8s) |
| Node waiting for coordinator, hears ALIVE | Wait indefinitely | Wait indefinitely | Immediate | Immediate |

**Coverage**: Both mechanisms are needed because:
- Not all nodes start elections when entering a partition (some may just be listening)
- Not all coordinators are guaranteed to hear ELECTION messages (packet loss)
- ALIVE messages are broadcast periodically (every 8s), providing continuous discovery opportunities

## Educational Value

This simulation demonstrates:
- **Distributed algorithms** in IoT networks
- **Leader election** mechanisms
- **Fault tolerance** in distributed systems
- **Message passing** using Contiki-NG
- **Network simulation** with Cooja

## Advanced: Enabling True Multi-Hop Routing

The current implementation uses link-local multicast which only reaches directly reachable neighbors. To enable **true multi-hop routing** where all nodes can communicate regardless of physical distance:

### Option A: Configure RPL Root + Site-Local Multicast

1. **Designate one node as RPL DODAG root** (typically the highest-priority node):
   ```c
   #include "net/routing/rpl-lite/rpl.h"

   // In PROCESS_THREAD, after UDP initialization:
   if (my_node_id == 6) {  // Node 6 becomes RPL root
     rpl_dag_root_init_dag_immediately();
     LOG_INFO("Configured as RPL DODAG root\n");
   }
   ```

2. **Change multicast address to site-local**:
   ```c
   // In send_message() and broadcast_message():
   uip_ipaddr_t dest_addr;
   uip_ip6addr(&dest_addr, 0xff05, 0, 0, 0, 0, 0, 0, 0x0001);  // ff05::1
   ```

3. **Add delay for RPL convergence**:
   ```c
   // Wait 30-60 seconds after startup before starting elections
   etimer_set(&rpl_wait_timer, 60 * CLOCK_SECOND);
   PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rpl_wait_timer));
   ```

### Option B: Application-Level Flooding

Implement hop-by-hop message flooding:

1. **Track recently seen messages** (already have sequence tracking)
2. **Re-broadcast once**: When receiving a message for the first time, re-broadcast it
3. **Stop propagation**: Don't re-broadcast messages already seen

This is simpler but generates more network traffic.

### Option C: Accept Partition-Tolerant Behavior (Current)

Keep the current implementation where:
- Partitions elect independent coordinators
- Suitable for networks where partitions are expected
- Lower complexity and overhead

## References

- Original Bully Algorithm: Garcia-Molina, H. (1982). "Elections in a Distributed Computing System"
- Contiki-NG Documentation: https://github.com/contiki-ng/contiki-ng
- Cooja Simulator Guide: Contiki-NG Wiki
- RPL Lite Documentation: Contiki-NG routing documentation