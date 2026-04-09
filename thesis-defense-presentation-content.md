# Thesis Defense Presentation
# Adaptive and Fault-Tolerant Leader Election in Resource-Constrained IoT and Robotic Clusters

---

## Slide 1: Title Slide

**Adaptive and Fault-Tolerant Leader Election in Resource-Constrained IoT and Robotic Clusters**

Pushpit Bhardwaj
Course: Distributed Systems Engineering

Referee: Dr. Ing. Thomas Springer
Supervisor: Leonard Herbst M.Sc.
Supervising Professor: Prof. Dr. Matthias Wahlisch

---

## Slide 2: Motivation

**Classical Leader Election Protocols Are Unsuitable for Resource-Constrained IoT Networks**

| IoT Reality | Classical Assumption |
|---|---|
| Lossy wireless links (10-30% packet loss) | Reliable communication channels |
| Kilobytes of RAM (Class 0/1 devices) | Sufficient computational resources |
| Battery-powered (months/years) | No energy constraints |
| Unattended operation, no manual recovery | Manual intervention after faults |

**Why Leader Election?**
- A single coordinator simplifies data aggregation, task assignment, and group decisions
- Required in sensor networks, robotic swarms, and industrial IoT clusters

**Research Gaps (Section 3.8):**
- No protocol combines energy awareness with self-stabilization. PraSLE is self-stabilizing but ignores energy; LEACH/HEED consider energy but are not self-stabilizing
- Link quality is not used in leader selection. Routing protocols use RSSI/ETX for path selection, but leader election protocols do not
- No adaptive timeouts, no graceful handover, and no backup recovery in existing self-stabilizing leader election

---

## Slide 3: Problem Statement

**Existing leader election protocols do not fully meet the needs of constrained IoT networks.**

- **Classical algorithms** (Bully, Ring) lack mechanisms to distinguish between failed nodes and temporary communication disruptions, which can trigger unnecessary re-elections [Garcia-Molina 1982, Chang & Roberts 1979].
- **Consensus protocols** (Paxos, Raft) serve a broader purpose than leader election, implementing full distributed state machines. Their overhead is disproportionate for the narrower task of electing a single leader.
- **PraSLE** [Conard & Ebnenasir, 2021], designed specifically for resource-constrained IoT networks, addresses self-stabilization but leaves the following problems unsolved:

1. **Context-blind leader selection.** PraSLE elects the node with the lowest identifier, without considering whether that node has sufficient remaining energy or reliable links to its neighbors. A node with a nearly depleted battery may become leader and fail shortly after election, triggering a complete re-election cycle.
2. **Fixed timing.** PraSLE uses a static timeout parameter that does not adapt to changing network conditions. If the timeout is too short, nodes may falsely suspect leader failure; if it is too long, actual failures go undetected for extended periods.
3. **Permanent leadership.** Once elected, a PraSLE leader serves until it fails, concentrating energy consumption on a single node while other nodes conserve their batteries. Over time, this asymmetry increases the probability of leader failure.

---

## Slide 4: State of the Art

| Approach | Description | Major Limitation |
|---|---|---|
| **Bully / Ring** | Classical leader election (Garcia-Molina, Chang-Roberts) | Not self-stabilizing; cannot recover from state corruption; Ring is fragile under packet loss |
| **Paxos / Raft** | Consensus-based coordination | Solves a broader problem than leader election; requires replicated logs, majority quorums, disproportionate overhead for constrained devices |
| **LEACH / HEED** | Energy-aware clustering protocols | Energy-aware but not self-stabilizing; produce multiple cluster heads with centralized base station |
| **PraSLE** | Self-stabilizing leader election for IoT | Self-stabilizing but context-blind: ignores energy, link quality; fixed timeouts; no rotation |

**Our Approach:** Extend PraSLE with context-aware scoring, adaptive timeouts, controlled rotation, and backup recovery, while preserving self-stabilization guarantees.

---

## Slide 5: Research Objectives

- **RQ1:** What impact does extending self-stabilizing leader election with energy awareness, link-quality awareness, and adaptive timing have on convergence behavior, leader stability, and network lifetime?

- **RQ2:** How does Adaptive-PraSLE perform relative to classical (Bully, Ring) and self-stabilizing (PraSLE) baselines across varying network sizes, topologies, and packet-loss conditions?

- **RQ3:** What resource overhead do the extensions introduce, and how does this scale relative to the performance gains?

**Approach:**
- Design and implement Adaptive-PraSLE in Contiki-NG
- Evaluate using 6,400 simulation runs in Cooja across 10 algorithm variants, 4 network sizes, and multiple fault/noise conditions

