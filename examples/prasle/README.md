# PraSLE: Practical Self-Stabilizing Leader Election for Contiki-NG

This example implements the **PraSLE (Practical Self-Stabilizing Leader Election)** algorithm in Contiki-NG using the Cooja simulator. PraSLE is based on the research paper:

> **"A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices"**
> Michael Conard and Ali Ebnenasir
> 2021 17th European Dependable Computing Conference (EDCC)

## Algorithm Overview

PraSLE is an extended version of the minimum finding algorithm that functions in a **round-based asynchronous fashion**. It is designed specifically for resource-constrained IoT devices and provides:

- **Self-stabilization**: Recovers from arbitrary initial states
- **Tunability**: Parameters K and T can be adjusted for different topologies
- **Multiple topology support**: Ring, line, mesh, clique, and tree
- **Fault tolerance**: Handles transient faults and message loss (in unreliable networks)

### Core Algorithm Mechanism

Based on Algorithm 1 from the paper, each node executes the following three-phase cycle per round:

1. **Collect Phase (Lines 11-18)**: Listen for T seconds and receive `(min, leader)` pairs from neighbors
2. **Update Phase (Lines 20-22)**: Compare received values with local values and update if better
3. **Disseminate Phase (Lines 23-25)**: Send updated `(min, leader)` pair to all neighbors

**Convergence**: The algorithm runs for at least K rounds (where K is typically related to network diameter), and elects the node with the minimum ranking value as leader.

### Key Properties

- **Time Complexity**: O(D) where D is network diameter
- **Message Complexity**: O(DΔN) where Δ is max degree, N is number of nodes
- **Self-Stabilizing**: Converges to unique leader from any arbitrary initial state
- **Tunable Parameters**:
  - **K**: Number of rounds (typically network diameter)
  - **T**: Maximum network latency in seconds

## Files

- **prasle-node.c**: Main implementation of PraSLE algorithm
- **Makefile**: Build configuration using nullnet
- **project-conf.h**: Project-specific configuration
- **prasle-cooja-ring.csc**: Cooja simulation for ring topology (6 nodes)
- **README.md**: This documentation file

## Algorithm Details

### Variables (per node i)

- `mini`: Current minimum value known to node i
- `temp_mini`: Temporary minimum value for current round
- `leaderi`: ID of the leader node i believes is elected
- `temp_leaderi`: Temporary leader ID for current round
- `neighbors`: List of neighbor nodes
- `round`: Current round counter

### Lexicographic Ordering

