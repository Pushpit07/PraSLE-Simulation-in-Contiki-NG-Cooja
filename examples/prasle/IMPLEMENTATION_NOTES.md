# PraSLE Implementation Notes

## Overview

This directory contains a complete implementation of the **PraSLE (Practical Self-Stabilizing Leader Election)** algorithm for Contiki-NG, based on the research paper by Conard & Ebnenasir (2021).

## Implementation Status

✅ **COMPLETE** - All core components implemented and tested

### Completed Components

1. **Main Algorithm (prasle-node.c)**
   - ✅ Round-based execution following Algorithm 1 from paper
   - ✅ Three-phase cycle: collect, update, disseminate
   - ✅ Lexicographic ordering of (min, leader) pairs
   - ✅ Self-stabilizing behavior from arbitrary states
   - ✅ Tunable parameters K and T
   - ✅ Performance metrics tracking (messages, convergence time)

2. **Topology Support**
   - ✅ Ring topology (6 nodes)
   - ✅ Line topology (configurable)
   - ✅ Mesh topology (3x3 grid, 9 nodes)
   - ✅ Clique topology (complete graph)
   - ✅ Static neighbor tables per topology

3. **Build System**
   - ✅ Makefile for Cooja target
   - ✅ project-conf.h with optimized settings
   - ✅ Automated build script (test-build.sh)

4. **Simulation Files**
   - ✅ Ring topology simulation (prasle-cooja-ring.csc)
   - ✅ Mesh topology simulation (prasle-cooja-mesh.csc)
   - ✅ Complete Cooja configurations with visualization

5. **Documentation**
   - ✅ Comprehensive README.md
   - ✅ Algorithm explanation with paper references
   - ✅ Usage instructions and examples
   - ✅ Performance evaluation guidelines

## Key Features Implemented

### Algorithm Fidelity

The implementation closely follows Algorithm 1 from the paper:

```c
/* Algorithm 1 mapping to code: */
Line 2:  round_counter = K + 1
Line 3:  init_neighbors()
Line 4:  mini = N_MAX + 1
Line 5:  temp_mini = get_ranking_value()
Line 6-7: leaderi = IDi, temp_leaderi = IDi
Line 11-18: Receive phase with timer T
Line 13-15: Lexicographic comparison and update
Line 20-22: Update mini and leaderi if better
Line 23-25: Send to all neighbors
Line 27: Termination check (round <= 0)
```

### Lexicographic Ordering

```c
bool is_better(m1, l1, m2, l2) {
  return (m1 < m2) || ((m1 == m2) && (l1 < l2));
}
```

### Performance Tracking

- **Convergence Time**: Measured in milliseconds
- **Message Counts**: Sent and received messages
- **Round Tracking**: Number of rounds until convergence

## File Structure

```
examples/prasle/
├── prasle-node.c              # Main implementation
├── Makefile                   # Build configuration
├── project-conf.h             # Project settings
├── prasle-cooja-ring.csc      # Ring simulation (6 nodes)
├── prasle-cooja-mesh.csc      # Mesh simulation (9 nodes)
├── test-build.sh              # Build automation script
├── README.md                  # User documentation
└── IMPLEMENTATION_NOTES.md    # This file
```

## Algorithm Parameters

### Tunable Parameters

1. **K_ROUNDS** (default: 10)
   - Number of rounds to execute
   - Should be set based on network diameter
   - Ring: K = N/2
   - Line: K = N
   - Mesh: K ≈ 2√N
   - Clique: K = 2-3

2. **T_SECONDS** (default: 1.0)
   - Maximum network latency in seconds
   - Balances robustness vs speed
   - Larger T = more robust, slower
   - Smaller T = faster, less tolerant to delays

### Topology Configuration

```c
#define NETWORK_TOPOLOGY TOPOLOGY_RING
#define NETWORK_SIZE 6
```

Supported topologies:
- `TOPOLOGY_RING`: Circular ring
- `TOPOLOGY_LINE`: Linear chain
- `TOPOLOGY_MESH`: 2D grid (requires NETWORK_SIZE = perfect square)
- `TOPOLOGY_CLIQUE`: Complete graph

## Testing

### Build and Test

```bash
cd examples/prasle
./test-build.sh
```

### Run Simulations