---

## Slide 6: Background (Section Header)

**Background**

---

## Slide 7: Leader Election Problem

**Definition:** A set of processes collectively agree on a single coordinator (leader).

**Properties required:**
- **Safety:** At most one leader at any time
- **Liveness:** Eventually exactly one leader is elected
- **Validity:** Only a participating process can be elected

**Applications:** Database replication (primary election), sensor data aggregation, robotic swarm coordination

---

## Slide 8: Self-Stabilization

**Introduced by Dijkstra (1974):** A system that recovers to correct behavior from any arbitrary initial state.

**Two properties:**
- **Convergence:** From any state, eventually reaches a legitimate state
- **Closure:** Once in a legitimate state, remains legitimate (if no further faults occur)

**Relevance to IoT:**
- Devices suffer transient faults: power glitches, memory corruption, interference
- No manual intervention is possible for recovery
- Self-stabilization handles any transient corruption automatically

---

## Slide 9: PraSLE: The Foundation

**PraSLE Algorithm (Conard & Ebnenasir, 2021):**

- Round-based: each node maintains a pair (min_score, leader_id)
- Each round: receive neighbors' pairs, adopt the best (lowest), broadcast if improved
- After K rounds: the globally best score has propagated and all nodes agree on one leader
- Unreliable mode: runs continuously with periodic resets for self-stabilization

**Parameters:**
- T: round duration (maximum network latency)
- K: rounds per cycle (must be at least the network diameter)

**Complexity:** O(K·n·Δ) messages per cycle, O(K·T) time, O(1) space per node

---

## Slide 10: IoT Constraints

**Resource-Constrained Devices (RFC 7228):**
- Class 0: <10 KiB RAM, <100 KiB Flash
- Class 1: ~10 KiB RAM, ~100 KiB Flash

**Energy Budget:**
- Radio TX: ~24 mA | Radio RX: ~20 mA | Sleep: ~0.001 mA
- Every message has a significant energy cost
- Implication: minimize radio-on time

**Wireless Communication:**
- IEEE 802.15.4: 250 kbps, 10-100m range
- Packet loss of 10-30% is common
- Asymmetric links, variable quality, gray zone

---

## Slide 11: Contiki-NG and Cooja

**Implementation Platform:**
- **Contiki-NG:** Lightweight IoT OS with protothreads, IPv6/6LoWPAN, Energest energy accounting
- **Cooja Simulator:** Cycle-accurate emulation, configurable radio models, deterministic experiments, fault injection

**Why Contiki-NG?**
- Energest module for software-based energy estimation
- Radio driver provides RSSI, LQI, ETX metrics
- Reproducible experiments across 6,400 runs
- Same platform as PraSLE, enabling direct comparison under identical conditions

---

## Slide 12: Fault Model and Scope

**This thesis targets transient faults in resource-constrained IoT networks.**

| Fault Type | Examples | Handled By |
|---|---|---|
| **State corruption** | Power glitches resetting protocol variables, memory corruption from electromagnetic interference | Self-stabilization: periodic reset cycles re-converge to correct state |
| **Message loss** | Packet drops due to interference, collisions, or fading | Continuous election cycles with broadcast redundancy |
| **Node crash and restart** | Battery depletion, unexpected reboot into arbitrary state | Self-stabilization: node re-joins and converges without external intervention |
| **Temporary link failure** | Obstructions, environmental changes causing transient disconnection | Adaptive timeouts distinguish slow links from failed nodes |

**Explicitly out of scope:**
- **Byzantine faults:** Malicious nodes falsifying scores or forging messages
- **Sybil attacks:** A node impersonating multiple identities
- **Message tampering or eavesdropping:** No encryption or authentication is used
- **Permanent hardware failures:** Persistent bit-flips in program code or irreparable hardware defects

**Rationale:** The focus is on the algorithmic contribution. Self-stabilization covers the dominant failure modes in IoT deployments (transient faults, lossy links, unattended reboots). Byzantine tolerance would significantly increase protocol complexity and message overhead, conflicting with the lightweight design goal for Class 0/1 devices.

---

## Slide 13: Concept and Solution Approach (Section Header)

**Concept and Solution Approach**

---

## Slide 13: Adaptive-PraSLE: Five Extensions

**Adaptive-PraSLE = PraSLE + Five Context-Aware Extensions**

