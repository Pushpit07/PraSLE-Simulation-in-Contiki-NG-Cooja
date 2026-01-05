# PraSLE (Practical Self-Stabilizing Leader Election) Algorithm

PraSLE is a **self-stabilizing** leader election algorithm designed for resource-constrained IoT devices operating on **unreliable networks**. It guarantees eventual convergence to a unique leader from any arbitrary initial state, providing fault tolerance through continuous operation.

## Paper Reference

Based on "**A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices**" by Michael Conard and Ali Ebnenasir (2021 17th European Dependable Computing Conference - EDCC).

Key paper contributions:
- Round-based asynchronous algorithm
- Works on unreliable UDP/IP networks
- Evaluated on ring, line, tree, mesh, and clique topologies (up to 80 nodes)
- Self-stabilizing: recovers from transient faults with probability 1
- Tunable parameters (K, T) for balancing convergence time vs. robustness

## Key Parameters (Paper Section II)

PraSLE is described in the paper as *"a practical, self-stabilizing and tunable version of the MinFind algorithm that takes two parameters: maximum network latency and a diameter-proportional value."*

| Paper Term | Implementation | Description |
|------------|----------------|-------------|
| **T** (Maximum Network Latency) | `PRASLE_T_SECONDS` | The maximum time (in seconds) a node waits for messages from neighbors in each round. This is the round duration. Default: `1.0` seconds. |
| **K** (Diameter-Proportional Value) | `PRASLE_K_ROUNDS` | The number of rounds the algorithm executes. Should be at least equal to the network diameter (DLE) for guaranteed convergence. Default: `10` rounds. |

### Parameter Tuning Guidelines

From the paper (Section II):
- **K** should be set to at least the network diameter
- **T** should be set to the maximum expected network latency
- **Convergence time** = K × T seconds (worst case)

| Topology | Network Diameter | Recommended K |
|----------|------------------|---------------|
| Clique (fully connected) | 1 | K ≥ 1 |
| Ring | N/2 | K ≥ N/2 |
| Line | N-1 | K ≥ N-1 |
| Mesh (square grid) | 2×(√N - 1) | K ≥ 2×(√N - 1) |
| Tree | 2×height | K ≥ 2×log(N) |

### Configuration Examples

```c
// In prasle-config.h:

// Fast convergence for clique (diameter=1)
#define PRASLE_K_ROUNDS 2
#define PRASLE_T_SECONDS 0.5

// Ring with 10 nodes (diameter=5)
#define PRASLE_K_ROUNDS 5
#define PRASLE_T_SECONDS 1.0

// Line with 10 nodes (diameter=9)
#define PRASLE_K_ROUNDS 9
#define PRASLE_T_SECONDS 1.0
```

Or via Makefile:
```bash
make ALGORITHM=prasle TARGET=cooja PRASLE_K_ROUNDS=5 PRASLE_T_SECONDS=0.5
```

## Algorithm Overview (Paper Algorithm 1, Figure 1)

### Pseudocode

```
process pi;
var round := K + 1;                                           // Line 2
    neighborsi := getListOfNeighbors();                       // Line 3
    mini := N + 1;                                            // Line 4 (null value)
    temp_mini := getRankingValue();                           // Line 5
    leaderi := IDi;                                           // Line 6
    temp_leaderi := IDi;                                      // Line 7
    T := 1.0;                                                 // Line 8 (tunable)
begin until False                                             // Line 9
    Timer recvTimer := T;                                     // Line 10
    while recvTimer > 0 and round < K + 1 {                   // Line 11
        :: recv (minj, leaderj) from pj in neighborsi {       // Line 12
            if (minj, leaderj) < (temp_mini, temp_leaderi) {  // Line 13
                temp_mini = minj;                             // Line 14
                temp_leaderi = leaderj;                       // Line 15
            }
        }
    }
    round--;                                                  // Line 19
    if (temp_mini, temp_leaderi) < (mini, leaderi) {          // Line 20
        mini = temp_mini;                                     // Line 21
        leaderi = temp_leaderi;                               // Line 22
        for pj in neighborsi {                                // Line 23
          send(pj, mini, leaderi)                             // Line 24
        }                                                     // Line 25
    }                                                         // Line 26
    else if round <= 0 → return leaderi                       // Line 27
end                                                           // Line 28

// UNRELIABLE NETWORK MODIFICATION (Paper Section III):
// Lines 23-25 moved OUTSIDE the if statement of Line 20
// Line 27 removed (never terminates)
```

### Key Algorithm Insights

1. **First Round Behavior**: When `round == K+1`, the while loop condition `round < K+1` is FALSE. Nodes skip the receive phase and immediately broadcast their initial values. This bootstraps the algorithm.