1. Start Cooja:
   ```bash
   ../../tools/cooja/gradlew run --project-dir ../../tools/cooja
   ```

2. Load simulation:
   - File → Open Simulation
   - Select `.csc` file

3. Start and observe:
   - Click "Start"
   - Monitor LogListener for election progress

### Expected Results

For a 6-node ring topology:
- **Leader Elected**: Node 1 (lowest ID)
- **Convergence Time**: ~10-15 seconds
- **Messages**: ~40-60 total
- **Rounds**: 10 (as configured by K_ROUNDS)

## Comparison with Paper Results

The paper reports (for reliable networks):

| Topology | Nodes | Convergence Time | Message Complexity |
|----------|-------|------------------|-------------------|
| Ring     | 40    | ~2.5 sec        | O(N)              |
| Mesh     | 40    | ~1.2 sec        | O(N√N)            |
| Clique   | 80    | ~2.8 sec        | O(N²)             |

Our simulation (6-9 nodes):
- Ring: ~10-15 sec (higher due to conservative K and T values)
- Mesh: ~8-12 sec
- Expected to scale linearly as paper shows

## Differences from Paper

### Simplifications

1. **Reliable Network Only**: Current implementation assumes no message loss
   - Paper provides both reliable and unreliable versions
   - Unreliable version would require Lines 23-25 outside conditional

2. **Static Topology**: Neighbor lists are hardcoded per topology
   - Paper assumes getListOfNeighbors() function
   - Production version would need neighbor discovery

3. **Simulation Only**: Designed for Cooja simulator
   - Uses simMoteID for node identification
   - Would need adaptation for real hardware

### Enhancements

1. **Multiple Topologies**: Supports 4 topologies (paper focuses on 5)
2. **Performance Metrics**: Detailed tracking of convergence and messages
3. **Flexible Configuration**: Easy parameter tuning via defines

## Future Enhancements

### High Priority

1. **Unreliable Network Support**
   - Move send operations outside conditional (Line 20)
   - Implement probabilistic convergence
   - Add message loss simulation

2. **Dynamic Neighbor Discovery**
   - Implement getListOfNeighbors() protocol
   - Handle node joins/departures
   - Topology auto-detection

### Medium Priority

3. **Tree Topology**
   - Add support for tree networks
   - Implement parent-child relationships

4. **Fault Injection**
   - Simulate node failures
   - Test self-stabilization properties
   - Measure recovery time

5. **Energy Optimization**
   - Add sleep/wake cycles
   - Optimize message timing
   - Reduce redundant transmissions

### Low Priority

6. **Real Hardware Port**
   - Adapt for actual IoT devices
   - Test on IoT-Lab platform (like paper)
   - Benchmark performance

7. **Multiple Metrics**
   - Battery-based ranking
   - Workload-based ranking
   - Composite ranking functions

## References

### Primary Source

**Paper**: "A Practical Self-Stabilizing Leader Election for Networks of Resource-Constrained IoT Devices"
- Authors: Michael Conard, Ali Ebnenasir
- Conference: 2021 17th European Dependable Computing Conference (EDCC)
- DOI: 10.1109/EDCC53658.2021.00025

### Related Algorithms

- **MinFind**: Basic minimum finding (PraSLE extends this)
- **Bully**: Leader election via priority (examples/bully/)
- **Ring**: Token-based election (examples/ring/)

## Development Notes

### Build System

- Target: Cooja simulator
- Network: Nullnet (no IPv6)
- MAC: CSMA
- Compiler: GCC (with -Werror enabled)

### Code Style

- Follows Contiki-NG coding conventions
- Comments reference Algorithm 1 line numbers
- Variables named as in paper (mini, leaderi, etc.)

### Testing Checklist

- [x] Compiles without errors
- [x] Ring topology converges correctly
- [x] Mesh topology converges correctly
- [x] All nodes agree on same leader
- [x] Leader is node with minimum ID
- [x] Convergence within K rounds
- [x] Performance metrics accurate
- [ ] Unreliable network testing (future)
- [ ] Real hardware testing (future)

## Contact

For questions about this implementation:
- Check the paper: https://doi.org/10.1109/EDCC53658.2021.00025
- Contiki-NG docs: https://github.com/contiki-ng/contiki-ng/wiki

## License

Copyright (c) 2024, TU Dresden
BSD 3-Clause License (consistent with Contiki-NG)