| Extension | Addresses | Mechanism |
|---|---|---|
| 1. Energy-Aware Scoring | Context-blind selection | Battery level in ranking score |
| 2. Link-Quality-Aware Scoring | Poor-connectivity leaders | RSSI/LQI with freshness weighting |
| 3. Controlled Leader Rotation | Permanent leadership drain | Proactive handover at low energy |
| 4. Adaptive Timeouts | Fixed timing mismatch | Jacobson's RTT estimation from TCP |
| 5. Backup Leader List | Full re-election delay | Ranked successor candidates for fast recovery |

**Key Principle:** Extensions modify how scores are computed and how timeouts are managed, but do not change PraSLE's core propagation logic. Self-stabilization is therefore preserved.

---

## Slide 14: Composite Scoring Function

**Score = w_E · S_E + w_L · S_L + w_C · S_C + w_P · S_P**

Lower score = better leader candidate.

| Component | Weight | What it measures | Score range |
|---|---|---|---|
| Energy (S_E) | 30% | Remaining battery % | 0 (full) to 100 (empty) |
| Link Quality (S_L) | 30% | Avg RSSI to neighbors + freshness | 0 (strong) to 100 (poor) |
| Connectivity (S_C) | 20% | Number of valid neighbors | 0 (many) to 100 (isolated) |
| CPU (S_P) | 20% | Processor utilization | 0 (idle) to 100 (busy) |

**Tiebreaking:** If scores are equal, lowest node ID wins (lexicographic comparison).

**Weights are configurable** at compile time; adaptive tuning is identified as future work.

---

## Slide 15: Adaptive Timeout Mechanism

**Problem:** A fixed timeout T leads to false failure suspicion if too short and delayed detection if too long.

**Solution:** Jacobson's RTT estimation (established in TCP since 1988):

- SRTT ← 7/8 · SRTT + 1/8 · RTT_sample
- RTTVAR ← 3/4 · RTTVAR + 1/4 · |RTT_sample - SRTT|
- Timeout ← SRTT + 4 · RTTVAR

**Behavior:**
- Stable network: small RTTVAR leads to tight timeout and fast failure detection
- Variable network: large RTTVAR leads to relaxed timeout and fewer false alarms
- Clamped to [150ms, 5000ms] to prevent extreme values

---

## Slide 16: Controlled Leader Rotation and Backup Recovery

**Controlled Rotation:**
1. Leader detects low energy (<20% threshold)
2. Selects best successor from known candidates
3. Sends HANDOVER_REQ; successor responds with HANDOVER_ACK
4. Old leader steps down; both broadcast updated leadership

**Safeguards:** 30s minimum leadership term to prevent oscillation, 3 retries, fallback to backup list.

**Backup Leader List:**
- Each node maintains a ranked list of top 3 backup candidates
- On leader failure: promote backup without full K-round re-election
- Full election runs in the background to confirm or correct

---

## Slide 17: Self-Stabilization Preservation

**How the extensions preserve PraSLE's guarantees:**

| Risk | Mitigation |
|---|---|
| Score instability (never converges) | Scores computed once per cycle, held constant; EWMA smoothing |
| Timeout divergence | Upper/lower bounds clamp corrupted values; reset cycle re-synchronizes |
| Handover oscillation | Minimum 30s leadership term prevents repeated back-and-forth transitions |
| Backup list corruption | Backup is advisory only; full election always runs in the background |

**Key Mechanism: Periodic Reset Cycles**
- Every R=3 election cycles, all nodes reset state and run a fresh K-round election
- Provides the same recovery guarantee as PraSLE's continuous broadcasting
- Achieves lower average message rate through conditional broadcasting between resets

---

## Slide 18: Conditional Broadcasting + Reset Cycles

**PraSLE:** Broadcasts every round, unconditionally. This leads to high message overhead.

**Adaptive-PraSLE:** Broadcasts only when (min, leader) values change. This reduces messages by 74-92%.

**How self-stabilization is maintained:**

```
Cycle 1: Initial election (active message exchange)
Cycle 2: Steady state (minimal traffic, conditional broadcasting)
Cycle 3: RESET. All nodes reinitialize, fresh election runs.
Cycle 4: Steady state again...
(pattern repeats)
```

- Corrupted state is cleared at every reset
- Worst-case recovery time: R × K × T (e.g., 3 × 2 × 1s = 6s for 10-node clique)

---

## Slide 19: Implementation (Section Header)

**Implementation**

---

## Slide 20: System Architecture

**Four-Layer Node Architecture:**

