# Ring Leader Election Algorithm for Contiki-NG

This example implements the Ring Leader Election algorithm in Contiki-NG using the Cooja simulator. The ring algorithm is a distributed leader election algorithm that works by passing messages around a logical ring topology.

## Algorithm Overview

The Ring Leader Election algorithm works as follows:

1. **Ring Topology**: Nodes are arranged in a logical ring where each node knows its successor in the ring
2. **Election Process**: When an election starts, a node sends an ELECTION message containing its ID around the ring
3. **Candidate Selection**: As the message travels around the ring, each node compares its ID with the current candidate ID in the message. If a node has a higher ID, it replaces the candidate ID with its own
4. **Leader Selection**: When the message completes the ring and returns to the initiator, the node with the highest ID becomes the leader
5. **Coordinator Announcement**: The new leader sends a COORDINATOR message around the ring to announce itself

## Key Features

- **Ring Topology**: Implements a logical ring: Node 1 → 2 → 3 → 4 → 5 → 6 → 1
- **Leader Election**: Automatic leader election based on highest node ID
- **Fault Tolerance**: Periodic alive messages and timeout-based re-election
- **Message Types**:
  - `ELECTION`: Initiates leader election with candidate ID
  - `COORDINATOR`: Announces the new leader
  - `ALIVE`: Periodic heartbeat from the leader

## Files

- `ring-node.c`: Main implementation of the ring leader election algorithm
- `Makefile`: Build configuration using nullnet for message passing
- `project-conf.h`: Project-specific configuration (disables IPv6, enables nullnet)
- `ring-cooja.csc`: Cooja simulation configuration with 6 nodes in ring formation
- `README.md`: This documentation file

## Ring Topology

The logical ring topology is defined as:
```
Node 1 → Node 2 → Node 3 → Node 4 → Node 5 → Node 6 → Node 1
```

Physical positions in Cooja simulation:
```
Node 1 (30,30)  → Node 2 (80,30)  → Node 3 (130,30)
     ↑                                        ↓
Node 6 (30,80)  ← Node 5 (80,80)  ← Node 4 (130,80)
```

## Building and Running

### Prerequisites

1. Contiki-NG development environment
2. Cooja simulator
3. Java runtime for Cooja

### Build Steps

1. **Navigate to the ring directory**:
   ```bash
   cd examples/ring
   ```

2. **Compile for Cooja**:
   ```bash
   make TARGET=cooja
   ```

3. **Start Cooja**:
   ```bash
   ../../tools/cooja/gradlew run --project-dir ../../tools/cooja
   ```

4. **Load the simulation**:
   - In Cooja, go to File → Open Simulation
   - Navigate to `examples/ring/ring-cooja.csc`
   - Click Open

5. **Run the simulation**:
   - Click the "Start" button in the Simulation Control window
   - Observe the log output in the LogListener window

### Alternative: Command Line Build

You can also build the project using:
```bash
make clean && make
```

## Expected Behavior

When you run the simulation, you should observe:

1. **Initialization**: All nodes start up with random delays
2. **Initial Election**: Node 6 (highest ID) initiates the first election
3. **Message Propagation**: ELECTION message travels around the ring exactly once
4. **Candidate Updates**: Nodes with higher IDs update the candidate field as the message passes
5. **Leader Selection**: When election message returns to Node 6, it becomes the leader
6. **Coordinator Announcement**: COORDINATOR message travels around the ring exactly once
7. **Periodic Alive Messages**: Leader sends ALIVE messages that travel around the ring exactly once
8. **Message Termination**: Each message stops when it returns to its initiator

### Sample Log Output

```
Ring node 6 starting (next node: 1)
I am the highest ID node, starting initial election
Sending ELECTION (initiator=6, candidate=6, seq=1) to node 1
Received ELECTION (initiator=6, candidate=6, seq=1)
Election completed - I am the leader (candidate=6)
Sending COORDINATOR (initiator=6, candidate=6, seq=1) to node 1
New coordinator announced: node 6
Coordinator announcement completed the ring
Sending ALIVE (initiator=6, candidate=6, seq=1) to node 1
Leader 6 is alive - forwarding
Alive message completed the ring
```