2. **Lexicographic Comparison**: `(m1, l1) < (m2, l2)` iff `(m1 < m2) OR ((m1 == m2) AND (l1 < l2))`. Lower values win.

3. **Three Tasks Per Round** (Paper page 4):
   - **Task 1**: Listen for T seconds to receive messages from neighbors
   - **Task 2**: Compare received values with local values, update if better
   - **Task 3**: Disseminate local values to neighbors

4. **Unreliable Network Mode**: For robustness against message loss, nodes broadcast EVERY round (not just when values change), and the algorithm runs continuously (never terminates).

## Algorithm Flow

### Reliable Network Mode (PRASLE_UNRELIABLE_MODE=0)

```
INITIALIZE:
  round = K + 1
  mini = N + 1  (null value)
  temp_mini = my_node_id
  leaderi = temp_leaderi = my_node_id

LOOP:
  IF round < K + 1:
    Wait T seconds, receive messages
    Update temp values if better pair received
  ELSE:
    Skip receive (first round bootstrap)

  round--

  IF (temp_mini, temp_leaderi) < (mini, leaderi):
    Update mini, leaderi
    Broadcast (mini, leaderi) to neighbors

  IF round <= 0:
    TERMINATE (election complete)
```

### Unreliable Network Mode (PRASLE_UNRELIABLE_MODE=1) - DEFAULT

```
INITIALIZE:
  round = K + 1
  mini = N + 1  (null value)
  temp_mini = my_node_id
  leaderi = temp_leaderi = my_node_id

LOOP FOREVER:
  IF round < K + 1:
    Wait T seconds, receive messages
    Update temp values if better pair received
  ELSE:
    Skip receive (first round bootstrap)

  round--

  IF (temp_mini, temp_leaderi) < (mini, leaderi):
    Update mini, leaderi

  ALWAYS: Broadcast (mini, leaderi) to neighbors  // MOVED OUTSIDE if

  IF round <= 0:
    Log convergence
    Reset round = K + 1  // START NEW CYCLE
    CONTINUE (never terminate)
```

## Message Structure

```c
typedef struct {
  uint16_t min_value;    // Ranking value (mini)
  uint16_t leader_id;    // Leader ID (leaderi)
  uint16_t sender_id;    // ID of sending node
} prasle_msg_t;  // Total: 6 bytes
```

## Network Stack

This implementation uses **IPv6/UDP** as specified in the paper:

- **Network Layer**: IPv6 with RPL-Lite routing
- **Transport Layer**: Simple UDP (via `simple-udp.h`)
- **MAC Layer**: CSMA (Carrier Sense Multiple Access)
- **Communication**: IPv6 multicast (ff02::1) with neighbor filtering

### Why IPv6/UDP?

The paper explicitly states (Page 2): *"we evaluate PraSLE using the unicast primitive on a **UDP/IP network**"*

Using IPv6/UDP provides:
- Consistency with the paper's specification
- Fair comparison with Bully and Ring algorithms (which also use IPv6/UDP)
- Proper simulation of real IoT network conditions

### Neighbor Filtering

Since IPv6 multicast (ff02::1) reaches ALL nodes in the network, we implement **application-level neighbor filtering**:

1. Each node maintains a list of logical neighbors based on the topology
2. When receiving a message, the node checks if the sender is a logical neighbor
3. Messages from non-neighbors are silently discarded

This ensures PraSLE operates correctly on logical topologies (ring, line, mesh, clique) even though the underlying network is fully connected

## Algorithm Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `PRASLE_K_ROUNDS` | 10 | Number of rounds (should be >= network diameter) |
| `PRASLE_T_SECONDS` | 1.0 | Maximum network latency / round duration (seconds) |
| `PRASLE_UNRELIABLE_MODE` | 1 | 0=reliable (terminates), 1=unreliable (continuous) |
| `MAX_NEIGHBORS` | 8 | Maximum neighbors per node |
| `N_MAX` | 100 | Maximum number of nodes in network |
| `NETWORK_SIZE` | 10 | Actual number of nodes |
| `NETWORK_TOPOLOGY` | CLIQUE | Topology type (see below) |

### Recommended K Values by Topology

| Topology | Network Diameter | Recommended K |
|----------|------------------|---------------|
| Clique | 1 | K >= 1 |
| Ring | N/2 | K >= N/2 |
| Line | N-1 | K >= N-1 |
| Mesh (square) | 2*(sqrt(N)-1) | K >= 2*(sqrt(N)-1) |
| Tree | 2*log(N) | K >= 2*log(N) |

### Convergence Time

**Worst-case convergence time**: K * T seconds