1. **Monitoring Layer:** Collects context. Energy via Energest, link quality via RSSI/LQI from the radio driver, connectivity via neighbor count, CPU utilization.
2. **Scoring Engine:** Combines inputs into a composite score (weighted sum, lower = better).
3. **Election Engine:** PraSLE round-based minimum-finding with five extensions.
4. **Communication Layer:** UDP/IPv6 multicast (ff02::1). One transmission reaches all neighbors.

Single Contiki-NG protothread process; incoming messages handled via UDP callback.

---

## Slide 21: Algorithm Design

**Adaptive-PraSLE Election Round (Algorithm 2 from thesis):**

1. Initialize: round = K+1, min = N+1, temp_min = GetCompositeScore(), leader = self
2. For each round (K down to 0):
   - Receive phase: listen for adaptive timeout duration, collect neighbor pairs
   - Update phase: if a received pair is better, adopt it
   - If values changed, broadcast to neighbors (conditional)
3. Return the elected leader identity

**Three key modifications from PraSLE:**
- Composite scoring replaces static node IDs
- Conditional broadcasting replaces unconditional
- Adaptive timeouts replace fixed T

---

## Slide 22: Message Formats

**Election Message (14 bytes):**

| Field | Size | Purpose |
|---|---|---|
| msg_type | 1B | Message type (election/heartbeat/handover) |
| padding | 1B | Alignment |
| min_value | 2B | Composite ranking score |
| leader_id | 2B | Current leader ID |
| sender_id | 2B | Sender node ID |
| energy_level | 1B | Battery % (0-100) |
| avg_rssi | 1B | Average RSSI |
| neighbor_count | 1B | Valid neighbor count |
| flags | 1B | IS_LEADER, LOW_ENERGY, HANDOVER_PENDING |
| seq_num | 2B | Sequence number |

**Handover Message (8 bytes):** msg_type, old_leader, new_leader, seq_num

**Comparison with PraSLE:** 8 bytes (min_value + leader_id only)

---

## Slide 23: State Machine

**Five States:**

- **INIT:** Neighbor discovery and parameter initialization
- **ELECTION:** Active round-based score propagation
- **NORMAL:** Converged; this node is a follower. Monitors for leader failure and periodic resets.
- **LEADER:** Converged; this node is the leader. Sends heartbeats and monitors for handover triggers.
- **RECOVERY:** Leader failure detected. Attempting fast recovery from backup list.

**Transitions:** Based on election progress, convergence, leader failure detection, and controlled handover.

---

## Slide 24: Configuration and Feature Toggles

**Modular Design:** Each extension can be independently enabled or disabled at compile time.

| Feature Toggle | Default | Purpose |
|---|---|---|
| ADAPTIVE_ENERGY_AWARE | ON | Energy-aware scoring |
| ADAPTIVE_LINK_QUALITY_AWARE | ON | Link quality scoring |
| ADAPTIVE_LEADER_ROTATION | ON | Controlled handover |
| ADAPTIVE_TIMEOUTS | ON | Jacobson-based adaptive timeouts |
| ADAPTIVE_BACKUP_LIST | ON | Backup leader list |
| ADAPTIVE_RESET_CYCLES | ON | Periodic reset for self-stabilization |

This supports ablation studies. The protocol remains correct regardless of which extensions are active.

---

## Slide 25: Evaluation (Section Header)

**Evaluation**

---

## Slide 26: Experimental Methodology

**Simulation Environment:**
- Cooja simulator with Contiki-NG
- 6,400 individual simulation runs
- 100 trials per configuration (unique random seeds)

**10 Algorithm Variants:**
- Baselines: Bully, Ring (Chang-Roberts)
- PraSLE: clique, line, ring, mesh topologies
- Adaptive-PraSLE: clique, line, ring, mesh topologies

**4 Experiment Types:**
- Convergence time, message overhead, fault tolerance (leader crash), noise resilience (10%, 30%, 50% packet loss), network partition

**Fair Comparison (Case 2):** All algorithms use T=2s. The broadcast rate is equalized so that differences reflect algorithmic efficiency rather than parameter tuning.

---

## Slide 27: Convergence Time Results

**Adaptive-PraSLE converges 44% faster than standard PraSLE**

| Algorithm | 5 nodes | 10 nodes | 50 nodes | 100 nodes |
|---|---|---|---|---|
| Bully | 5.4s | 5.3s | 4.7s | 4.5s |
| Ring | 1.1s | 1.6s | 3.5s | 7.1s |
| PraSLE (clique) | 3.2s | 3.1s | 2.9s | 2.8s |
| **Adaptive-PraSLE (clique)** | **2.0s** | **1.9s** | **1.7s** | **1.6s** |

