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

## Timing Configuration

The following timing parameters can be adjusted in `bully-node.c`:

- `ELECTION_TIMEOUT`: 3 seconds - Time to wait for ANSWER messages
- `COORDINATOR_TIMEOUT`: 10 seconds - Time to wait for COORDINATOR announcement
- `ALIVE_INTERVAL`: 5 seconds - Interval for leader heartbeat messages
- `RANDOM_DELAY_MAX`: 2 seconds - Maximum random startup delay

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

1. **Initial Election**: All nodes start simultaneously and initiate elections
2. **Leader Selection**: Node 6 (highest ID) becomes the leader
3. **Steady State**: Node 6 sends periodic ALIVE messages
4. **Failure Simulation**: You can pause/kill node 6 to trigger a new election
5. **Recovery**: Node 5 should become the new leader

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
- Check that nullnet is properly configured
- Verify radio settings in project-conf.h
- Ensure nodes are within radio range

## Implementation Details

### Node ID Assignment
Node IDs are derived from the linkaddr, ensuring unique identifiers across the network.

### Message Structure
```c
typedef struct {
  uint8_t type;        // Message type (1-4)
  uint16_t node_id;    // Sender's node ID
  uint16_t sequence;   // Election sequence number
} bully_msg_t;
```

### State Machine
The implementation uses a simple state machine to track election progress and ensure proper message handling.

## Educational Value

This simulation demonstrates:
- **Distributed algorithms** in IoT networks
- **Leader election** mechanisms
- **Fault tolerance** in distributed systems
- **Message passing** using Contiki-NG
- **Network simulation** with Cooja

## References

- Original Bully Algorithm: Garcia-Molina, H. (1982)
- Contiki-NG Documentation: https://github.com/contiki-ng/contiki-ng
- Cooja Simulator Guide: Contiki-NG Wiki