Examples:
- Clique (10 nodes): K=1, T=1.0s → 1 second
- Ring (10 nodes): K=5, T=1.0s → 5 seconds
- Line (10 nodes): K=9, T=1.0s → 9 seconds

## Supported Topologies

| Topology | Code | Description | Neighbors |
|----------|------|-------------|-----------|
| RING | 1 | Circular ring | 2 (left, right) |
| LINE | 2 | Linear chain | 1-2 (ends have 1) |
| MESH | 3 | 2D grid | 2-4 (corners/edges/interior) |
| CLIQUE | 4 | Fully connected | N-1 (all nodes) |

Configure in `prasle-config.h`:
```c
#define NETWORK_TOPOLOGY TOPOLOGY_CLIQUE
#define NETWORK_SIZE 10
```

### Topology Diagrams

**Ring (N=5)**:
```
    1 ─── 2
   /       \
  5         3
   \       /
    ─ 4 ──
```

**Line (N=5)**:
```
1 ─── 2 ─── 3 ─── 4 ─── 5
```

**Mesh (3x3)**:
```
1 ─── 2 ─── 3
│     │     │
4 ─── 5 ─── 6
│     │     │
7 ─── 8 ─── 9
```

**Clique (N=4)**:
```
    1 ─── 2
    │ ╲ ╱ │
    │ ╱ ╲ │
    4 ─── 3
```

## Self-Stabilization Properties

### Definition

A self-stabilizing algorithm eventually reaches a legitimate state from **any** arbitrary initial state, without external intervention.

### PraSLE Guarantees (Paper Theorem III.1)

1. **Convergence (Reliable)**: In reliable networks, PraSLE eventually elects a leader
2. **Convergence (Unreliable)**: In unreliable networks, PraSLE elects a leader with probability 1
3. **Closure**: Once converged, the system remains in a legitimate state
4. **Time Complexity**: O(D) where D is network diameter (Theorem III.2)
5. **Message Complexity**: O(D*Δ*N) where Δ is max node degree (Theorem III.3)

### Why Self-Stabilization Matters for IoT

- **No careful initialization required**: Works from any starting state
- **Automatic recovery**: Handles soft errors, memory corruption, bit flips
- **Resilient to restarts**: Nodes can crash and rejoin seamlessly
- **Dynamic networks**: Adapts to nodes joining/leaving
- **Message loss tolerance**: Unreliable mode handles packet drops

## Ranking Function

The ranking value determines election priority (lower wins):

```c
static uint16_t get_ranking_value(void) {
  // Current implementation: use node ID
  return my_node_id;  // Lowest ID wins

  // Alternative implementations (from paper):
  // return battery_level();        // Prefer high-battery nodes
  // return 100 - compute_power();  // Prefer low-compute nodes
  // return random_value();         // Randomized leader selection
}
```

## Operation Modes

### Unreliable Network Mode (Default)

```c
#define PRASLE_UNRELIABLE_MODE 1
```

- **Broadcasts every round** regardless of value changes
- **Never terminates** - runs continuously
- **Self-stabilizing**: recovers from faults automatically
- **Recommended** for real IoT deployments

### Reliable Network Mode

```c
#define PRASLE_UNRELIABLE_MODE 0
```

- **Only broadcasts when values change** (optimization)
- **Terminates after K rounds** when converged
- **Assumes no message loss**
- Use for controlled environments or simulation testing

## Building

```bash
# Build PraSLE algorithm
make ALGORITHM=prasle TARGET=cooja

# Build with custom parameters
make ALGORITHM=prasle TARGET=cooja PRASLE_K_ROUNDS=5 PRASLE_T_SECONDS=0.5

# Build with specific network size
make ALGORITHM=prasle TARGET=cooja PRASLE_NETWORK_SIZE=50

# Build with metrics enabled
make ALGORITHM=prasle TARGET=cooja ENABLE_METRICS=1

# Clean and rebuild
make ALGORITHM=prasle TARGET=cooja clean
make ALGORITHM=prasle TARGET=cooja
```

## Configuration

### Via prasle-config.h

```c
#define PRASLE_K_ROUNDS 10
#define PRASLE_T_SECONDS 1.0
#define PRASLE_NETWORK_SIZE 50
#define PRASLE_UNRELIABLE_MODE 1
#define NETWORK_TOPOLOGY TOPOLOGY_MESH
```

## Testing Scenarios

### 1. Normal Convergence

1. Start all nodes simultaneously
2. Observe first round bootstrap (all nodes broadcast initial values)
3. Watch round-by-round value propagation
4. Verify all nodes converge to same leader (lowest ID)
5. Expected time: K * T seconds

### 2. Self-Stabilization Test