- PraSLE and Adaptive-PraSLE show consistent performance across all sizes
- Ring degrades at scale with high variance (7.1s at 100 nodes)
- Bully is stable but slow (~5s), dominated by its fixed timeout
- Adaptive-PraSLE's advantage comes from conditional broadcasting reducing channel contention

*(Show Figure 6.1 from thesis)*

---

## Slide 28: Message Overhead Results

**Adaptive-PraSLE reduces messages by 74-92%**

| Algorithm | 5 nodes | 100 nodes |
|---|---|---|
| Bully | 164 | 13,400 |
| Ring | - | 16,400 |
| PraSLE (clique) | 1,900 | 27,200 |
| PraSLE (line/ring/mesh) | 1,900 | 38,700 |
| **Adaptive-PraSLE (clique)** | **68** | **2,100** |
| **Adaptive-PraSLE (line/ring/mesh)** | **78** | **9,300-10,000** |

- Standard PraSLE has the highest overhead due to unconditional broadcasting
- Adaptive-PraSLE clique: 92% reduction (27,200 to 2,100 at 100 nodes)
- Total bytes: 217,600B (PraSLE) vs 29,400B (Adaptive-PraSLE), an 86% reduction

*(Show Figure 6.2 from thesis)*

---

## Slide 29: Fault Tolerance Results

**PraSLE variants achieve sub-second recovery at scale**

| Algorithm | 5 nodes | 10 nodes | 50 nodes | 100 nodes |
|---|---|---|---|---|
| Bully | 7.0s | 7.0s | 7.0s | 7.0s |
| Ring | 6.4s | 11.4s | 53s | 102s (13-21% failure) |
| PraSLE (clique) | 3.0s | 3.0s | 1.6s | 0.2s |
| **Adaptive-PraSLE (ring)** | 4.0s | 1.7s | **0.037s** | **0.25s** |

- Recovery improves with network size for PraSLE variants because more nodes detect failure simultaneously
- Ring recovery degrades from 6.4s to 102s, making it unusable at scale
- Bully is constant at 7s (timeout-bounded)

*(Show Figure 6.3 from thesis)*

---

## Slide 30: Noise Resilience Results

**PraSLE variants are virtually unaffected by packet loss**

**At 100 nodes:**
| Algorithm | 0% loss | 10% loss | 30% loss | 50% loss |
|---|---|---|---|---|
| Ring | 7.1s | **117s** (24% fail) | 105s | 105s |
| PraSLE | 2.8s | 2.8s | 2.8s | 2.8s |
| **Adaptive-PraSLE** | **1.6s** | **1.6s** | **1.6s** | **1.6s** |

- Ring: even 10% loss causes a 16x slowdown; 24-36% of trials fail entirely
- Reason: sequential forwarding means P(at least 1 of 50 messages lost) = 1-(0.9)^50 = 99.5%
- PraSLE/Adaptive-PraSLE: broadcast gives every message a fresh chance each round; self-stabilization retries automatically

*(Show Figures 6.4, 6.5, 6.6 from thesis)*

---

## Slide 31: Network Partition Results

**All algorithms correctly handle network partitions:**
- Each isolated group independently elects its own leader
- No split-brain behavior observed
- Convergence times scale with effective partition size
- PraSLE's self-stabilization guarantees re-convergence when connectivity is restored

*(Show Figure 6.7 from thesis)*

---

## Slide 32: Trade-off Analysis

**Message Overhead vs. Convergence Time (at 100 nodes):**

| Algorithm | Convergence | Messages | Position |
|---|---|---|---|
| Ring | 7.1s | 16,400 | Worst in both metrics |
| Bully | 4.5s | 13,400 | Moderate |
| PraSLE (clique) | 2.8s | 27,200 | Fast but high overhead |
| **Adaptive-PraSLE (clique)** | **1.6s** | **2,100** | **Fastest with fewest messages** |

**Recommendations by use case:**
- **Small networks (10 nodes or fewer):** Any algorithm is adequate; Bully is simplest to implement
- **Large networks (50+ nodes):** Avoid Ring; Adaptive-PraSLE (clique) provides the best balance
- **Lossy environments:** PraSLE variants are clearly superior
- **Energy-constrained deployments:** Adaptive-PraSLE minimizes total bytes transmitted

*(Show Figure 6.8 from thesis)*

---

## Slide 33: Algorithm Scalability