Values are compared using lexicographic ordering: `(m1, l1) < (m2, l2)` if and only if:
- `m1 < m2` OR
- `(m1 == m2) AND `l1 < l2`

### Initialization

```
round := K + 1
mini := N + 1  (N is max number of nodes)
temp_mini := getRankingValue()  (for simulation: node ID)
leaderi := IDi
temp_leaderi := IDi
```

## Supported Topologies

The implementation supports multiple network topologies (configurable via `NETWORK_TOPOLOGY` define):

### 1. Ring Topology (TOPOLOGY_RING)
```
Node 1 → Node 2 → Node 3 → Node 4 → Node 5 → Node 6 → Node 1
```
- Each node connects to its successor and predecessor
- Diameter: N/2

### 2. Line Topology (TOPOLOGY_LINE)
```
Node 1 — Node 2 — Node 3 — Node 4 — Node 5 — Node 6
```
- Each node connects to neighbors on left and right
- Diameter: N-1

### 3. Mesh Topology (TOPOLOGY_MESH)
```
1 — 2 — 3
|   |   |
4 — 5 — 6
|   |   |
7 — 8 — 9
```
- 2D grid topology (sqrt(N) × sqrt(N))
- Each node connects to up to 4 neighbors (up, down, left, right)
- Diameter: ~2√N

### 4. Clique Topology (TOPOLOGY_CLIQUE)
```
All nodes connected to all other nodes (complete graph)
```
- Each node connects to N-1 other nodes
- Diameter: 1

## Configuration Parameters

Key parameters in `prasle-node.c`:

```c
#define K_ROUNDS 10              // Number of rounds (K parameter)
#define T_SECONDS 1.0            // Network latency in seconds (T parameter)
#define NETWORK_TOPOLOGY TOPOLOGY_RING  // Topology selection
#define NETWORK_SIZE 6           // Number of nodes
#define MAX_NEIGHBORS 8          // Maximum neighbors per node
```

### Tuning Guidelines (from paper)

- **K (rounds)**: Set to network diameter for optimal performance
  - Ring: K = N/2
  - Line: K = N
  - Mesh: K = L + W - 2 (length + width)
  - Clique: K = 2 or 3

- **T (latency)**: Set to maximum expected message delay
  - Larger T = more robust, slower convergence
  - Smaller T = faster, less robust to delays

## Building and Running

### Prerequisites

1. Contiki-NG development environment
2. Cooja simulator
3. Java runtime for Cooja

### Build Steps

1. **Navigate to prasle directory**:
   ```bash
   cd examples/prasle
   ```

2. **Compile for Cooja**:
   ```bash
   make TARGET=cooja
   ```

3. **Start Cooja**:
   ```bash
   ../../tools/cooja/gradlew run --project-dir ../../tools/cooja
   ```

4. **Load simulation**:
   - In Cooja: File → Open Simulation
   - Navigate to `examples/prasle/prasle-cooja-ring.csc`
   - Click Open

5. **Run simulation**:
   - Click "Start" button
   - Observe logs in LogListener window

### Alternative: Command Line Build

```bash
make clean && make TARGET=cooja
```

## Expected Behavior

When running the simulation, you should observe:

1. **Initialization**: All nodes start with random delays
2. **Round Execution**: Each node executes K rounds
3. **Message Exchange**: Nodes broadcast `(min, leader)` pairs
4. **Value Updates**: Nodes update their values when receiving better pairs
5. **Convergence**: After K rounds, all nodes agree on the same leader
6. **Leader Election**: Node with minimum ranking value (lowest ID) becomes leader

### Sample Log Output

```
PraSLE node 1 starting
Parameters: K=10 rounds, T=1.0 seconds
Initialized 2 neighbors: 2 6
Initial values: mini=21, temp_mini=1, leaderi=1
========== Starting Round 10 ==========
Round 10: Receiving phase (1000 ms)
Round 10: Received from node 2: (min=2, leader=2)
Round 10: Updated temp values to (min=1, leader=1)
Round 10: Updated to (min=1, leader=1)
Round 10: Broadcasting (min=1, leader=1)
...
========== Election Complete ==========
Final Leader: 1 (min=1)
CONVERGED at round 0: Leader = 1 (min=1)
Convergence time: 11500 ms
Total messages sent: 20, received: 40
```

## Testing Different Topologies

To test different topologies, modify the `NETWORK_TOPOLOGY` define in `prasle-node.c`:

```c
// For ring topology
#define NETWORK_TOPOLOGY TOPOLOGY_RING
#define NETWORK_SIZE 6

// For mesh topology
#define NETWORK_TOPOLOGY TOPOLOGY_MESH
#define NETWORK_SIZE 9  // 3x3 grid