1. Initialize nodes with arbitrary/corrupted states
2. Let algorithm run
3. Verify convergence to correct leader within K rounds
4. This demonstrates fault recovery

### 3. Transient Fault Injection

1. Wait for initial convergence
2. Corrupt a node's `(mini, leaderi)` values
3. Observe automatic recovery within K rounds
4. Leader remains unchanged (fault was transient)

### 4. Leader Failure

1. Wait for convergence (node 1 is leader)
2. Stop node 1
3. Observe re-convergence to new leader (node 2)
4. Verify all surviving nodes agree on new leader

### 5. Network Partition

1. Start with converged network
2. Create partition (two isolated groups)
3. Each partition converges to local leader
4. Heal partition
5. Observe re-convergence to global leader

### 6. Message Loss Test

1. Enable unreliable mode
2. Introduce packet loss (50%, 70%, 90%)
3. Verify eventual convergence despite losses
4. May require more election cycles

## Experimental Results (Paper Section IV)

The paper reports results on IoT-Lab testbed with Cortex-M3 devices:

### Convergence Time (seconds)

| Nodes | Ring | Line | Mesh | Clique |
|-------|------|------|------|--------|
| 10 | 0.66 | 0.69 | 0.66 | 0.57 |
| 20 | 1.19 | 1.28 | 0.89 | 0.85 |
| 30 | 1.88 | 2.40 | 0.99 | 0.99 |
| 40 | 2.59 | 2.95 | 1.14 | 1.17 |

### Key Findings

- Convergence time grows slowly (sub-linear for non-linear topologies)
- Mesh topology is most efficient (higher connectivity, lower diameter)
- Message complexity grows linearly except for clique (quadratic)
- Energy consumption is minimal (< 0.1J for most configurations)

## Algorithm Comparison

| Property | PraSLE | Bully | Ring |
|----------|--------|-------|------|
| Self-stabilizing | **Yes** | No | No |
| Message complexity | O(K*N) | O(N²) | O(N) |
| Time complexity | O(K*T) | O(timeout) | O(N) |
| Topology required | Neighbors | None | Ring |
| Handles arbitrary faults | **Yes** | Limited | No |
| Continuous operation | **Yes** | No | No |
| Network type | Unreliable UDP | Any | Any |

## File Structure

```
algorithms/prasle/
├── prasle-node.c       # Main algorithm implementation
├── prasle-config.h     # Configuration and message types
└── README.md           # This documentation
```

## Key Implementation Details

### Variable Initialization (Paper Lines 2-8)

```c
round_counter = PRASLE_K_ROUNDS + 1;  // Start at K+1
mini = N_MAX + 1;                      // Null value (above valid range)
temp_mini = my_node_id;                // Ranking value
leaderi = my_node_id;                  // Initial leader claim
temp_leaderi = my_node_id;             // Initial leader claim
```

### First Round Bootstrap

```c
if (round_counter < PRASLE_K_ROUNDS + 1) {
  // Wait T seconds for messages
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&recv_timer));
} else {
  // First round: skip receive, go straight to broadcast
  LOG_INFO("FIRST ROUND - skipping receive\n");
}
```

### Lexicographic Comparison

```c
static bool is_better(uint16_t m1, uint16_t l1, uint16_t m2, uint16_t l2) {
  return (m1 < m2) || ((m1 == m2) && (l1 < l2));
}
```

### Continuous Operation (Unreliable Mode)

```c
if (round_counter <= 0) {
  if (!election_converged) {
    election_converged = true;
    LOG_INFO("CONVERGED: Leader = %u\n", leaderi);
  }
  // Reset for next cycle (never terminate)
  election_cycle++;
  round_counter = PRASLE_K_ROUNDS + 1;
}
```

## Troubleshooting

### Nodes Don't Converge

1. **Check K value**: K should be >= network diameter
2. **Check topology**: Ensure neighbors are correctly initialized
3. **Check T value**: T should be > maximum message delay
4. **Verify UDP**: IPv6/UDP must be properly configured

### Convergence Is Slow

1. **Reduce T**: Faster rounds = faster convergence
2. **Optimize K**: Use minimum K for your topology
3. **Use clique**: Clique has diameter 1, fastest convergence

### High Message Overhead

1. **Use reliable mode**: Only sends on value changes
2. **Reduce K**: Fewer rounds after convergence
3. **Avoid clique**: Clique has O(N²) message complexity

## References

- Conard, M. & Ebnenasir, A. (2021). "A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices". EDCC 2021.
- Dolev, S. (2000). "Self-Stabilization". MIT Press.
- Dijkstra, E.W. (1974). "Self-stabilizing systems in spite of distributed control". Communications of the ACM.
- Implementation source code: https://github.com/maconard/cps-iot_leader-election