**PraSLE and Adaptive-PraSLE maintain consistent performance across all network sizes:**
- Ring shows linear or worse scaling with high variance at 50+ nodes
- Bully is stable but slow (~5s regardless of size)
- PraSLE shows nearly constant convergence (~2.8s)
- Adaptive-PraSLE shows nearly constant convergence at a faster rate (~1.6s)

**Fault recovery scales inversely.** Recovery time decreases with larger networks for PraSLE variants.

*(Show Figure 6.9 from thesis)*

---

## Slide 34: Answering the Research Questions

**RQ1, Impact of extensions:**
- 44% faster convergence (1.6s vs 2.8s at 100 nodes)
- 74-92% fewer messages; 86% reduction in total bytes
- Sub-second fault recovery at scale
- Self-stabilization preserved through periodic reset cycles

**RQ2, Performance vs. baselines:**
- Adaptive-PraSLE outperforms all baselines in every metric
- Ring fails under packet loss
- Bully is stable but offers no context awareness

**RQ3, Resource overhead vs. gains:**
- Messages grow from 8 to 14 bytes (+75%), but count drops 74-92%
- Total bytes transmitted reduced by 86%
- Additional state (backup list, EWMA counters) fits Class 1 device memory

---

## Slide 35: Conclusion and Future Work (Section Header)

**Conclusion and Future Work**

---

## Slide 36: Contributions

**Key Achievements:**
- Designed Adaptive-PraSLE: first protocol combining self-stabilization with energy-aware and link-quality-aware leader election
- Implemented in Contiki-NG with modular feature toggles for each extension
- Rigorous evaluation across 6,400 simulation runs comparing 10 algorithm variants

**Key Results:**
- 44% faster convergence than PraSLE, 74-92% fewer messages
- Virtually unaffected by up to 50% packet loss
- Sub-second fault recovery at scale (34ms at 50 nodes)
- Classical algorithms (Ring, Bully) shown to be unsuitable for IoT at scale

---

## Slide 37: Threats to Validity

| Threat | Impact |
|---|---|
| Simulation vs. real hardware | Cooja's UDGM radio model simplifies real-world propagation (multipath fading, interference) |
| Uniform nodes | All nodes are identical; heterogeneous hardware may behave differently |
| Static topology | No mobility tested; mobile nodes would stress the protocol differently |
| Parameter sensitivity | Scoring weights (30/30/20/20) chosen from preliminary experiments, no formal sensitivity analysis |
| Benign environment | No security or Byzantine fault model; assumes all nodes are honest |

---

## Slide 38: Future Work

- **Hardware testbed validation:** Deploy on physical IoT motes (CC2538/CC26xx) with real radio conditions
- **Mobile network support:** Dynamic neighbor discovery, faster reset cycles, position-aware scoring
- **Adaptive weight tuning:** Online learning to adjust scoring weights based on network conditions
- **Security extensions:** Message authentication (HMAC), Byzantine fault tolerance
- **Heterogeneous networks:** Evaluate with mixed device capabilities
- **Adaptive reset interval:** Increase R when stable, decrease when failures are detected
- **RPL integration:** Coordinate leader election with routing protocol (leader as DODAG root)

---

## Slide 39: Q&A

**Thank you!**

**Questions?**

Source code: https://github.com/Pushpit07/PraSLE-Simulation-in-Contiki-NG-Cooja

---

## Slide 40: Appendix (Section Header)

**Appendix**

---

## Slide 41: What is self-stabilization?

- Introduced by Dijkstra in 1974
- A system recovers to correct behavior from any arbitrary initial state
- Two properties: Convergence (eventually reaches legitimate state) and Closure (stays legitimate)
- Differs from Byzantine fault tolerance: handles transient faults, not persistent malicious behavior
- In Adaptive-PraSLE: even if all nodes' state is corrupted, the next reset cycle re-converges to the correct leader

---

## Slide 42: Why PraSLE as the base protocol?

- **Lightweight:** O(1) space per node, fits Class 0/1 devices
- **Self-stabilizing:** Recovers from any transient fault without manual intervention
- **Extensible:** Modular enough to add energy/link-quality scoring without breaking core guarantees
- **Alternatives considered and rejected:**
  - Bully/Ring: not self-stabilizing
  - Paxos/Raft: full consensus is disproportionate for leader election
  - LEACH/HEED: energy-aware but not self-stabilizing
  - Population protocols: pairwise interaction model does not match IoT broadcast communication

---

## Slide 43: How does Jacobson's algorithm work?