// For clique topology
#define NETWORK_TOPOLOGY TOPOLOGY_CLIQUE
#define NETWORK_SIZE 6
```

Then recompile:
```bash
make clean && make TARGET=cooja
```

## Performance Metrics

The implementation tracks:

1. **Convergence Time**: Time from start to leader election completion
2. **Message Count**: Total messages sent and received
3. **Round Count**: Number of rounds until convergence

These metrics help evaluate the algorithm's efficiency for different topologies.

## Comparison with Other Algorithms

| Algorithm | Time Complexity | Message Complexity | Self-Stabilizing | Topology Support |
|-----------|----------------|-------------------|------------------|------------------|
| **PraSLE** | O(D) | O(DΔN) | Yes | Ring, Line, Mesh, Clique |
| **Bully** | O(N²) | O(N²) | No | Clique |
| **Ring** | O(N) | O(N) | No | Ring only |
| **MinFind** | O(D) | O(N·D) | No | Any |

## Advantages of PraSLE

1. **Self-Stabilization**: Recovers from arbitrary initial states and transient faults
2. **Tunability**: K and T parameters allow optimization for specific platforms
3. **Multiple Topologies**: Works efficiently on ring, line, mesh, and clique
4. **Resource Efficient**: Sub-linear convergence time for non-linear topologies
5. **Symmetric**: All nodes run identical code
6. **Asynchronous**: No global synchronization required

## Limitations

1. **Static Topology**: Assumes fixed network topology (no dynamic neighbor discovery)
2. **Reliable Links**: Current implementation assumes reliable message delivery
3. **Fixed Parameters**: K and T must be configured before deployment
4. **No Fault Detection**: Does not explicitly detect/handle permanent node failures

## Extensions and Future Work

Possible improvements:

1. **Unreliable Network Support**: Implement probabilistic convergence for UDP/lossy networks
2. **Dynamic Topology**: Add neighbor discovery and topology adaptation
3. **Fault Detection**: Implement leader failure detection and re-election
4. **Energy Optimization**: Add sleep/wake cycles for energy efficiency
5. **Multi-Leader**: Extend to elect top-k leaders instead of single leader

## Research Paper Details

This implementation closely follows Algorithm 1 from the paper:

```
Algorithm 1: Practical Self-Stabilizing Leader Election
process pi;
var round := K + 1;
    neighborsi := getListOfNeighbors();
    mini := N + 1;
    temp_mini := getRankingValue();
    leaderi := IDi;
    temp_leaderi := IDi;
    T := 1.0;  // tunable value

begin until False
    Timer recvTimer := T;
    while recvTimer > 0 and round < K + 1 {
        recv (minj, leaderj) from pj in neighborsi {
            if (minj, leaderj) < (temp_mini, temp_leaderi) {
                temp_mini = minj;
                temp_leaderi = leaderj;
            }
        }
    }
    round--;
    if (temp_mini, temp_leaderi) < (mini, leaderi) {
        mini = temp_mini;
        leaderi = temp_leaderi;
        for pj in neighborsi {
            send(pj, mini, leaderi)
        }
    }
    else if round <= 0 → return leaderi
end
```

## Troubleshooting

### Common Issues

1. **Build Errors**: Ensure Contiki-NG is properly configured
2. **Cooja Won't Start**: Check Java installation
3. **No Convergence**: Verify K and T parameters are appropriate for topology
4. **Missing Logs**: Enable LogListener plugin in Cooja

### Debug Tips

1. Change `LOG_LEVEL` to `LOG_LEVEL_DBG` for verbose output
2. Use Timeline plugin to visualize message exchanges
3. Verify neighbor configuration matches topology
4. Check that NETWORK_SIZE matches actual number of nodes

## References

1. **Original Paper**: Conard, M., & Ebnenasir, A. (2021). "A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices." 2021 17th European Dependable Computing Conference (EDCC).

2. **Contiki-NG Documentation**: https://github.com/contiki-ng/contiki-ng/wiki

3. **Cooja Simulator**: https://github.com/contiki-ng/contiki-ng/wiki/Tutorial:-Running-Contiki-NG-in-Cooja

4. **Self-Stabilization**: Dijkstra, E. W. (1974). "Self-stabilizing systems in spite of distributed control." Communications of the ACM.

## License

Copyright (c) 2024, TU Dresden. All rights reserved.

This implementation follows the BSD 3-Clause License consistent with Contiki-NG.