## Configuration Parameters

Key timing parameters (defined in `ring-node.c`):

- `ELECTION_TIMEOUT`: 8 seconds - timeout for election completion
- `COORDINATOR_TIMEOUT`: 15 seconds - timeout for coordinator monitoring
- `ALIVE_INTERVAL`: 10 seconds - interval for leader alive messages
- `RANDOM_DELAY_MAX`: 3 seconds - maximum random startup delay

## Simulation Features

The Cooja simulation includes:

- **Network Visualizer**: Shows node positions and radio communication
- **Log Listener**: Displays real-time log messages from all nodes
- **Timeline**: Shows radio activity and LED status over time
- **Simulation Control**: Start/pause/stop simulation controls

## Testing Scenarios

### Basic Leader Election
1. Start the simulation
2. Observe initial election process
3. Verify Node 6 becomes the leader

### Leader Failure Simulation
1. Right-click on Node 6 in the visualizer
2. Select "Remove mote" or use the mote control to stop it
3. Observe other nodes detecting the failure and starting a new election
4. Verify Node 5 becomes the new leader

### Network Partitioning
1. Reduce the transmission range to create network partitions
2. Observe how different partitions elect their own leaders
3. Restore connectivity and observe re-election

## Algorithm Properties

- **Time Complexity**: O(n) where n is the number of nodes in the ring
- **Message Complexity**: O(n) messages per election
- **Fault Tolerance**: Handles single node failures through timeout mechanisms
- **Assumption**: Nodes have unique IDs and the ring topology is known

## Algorithm Improvements (Version 2.0)

This implementation has been improved to fix several critical issues:

### Fixed Issues
1. **Proper Ring Communication**: Now uses unicast to specific next node instead of broadcast
2. **Message Termination**: Messages correctly terminate when they return to the initiator
3. **Simplified Logic**: Removed complex hop counting that caused message storms
4. **Correct State Management**: Proper election state handling and leader recognition
5. **Efficient Message Flow**: Each message travels exactly once around the ring

### Remaining Limitations

1. **Static Topology**: Ring topology is hardcoded and doesn't adapt to node failures
2. **No Ring Repair**: Algorithm doesn't handle ring breaks due to node failures  
3. **Fixed Ring Size**: Currently configured for exactly 6 nodes
4. **Assumes Reliable Links**: No handling of message loss or corruption

## Extensions

Possible improvements for the algorithm:

1. **Dynamic Ring Formation**: Automatically discover and maintain ring topology
2. **Ring Repair**: Handle node failures by bypassing failed nodes
3. **Multiple Rings**: Support for multiple concurrent rings
4. **Priority-based Election**: Use node priorities instead of just IDs

## Troubleshooting

### Common Issues

1. **Compilation Errors**: Ensure Contiki-NG environment is properly set up
2. **Cooja Won't Start**: Check Java installation and JAVA_HOME environment variable
3. **No Log Output**: Verify LogListener plugin is enabled in the simulation
4. **Nodes Not Communicating**: Check radio range settings in the simulation

### Debug Tips

1. Enable more verbose logging by changing `LOG_LEVEL` to `LOG_LEVEL_DBG`
2. Use the Timeline plugin to visualize radio communication patterns
3. Check node positions and radio ranges in the Network Visualizer
4. Monitor message flow using the LogListener with timestamp formatting

## References

- Contiki-NG Documentation: https://github.com/contiki-ng/contiki-ng/wiki
- Cooja Simulator Guide: https://github.com/contiki-ng/contiki-ng/wiki/Tutorial:-Running-Contiki-NG-in-Cooja
- Ring Leader Election Algorithm: Distributed Systems textbooks and research papers