**From TCP congestion control (1988):**
- Track smoothed RTT: SRTT = 7/8 · SRTT + 1/8 · RTT_sample
- Track variance: RTTVAR = 3/4 · RTTVAR + 1/4 · |RTT_sample - SRTT|
- Compute timeout: RTO = SRTT + 4 · RTTVAR
- Clamped to [150ms, 5000ms]

**Stable network:** Small RTTVAR leads to tight timeout and fast failure detection.
**Variable network:** Large RTTVAR leads to relaxed timeout and fewer false alarms.

---

## Slide 44: Why Case 2 (standardized T=2s)?

- Case 1: PraSLE uses T=1s while Bully/Ring use heartbeat=2s, meaning PraSLE broadcasts 2x more frequently and has an unfair structural advantage
- Case 2: All algorithms use T=2s, so the broadcast rate is equalized
- Measured differences reflect algorithmic efficiency rather than parameter tuning
- Absolute times are conservative (safety margins for real wireless networks); relative differences are the meaningful comparison

---

## Slide 45: Why does Ring fail under packet loss?

- Ring uses sequential point-to-point forwarding: each node passes the message to its successor
- If any single message in the chain is lost, the entire election stalls
- At 50 nodes with 10% loss: P(at least 1 lost) = 1 - (0.9)^50 = 99.5%
- At higher loss rates, cascading failures cause the algorithm to exceed the simulation timeout
- 13-36% of trials failed to converge entirely
- PraSLE uses broadcast, so every message gets a fresh chance each round

---

## Slide 46: What is the attacker model?

**Benign environment assumed. All nodes are honest.**

**Covered:** Transient faults (power glitches, message loss, reboots, link failures)

**Not covered:**
- Byzantine faults (malicious score falsification)
- Sybil attacks (impersonating multiple identities)
- Eavesdropping or message tampering (no encryption)
- Permanent hardware failures

**Rationale:** The focus is on the algorithmic contribution. Byzantine tolerance would significantly increase complexity and overhead, conflicting with the lightweight design goal.

---

## Slide 47: How does controlled handover work?

1. Leader detects energy below 20% threshold
2. Selects best successor from backup list
3. Sends HANDOVER_REQ to successor
4. Successor responds with HANDOVER_ACK and assumes leader role
5. Old leader receives ACK and steps down
6. Both broadcast updated leadership

A brief overlap period exists where both nodes consider themselves leader. This is preferred over a leaderless gap. It is resolved within one round-trip time. Self-stabilization guarantees convergence.

---

## Slide 48: Why R=3 for reset cycles?

- R=2: No steady-state cycle, so conditional broadcasting provides zero benefit
- R=3: One initial election, one steady-state cycle, one reset. This provides a reasonable balance.
- R>3: More efficiency but a longer window for corrupted state to persist
- Worst-case recovery: R × K × T (e.g., 3 × 2 × 1s = 6s for 10-node clique)
- Future work: adaptive R that increases when stable and decreases when failures are detected

---

## Slide 49: Message size 14 bytes vs 8 bytes. Is the overhead justified?

**Per-message:** +75% (14 vs 8 bytes)
**Message count:** -74 to -92%

**Total bytes at 100 nodes (clique):**
- PraSLE: 27,200 msgs × 8B = 217,600 bytes
- Adaptive-PraSLE: 2,100 msgs × 14B = 29,400 bytes
- **86% total reduction**

The 6 extra bytes (energy, RSSI, neighbor count, flags, seq_num) enable all five extensions.

---

## Slide 50: How does energy estimation work?

**In simulation (Cooja):**
- Energest module tracks time in each power state (TX, RX, CPU, sleep)
- Radio duty cycle = (TX + RX time) / total time
- Battery drained proportionally to duty cycle

**On real hardware:**
- On-chip battery monitor (e.g., CC26xx AON_BATMON)
- ADC with voltage divider
- External BMS via I2C/UART
- Scoring function remains identical; only the energy input source changes

---

## Slide 51: Topology effect on performance

**Convergence time:** Topology-independent for standard PraSLE (all ~2.8s at 100 nodes) because total cycle time = T regardless of K.

**Message overhead:** Topology-dependent.
- Clique (K=2): fewest messages due to short cycles and simultaneous propagation
- Line (K=N): most messages due to hop-by-hop propagation

**For Adaptive-PraSLE:** Clique has the lowest message count (2,100) but slightly slower convergence (1.6s) than line/ring/mesh (1.5s, with 9,300-10,000 messages).

**Recommendation:** Use clique topology when message overhead is the priority; use other topologies when clique connectivity is not physically achievable.

---

## Slide 52: FLP impossibility and this work

- Fischer, Lynch, and Paterson (1985): No deterministic consensus in purely asynchronous systems if even one process may fail
- Leader election is closely related to consensus
- This work operates under partial synchrony (bounded but unknown message delay) and sidesteps FLP using timeouts
- The adaptive timeout mechanism dynamically estimates the unknown delay bound rather than using a fixed value
- Connected to Chandra-Toueg failure detector theory: the Omega detector (weakest for consensus) amounts to eventual leader election

---

## Slide 53: Could this work for robotic swarms?

- For static robot teams (e.g., factory floor): applicable without modification
- For mobile swarms, changes would be needed:
  - Dynamic neighbor discovery
  - Faster reset cycles (lower R)
  - Position/velocity in the scoring function
  - Potentially multi-leader election for large swarms
- Controlled rotation is especially valuable for battery-limited robots
- The modular design (configurable weights, feature toggles) supports customization

---

## Slide 54: Why no ablation study?

- The evaluation already includes 6,400 simulation runs (~25 hours of compute)
- A full ablation with all 2^6 = 64 toggle combinations would multiply this significantly
- Feature toggles are implemented and available for future targeted ablation
- The evaluation focused on answering the three research questions by comparing complete Adaptive-PraSLE against baselines
- A specific gap identified for future work: isolating the backup list's contribution to fault recovery

---

## Slide 55: How is PraSLE-ring different from the classical Ring algorithm?

PraSLE-ring and the classical Ring algorithm both restrict communication to ring neighbors, but they work in fundamentally different ways.

**Classical Ring (Chang-Roberts):**
- Passes a single election token around the ring sequentially: one node sends to the next, all the way around
- If any single message is lost, the entire election stalls
- Terminates after one complete pass
- No built-in recovery from leader failure; requires external re-initiation

**PraSLE-ring:**
- Uses broadcast communication. Every node sends its current best (min, leader) pair to both ring neighbors every round, for K rounds
- If a message is lost, the same information is sent again in the next round. The protocol is self-healing by design.
- Runs continuously with periodic resets, so it recovers from leader crashes without anyone triggering a new election
- The "ring" refers to the logical topology (which neighbors are accepted), not the communication pattern

This distinction explains why Ring degrades to 117s at 100 nodes under 10% packet loss while PraSLE-ring remains at 2.8s.

---

## Slide 56: How does the retry mechanism work?

**Standard PraSLE: Unconditional periodic broadcasting**

Every round (every T seconds), each node broadcasts its current (min, leader) pair regardless of whether the values changed. There is no explicit retry. The redundancy is built into the protocol's periodic structure. If a broadcast is lost due to packet loss, the same information is sent again next round.

This is why standard PraSLE shows less than 0.4% convergence change at 50% packet loss.

**Adaptive-PraSLE: Conditional broadcasting + reset cycles**

Adaptive-PraSLE only broadcasts when the (min, leader) pair actually changes. This reduces message overhead by 74-92%, but it means lost messages are not immediately re-sent in the next round.

Recovery from lost messages comes from the reset cycle mechanism: every 3 election cycles, all nodes reset their state and run a fresh election, which triggers new broadcasts. This is slower than PraSLE's per-round redundancy, which explains why Adaptive-PraSLE shows up to 3% convergence variation at 50% loss compared to less than 0.4% for standard PraSLE.

---

## Slide 57: Why were Bully and Ring chosen as baseline protocols?

- Canonical classical leader election algorithms, widely covered in distributed systems textbooks [Coulouris et al.] and surveys [Rahman]
- Span two ends of the complexity spectrum: Bully is fast but message-heavy (O(n^2)); Ring is simple but slow (O(n·d))
- Both assume reliable communication and no energy constraints, making them suitable for demonstrating why IoT-specific design is necessary
- Including them alongside PraSLE strengthens external validity beyond incremental improvements over a single protocol

---

## Slide 58: Why is receiver-side filtering used instead of sender-side filtering?

**Current design:** Broadcast to ff02::1, then filter at the receiver via is_logical_neighbor().

**Why not unicast to each neighbor instead?**

| Factor | Broadcast + receiver filter | Unicast to each neighbor |
|---|---|---|
| Radio TX per round | 1 | K (number of neighbors) |
| Energy cost | Low | K times higher |

In wireless networks, the physical layer is broadcast regardless. All nodes in range receive the signal whether it is addressed to them or not. Receiver-side filtering costs roughly 10-50 CPU cycles (a linear search through a small array), which is negligible compared to additional radio transmissions. In a 10-node clique, unicast would require 9 transmissions instead of 1.