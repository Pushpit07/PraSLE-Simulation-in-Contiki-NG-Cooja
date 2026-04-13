# Thesis Defense Presentation
# Adaptive and Fault-Tolerant Leader Election in Resource-Constrained IoT and Robotic Clusters

---

# 1. Introduction and Motivation

---

## Slide 1: Title Slide

**Adaptive and Fault-Tolerant Leader Election in Resource-Constrained IoT and Robotic Clusters**

Pushpit Bhardwaj
Course: Distributed Systems Engineering

Referee: Dr. Ing. Thomas Springer
Supervisor: Leonard Herbst M.Sc.
Supervising Professor: Prof. Dr. Matthias Wahlisch

> **Speaker Notes:** Good morning/afternoon. My name is Pushpit Bhardwaj and I will be presenting my master thesis on adaptive and fault-tolerant leader election in resource-constrained IoT and robotic clusters, supervised by Leonard Herbst and reviewed by Dr. Thomas Springer under Professor Wahlisch.

---

## Slide 2: Motivation

| IoT Reality | Classical Assumption |
|---|---|
| Lossy wireless links (10-30% packet loss) | Reliable communication |
| Kilobytes of RAM (Class 0/1, RFC 7228) | Sufficient resources |
| Battery-powered; TX ~24 mA, sleep ~0.001 mA | No energy constraints |
| Unattended, no manual recovery | Manual intervention |

**Why Leader Election?**
- Single coordinator for data aggregation, task assignment, group decisions
- Required in sensor networks, robotic swarms, industrial IoT

> **Speaker Notes:** Leader election is a fundamental coordination primitive in distributed systems. A single coordinator simplifies tasks like data aggregation and group decisions. However, classical leader election protocols assume reliable communication, sufficient compute resources, and no energy constraints. IoT networks violate all of these assumptions. Devices operate on kilobytes of RAM, run on batteries where radio transmission draws 24 milliamps compared to 1 microamp during sleep, communicate over lossy wireless links with 10 to 30 percent packet loss, and are deployed in environments where manual recovery is not possible. This mismatch between classical assumptions and IoT reality motivates the need for a new approach.

---

## Slide 3: Leader Election Problem

**Definition:** A set of processes collectively agree on a single coordinator (leader).

**Properties:**
- **Safety:** At most one leader at any time
- **Liveness:** Eventually exactly one leader is elected
- **Validity:** Only a participating process can be elected

**Applications:** Database replication, sensor data aggregation, robotic swarm coordination

> **Speaker Notes:** Before discussing the problem, let me briefly define leader election. It is a standard coordination problem in distributed systems where a set of processes must collectively agree on a single coordinator. A correct algorithm must satisfy three properties: safety, meaning at most one leader at any time; liveness, meaning eventually exactly one leader is elected and acknowledged; and validity, meaning only a participating process can be elected. Leader election arises in many settings: database replicas elect a primary to sequence writes, sensor networks elect aggregators to compress readings, and robot swarms elect coordinators to direct movement.

---

# 2. Problem Discussion, Thesis Goals, Research Questions

---

## Slide 4: Problem Statement

**Existing protocols do not fully meet the needs of constrained IoT networks.**

- **Classical algorithms** (Bully, Ring): cannot distinguish failed nodes from temporary disruptions
- **Consensus protocols** (Paxos, Raft): disproportionate overhead for the narrower task of leader election
- **PraSLE** [Conard & Ebnenasir, 2021]: self-stabilizing but three problems remain:

1. **Context-blind leader selection.** Elects the node with the lowest identifier, without considering remaining energy or link quality. A nearly depleted node may become leader and fail shortly after.
2. **Fixed timing.** Static timeout does not adapt to changing network conditions. Too short causes false failure suspicion; too long delays detection.
3. **Permanent leadership.** Leader serves until it fails, concentrating energy consumption on a single node while others conserve their batteries.

> **Speaker Notes:** Looking at the existing landscape: classical algorithms like Bully and Ring assume reliable communication and cannot distinguish between a crashed node and a temporarily disrupted link, leading to unnecessary re-elections. Consensus protocols like Paxos and Raft solve a broader problem than what we need. They implement full distributed state machines with replicated logs and majority quorums, which is disproportionate when all we need is agreement on a coordinator identity. PraSLE, proposed by Conard and Ebnenasir in 2021, is specifically designed for constrained IoT networks and provides self-stabilization, meaning it recovers from any arbitrary state corruption. However, it leaves three problems unsolved. First, it selects leaders by node ID alone, so a node with a nearly depleted battery can become leader and fail shortly after, triggering a full re-election. Second, it uses a fixed timeout that cannot adapt to changing wireless conditions. Third, once elected, a leader serves until it fails, concentrating energy consumption on a single node.

---

## Slide 5: Thesis Goals and Research Questions

**Goal:** Design, implement, and evaluate Adaptive-PraSLE with context-aware leader selection while preserving self-stabilization.

- **RQ1:** Impact of energy awareness, link-quality awareness, and adaptive timing on convergence, stability, and network lifetime?
- **RQ2:** Performance relative to Bully, Ring, and PraSLE across network sizes, topologies, and packet-loss conditions?
- **RQ3:** Resource overhead of the extensions vs. performance gains?

**Approach:** Contiki-NG implementation, 6,400 Cooja simulation runs, 10 algorithm variants

> **Speaker Notes:** The goal of this thesis is to design, implement, and evaluate Adaptive-PraSLE, which extends PraSLE with context-aware leader selection while preserving its self-stabilization guarantees. We address three research questions. RQ1 asks about the impact of our extensions on convergence behavior, leader stability, and network lifetime. RQ2 compares Adaptive-PraSLE against classical Bully and Ring algorithms and the original PraSLE across different network sizes, topologies, and packet loss conditions. RQ3 examines the resource overhead that our extensions introduce and whether the gains justify the cost. The protocol is implemented in Contiki-NG and evaluated through 6,400 simulation runs in the Cooja simulator, covering 10 algorithm variants across 4 network sizes.

---

## Slide 6: Fault Model and Scope

| Fault Type | Handled By |
|---|---|
| State corruption (power glitches, interference) | Self-stabilization via periodic reset cycles |
| Message loss (collisions, fading) | Broadcast redundancy in continuous election cycles |
| Node crash and restart | Self-stabilization: re-joins without intervention |
| Temporary link failure | Adaptive timeouts distinguish slow links from failures |

**Out of scope:** Byzantine faults, Sybil attacks, message tampering, permanent hardware failures

> **Speaker Notes:** Our fault model targets transient faults, which are the dominant failure modes in IoT deployments. These include state corruption from power glitches or electromagnetic interference, message loss due to collisions and fading, unexpected node crashes and restarts, and temporary link failures. Each is addressed by a specific mechanism: self-stabilization handles state corruption, broadcast redundancy handles message loss, and adaptive timeouts help distinguish slow links from actual failures. We explicitly exclude Byzantine faults, Sybil attacks, message tampering, and permanent hardware failures. This is a deliberate scope decision. Byzantine tolerance would significantly increase protocol complexity and message overhead, conflicting with the lightweight design goal for Class 0 and Class 1 devices. Security extensions are identified as future work.

---

# 3. Related Work with Demarcation

---

## Slide 7: Related Work

| Approach | Limitation for IoT Leader Election |
|---|---|
| **Bully / Ring** | Not self-stabilizing; assume reliable communication |
| **Paxos / Raft** | Disproportionate overhead (replicated logs, quorums) |
| **LEACH / HEED** | Energy-aware but not self-stabilizing; centralized base station |
| **Self-stabilizing LE** (Altisen et al., Mo et al.) | Elect by identifiers, not runtime conditions |
| **PraSLE** | Context-blind; fixed timeouts; no rotation or backup |

> **Speaker Notes:** Let me briefly position our work against the related literature. Classical algorithms like Bully and Ring are not self-stabilizing and assume reliable communication. Consensus protocols like Paxos and Raft provide strong fault tolerance but their overhead is disproportionate for the narrower task of leader election. Energy-aware clustering protocols like LEACH and HEED consider energy in coordinator selection but are not self-stabilizing, and they produce multiple cluster heads with a centralized base station rather than a single decentralized leader. More recent self-stabilizing leader election algorithms by Altisen et al. and Mo et al. provide recovery from arbitrary states, but they elect leaders based on identifiers or topological properties, not runtime conditions like energy or link quality. PraSLE addresses self-stabilization for IoT specifically, but remains context-blind with fixed timeouts and no rotation mechanism.

---

## Slide 8: Research Gaps and Positioning

| Gap | Adaptive-PraSLE Response |
|---|---|
| No energy awareness + self-stabilization combined | Energy-aware composite scoring + PraSLE core |
| Link quality not used in leader selection | RSSI/LQI scoring with freshness weighting |
| Fixed timeouts in variable wireless environments | Jacobson's adaptive RTT estimation |
| No graceful leadership transition | Controlled handover protocol |
| Full re-election on every failure | Backup leader list for fast recovery |

> **Speaker Notes:** From the related work analysis, we identified five specific gaps. First, no protocol combines energy awareness with self-stabilization. PraSLE provides self-stabilization but ignores energy; LEACH and HEED consider energy but cannot recover from corrupted state. Second, link quality is used in routing protocols for path selection, but not in any leader election protocol. Third, self-stabilizing protocols still use fixed timeouts despite the variable nature of wireless links. Fourth, in PraSLE a leader serves until failure with no graceful transition mechanism. Fifth, when a PraSLE leader fails, the network must complete a full K-round election before a new leader is established. Adaptive-PraSLE addresses each of these gaps with a specific extension, and it is, to the best of our knowledge, the first protocol to combine self-stabilization with energy-aware and link-quality-aware leader election for constrained IoT networks.

---

# 4. Concept Overview and Selected Contributions

---

## Slide 9: Self-Stabilization

**Introduced by Dijkstra (1974):** A system that recovers to correct behavior from any arbitrary initial state.

**Two properties:**
- **Convergence:** From any state, eventually reaches a legitimate state
- **Closure:** Once in a legitimate state, remains legitimate (if no further faults occur)

**Relevance to IoT:**
- Devices suffer transient faults: power glitches, memory corruption, interference
- No manual intervention is possible for recovery
- Self-stabilization handles any transient corruption automatically

> **Speaker Notes:** Self-stabilization is a central concept in this thesis, so let me explain it before presenting PraSLE. It was introduced by Dijkstra in 1974 and means that a system recovers to correct behavior from any arbitrary starting state, no matter how badly corrupted. It guarantees two properties: convergence, meaning the system eventually reaches a legitimate state regardless of its initial configuration, and closure, meaning once in a legitimate state, it stays there as long as no further faults occur. This is particularly important for IoT because these devices operate unattended in environments where transient faults are common. Power glitches can reset protocol variables, electromagnetic interference can corrupt memory, and nodes reboot unexpectedly. There is no administrator to press a reset button. Self-stabilization provides automatic recovery from all of these scenarios. This distinguishes it from traditional fault tolerance approaches that target specific failure models like crash failures or Byzantine behavior. Self-stabilization handles any transient corruption without requiring knowledge of what went wrong.

---

## Slide 10: PraSLE Foundation

**PraSLE Algorithm (Conard & Ebnenasir, 2021):**
- Each node maintains (min_score, leader_id)
- K rounds: receive, adopt best, broadcast if improved
- After K rounds: all nodes agree on one leader
- Unreliable mode: continuous operation with periodic resets

**Parameters:** T (round duration), K (rounds, must be at least network diameter)
**Complexity:** O(K·n·Δ) messages per cycle, O(K·T) time, O(1) space per node

> **Speaker Notes:** PraSLE applies self-stabilization to leader election. It is round-based: each node maintains a pair of minimum score and leader identity. Over K rounds, nodes exchange these pairs with neighbors, always adopting the better value. After K rounds, the globally best score has propagated and all nodes agree on one leader. The parameter K must be at least the network diameter to ensure the best score can reach every node, and T is the round duration. In unreliable mode, PraSLE runs continuously with periodic resets, providing self-stabilization. Its complexity is O(K times n times delta) messages per cycle, with O(1) space per node, making it practical for constrained devices.

---

## Slide 11: Adaptive-PraSLE: Five Extensions

| Extension | Addresses | Mechanism |
|---|---|---|
| 1. Energy-Aware Scoring | Context-blind selection | Battery level in ranking |
| 2. Link-Quality-Aware Scoring | Poor-connectivity leaders | RSSI/LQI + freshness |
| 3. Controlled Rotation | Permanent leadership | Handover at low energy |
| 4. Adaptive Timeouts | Fixed timing | Jacobson's RTT estimation |
| 5. Backup Leader List | Full re-election delay | Ranked successors |

Each extension independently toggleable at compile time.

> **Speaker Notes:** Adaptive-PraSLE extends PraSLE with five context-aware mechanisms. Energy-aware scoring incorporates remaining battery level so that depleted nodes are not elected leader. Link-quality-aware scoring uses RSSI and LQI measurements with a freshness confidence factor so that nodes with poor or stale connectivity information receive worse scores. Controlled leader rotation allows a leader to proactively hand over its role when energy drops below a critical threshold, rather than serving until failure. Adaptive timeouts replace the fixed parameter T with a dynamic timeout computed from observed round-trip times using Jacobson's algorithm from TCP. And the backup leader list maintains ranked successor candidates so that recovery after leader failure does not require a full K-round re-election. The key design principle is that these extensions modify how scores are computed and how timeouts are managed, but they do not change PraSLE's core minimum-finding propagation logic. Self-stabilization is therefore preserved. Each extension can be independently enabled or disabled at compile time, which supports ablation studies.

---

## Slide 12: System Architecture

*(Show Figure 5.1 from thesis)*

Four layers: Monitoring, Scoring, Election, Communication

Implemented in Contiki-NG. UDP/IPv6 multicast (ff02::1). Evaluated in Cooja.

> **Speaker Notes:** The system architecture follows a four-layer design. The monitoring layer collects context information: energy via Contiki-NG's Energest module, link quality via RSSI and LQI from the radio driver, connectivity through neighbor counts, and CPU utilization. The scoring engine combines these inputs into a single composite score using a weighted sum where lower means better. The election engine implements PraSLE's round-based minimum-finding algorithm with our five extensions. And the communication layer handles message exchange using UDP over IPv6 with link-local multicast. All election messages are broadcast to the all-nodes multicast address ff02::1, with topology enforcement performed at the receiver side through neighbor filtering. The entire protocol is implemented as a single Contiki-NG protothread process, and incoming messages are handled asynchronously via a UDP receive callback. We chose Contiki-NG because of its Energest module for energy accounting, its radio driver providing RSSI and LQI metrics, and the Cooja simulator which enables deterministic, reproducible experiments.

---

## Slide 13: Composite Scoring Function

**Score = w_E · S_E + w_L · S_L + w_C · S_C + w_P · S_P**

| Component | Weight | Score range |
|---|---|---|
| Energy (S_E) | 30% | 0 (full) to 100 (empty) |
| Link Quality (S_L) | 30% | 0 (strong) to 100 (poor) |
| Connectivity (S_C) | 20% | 0 (many neighbors) to 100 (isolated) |
| CPU (S_P) | 20% | 0 (idle) to 100 (busy) |

Tiebreaking by node ID. Weights configurable at compile time.

> **Speaker Notes:** The composite scoring function replaces PraSLE's static node-ID-based ranking. It is a weighted sum of four normalized components, each scaled from 0 to 100 where lower is better. Energy gets 30% weight and penalizes nodes with depleted batteries. Link quality also gets 30% and penalizes nodes with poor average RSSI to their neighbors, modulated by a freshness confidence factor that reduces trust in stale measurements. Connectivity gets 20% and penalizes isolated nodes. CPU availability gets 20% and penalizes nodes under heavy processing load. If two nodes have identical composite scores, the tie is broken by node ID using lexicographic comparison, which ensures deterministic leader selection. The weights are configurable at compile time. The default 30/30/20/20 split was chosen based on preliminary experiments balancing energy preservation with communication reliability. Adaptive weight tuning based on observed network conditions is identified as future work.

---

## Slide 14: Adaptive Timeout Mechanism

**Jacobson's RTT estimation (TCP, 1988):**

- SRTT ← 7/8 · SRTT + 1/8 · RTT_sample
- RTTVAR ← 3/4 · RTTVAR + 1/4 · |RTT_sample - SRTT|
- Timeout ← SRTT + 4 · RTTVAR
- Clamped to [150ms, 5000ms]

> **Speaker Notes:** PraSLE uses a fixed timeout T for each election round. If T is too short for current network conditions, nodes may time out before receiving messages from slow neighbors, causing false failure suspicions. If T is too long, actual failures go undetected. Our adaptive timeout mechanism replaces this fixed value with a dynamic one based on Jacobson's RTT estimation algorithm, which has been the standard approach in TCP since 1988. The mechanism tracks a smoothed round-trip time estimate and its variance using exponentially weighted moving averages. The timeout is then computed as the smoothed RTT plus four times the variance. In a stable network, the variance is small, leading to a tight timeout and fast failure detection. In a variable network with congestion or interference, the variance increases, automatically extending the timeout to avoid false alarms. We clamp the timeout to a range of 150 milliseconds to 5 seconds to prevent extreme values. The minimum ensures nodes have time to process messages, and the maximum prevents indefinite waiting when the network is partitioned.

---

## Slide 15: Controlled Leader Rotation and Backup Recovery

**Leader Handover Protocol:**
1. Leader detects energy below 20% critical threshold
2. Selects best successor from known candidates
3. Sends HANDOVER_REQ to successor
4. Successor responds with HANDOVER_ACK, assumes leader role
5. Old leader steps down; both broadcast updated leadership

**Safeguards:** 30s minimum leadership term, 3-second ACK timeout with 3 retries, fallback to next backup candidate.

**Backup Leader List:** Each node maintains top 3 ranked successors. On unexpected failure: promote backup without full K-round re-election. Full election runs in background to confirm.

*(Show Figure 5.4 from thesis: Controlled leader handover protocol)*

> **Speaker Notes:** When a leader's energy falls below a critical threshold of 20%, it proactively initiates a controlled handover rather than waiting to fail. The leader selects the best successor from its known candidates and sends a HANDOVER_REQ message. The successor responds with a HANDOVER_ACK, assumes the leader role, and both nodes broadcast the updated leadership to followers. There is a brief overlap period where both nodes consider themselves leader, but this is by design. The alternative of stepping down before confirmation would create a leaderless gap that could trigger unnecessary failure detection. The overlap is resolved within one message round-trip. Several safeguards prevent instability: a minimum leadership term of 30 seconds prevents rapid oscillation between candidates with similar scores, the acknowledgment has a 3-second timeout with up to 3 retries, and if the primary successor fails to respond, the next candidate from the backup list is tried. Speaking of the backup list, each node maintains a ranked list of the top 3 backup leader candidates based on observed scores during elections. When a leader fails unexpectedly without handover, followers can promote a known backup without running a full K-round re-election. A full election always runs in the background to confirm or correct the selection.

---

## Slide 16: Self-Stabilization Preservation

| Risk | Mitigation |
|---|---|
| Score instability | Computed once per cycle, held constant; EWMA smoothing |
| Timeout divergence | Bounds clamp values; reset cycle re-synchronizes |
| Handover oscillation | 30s minimum leadership term |
| Backup list corruption | Advisory only; full election runs in background |

**Reset cycles every R=3 election cycles clear all state.**

> **Speaker Notes:** A critical question is whether our extensions break PraSLE's self-stabilization guarantee. We identified four risks and addressed each. Score instability could prevent convergence if scores change mid-election, so we compute scores once at the start of each election cycle and hold them constant throughout, with EWMA smoothing to reduce noise in the underlying measurements. Timeout divergence could cause nodes to process rounds at different rates, so we clamp timeouts to bounded ranges and rely on the reset cycle to re-synchronize. Handover oscillation could occur if two nodes repeatedly hand off leadership, so we enforce a minimum 30-second leadership term. Backup list corruption could lead to promoting the wrong successor, so the backup list is advisory only and a full election always runs in the background to correct any incorrect selection. The key mechanism is the periodic reset cycle. Every R equals 3 election cycles, all nodes reset their state and run a fresh K-round election. This provides the same recovery guarantee as PraSLE's continuous unconditional broadcasting, but with lower average message rate because we use conditional broadcasting between resets.

---

## Slide 17: Conditional Broadcasting and Reset Cycles

**PraSLE:** Unconditional broadcast every round.
**Adaptive-PraSLE:** Broadcast only when values change. 74-92% fewer messages.

```
Cycle 1: Initial election
Cycle 2: Steady state (minimal traffic)
Cycle 3: RESET (all nodes reinitialize)
(repeats)
```

> **Speaker Notes:** Let me explain how conditional broadcasting works together with reset cycles. Standard PraSLE broadcasts its minimum-leader pair every single round regardless of whether values have changed. This is simple and robust but generates high message overhead. At 100 nodes, standard PraSLE sends 27,200 to 38,700 messages depending on topology. Adaptive-PraSLE instead broadcasts only when the minimum-leader pair actually changes. Once the best score has propagated and all nodes agree, message traffic drops to near zero during the steady state. This reduces total messages by 74 to 92 percent. The trade-off is that if a node's state becomes corrupted, the corruption will not be corrected until the sender's value changes again. To compensate, every third election cycle is a reset cycle where all nodes reinitialize their state and run a fresh election. So the pattern is: cycle 1 for initial election, cycle 2 for steady state with minimal traffic, cycle 3 for reset. This provides the same self-stabilization guarantee as continuous broadcasting. The worst-case recovery time is R times K times T, for example 6 seconds for a 10-node clique with R=3, K=2, and T=1 second.

---

## Slide 18: State Machine and Message Formats

**Five States:** INIT, ELECTION, NORMAL (follower), LEADER, RECOVERY

**Election Message:** 14 bytes (vs. PraSLE's 8 bytes)
- min_value, leader_id, sender_id, energy_level, avg_rssi, neighbor_count, flags, seq_num

**Handover Message:** 8 bytes

> **Speaker Notes:** The protocol is governed by a five-state machine. Nodes start in INIT for neighbor discovery, transition to ELECTION during active score propagation, and then reach either NORMAL as a follower or LEADER. The RECOVERY state handles fast recovery from the backup list when a leader failure is detected. The election message extends PraSLE's 8-byte format to 14 bytes by adding fields for energy level, average RSSI, neighbor count, status flags, and a sequence number. The status flags encode whether the sender is the current leader, has low energy, or has a handover pending. The additional 6 bytes enable all five extensions. Handover messages are smaller at 8 bytes since they only need to identify the old leader, proposed new leader, and a sequence number. While the per-message size increases by 75 percent, the total bytes transmitted actually decrease by 86 percent at 100 nodes due to the dramatic reduction in message count from conditional broadcasting.

---

# 5. Evaluation

---

## Slide 19: Implementation and Simulation Platform

- **Contiki-NG:** Lightweight IoT OS with protothreads, IPv6/6LoWPAN, Energest energy accounting
- **Cooja Simulator:** Cycle-accurate emulation, configurable radio models, deterministic experiments, fault injection
- Same platform as PraSLE, enabling direct comparison under identical conditions

> **Speaker Notes:** Before presenting the evaluation results, let me briefly describe the implementation and simulation platform. We selected Contiki-NG, a lightweight IoT operating system that provides protothreads for memory-efficient programming, a full IPv6 networking stack with 6LoWPAN, and the Energest module for software-based energy accounting. The Cooja simulator provides cycle-accurate MSP430 emulation, configurable radio propagation models including the Unit Disk Graph Medium used in our experiments, deterministic reproducible experiments through configurable random seeds, and programmable fault injection for systematic testing. We chose this platform over alternatives like RIOT or Zephyr because neither provides an equivalent integrated simulator. OMNeT++ was considered for its scalability but would require custom energy models. Contiki-NG was also the natural choice because PraSLE itself targets the same platform, enabling direct comparison under identical conditions.

---

## Slide 20: Experimental Methodology

- **Cooja simulator** with Contiki-NG, 6,400 simulation runs, 100 trials per configuration
- **10 variants:** Bully, Ring, PraSLE (4 topologies), Adaptive-PraSLE (4 topologies)
- **Experiments:** Convergence, message overhead, fault tolerance, noise (10/30/50% loss), partition
- **Fair comparison (Case 2):** All algorithms use T=2s

> **Speaker Notes:** All experiments were conducted using the Cooja network simulator with Contiki-NG. The evaluation comprises 6,400 individual simulation runs, with 100 trials per configuration using unique random seeds for statistical independence. We evaluated 10 algorithm variants: the classical Bully and Ring algorithms as baselines, standard PraSLE across four logical topologies (clique, line, ring, and mesh), and Adaptive-PraSLE across the same four topologies. The experiments cover convergence time, message overhead, fault tolerance after leader crash, noise resilience at 10, 30, and 50 percent packet loss, and network partition behavior. All algorithms use standardized timing parameters with T equals 2 seconds. This is important because in the original papers, PraSLE uses T=1 second while Bully and Ring use 2 seconds, giving PraSLE an unfair advantage from broadcasting twice as frequently. By equalizing the broadcast rate, measured differences reflect algorithmic efficiency rather than parameter tuning.

---

## Slide 21: Convergence Time Results

| Algorithm | 5 nodes | 10 nodes | 50 nodes | 100 nodes |
|---|---|---|---|---|
| Bully | 5.4s | 5.3s | 4.7s | 4.5s |
| Ring | 1.1s | 1.6s | 3.5s | 7.1s |
| PraSLE (clique) | 3.2s | 3.1s | 2.9s | 2.8s |
| **Adaptive-PraSLE** | **2.0s** | **1.9s** | **1.7s** | **1.6s** |

**44% faster than standard PraSLE.** *(Show Figure 6.1)*

> **Speaker Notes:** Starting with convergence time, which measures how long from network startup until all nodes agree on a single leader. Adaptive-PraSLE converges 44 percent faster than standard PraSLE, reaching 1.6 seconds at 100 nodes compared to 2.8 seconds. PraSLE and Adaptive-PraSLE both show consistent performance across all network sizes. Ring performs well at small scale but degrades significantly as the network grows, reaching 7.1 seconds at 100 nodes with high variance. Bully is stable at around 5 seconds regardless of network size, dominated by its fixed coordinator timeout. The speed advantage of Adaptive-PraSLE comes from conditional broadcasting. By sending messages only when values change, it reduces channel contention, which means the messages that are sent arrive faster. The convergence times here are dominated by the configured timeout values, which provide safety margins for real wireless networks. The relative differences between algorithms are more meaningful than the absolute values.

---

## Slide 22: Message Overhead Results

| Algorithm | 5 nodes | 100 nodes |
|---|---|---|
| PraSLE (clique) | 1,900 | 27,200 |
| **Adaptive-PraSLE (clique)** | **68** | **2,100** |

**92% reduction.** Total bytes: 217,600B vs 29,400B = **86% reduction.** *(Show Figure 6.2)*

> **Speaker Notes:** Message overhead directly impacts energy consumption because radio transmission is the dominant energy cost. Standard PraSLE generates the highest message count among all algorithms due to unconditional broadcasting. At 100 nodes with clique topology, it sends 27,200 messages. Adaptive-PraSLE with the same topology sends only 2,100 messages, a 92 percent reduction. Even accounting for the larger message size of 14 bytes versus 8 bytes, the total bytes transmitted drop from 217,600 to 29,400, an 86 percent reduction. The other Adaptive-PraSLE topologies (line, ring, mesh) show 74 percent reductions, sending 9,300 to 10,000 messages compared to standard PraSLE's 38,700. The topology differences arise because clique topology has K equals 2, meaning short election cycles where the minimum propagates to all neighbors simultaneously, while line topology requires K equals N rounds with hop-by-hop propagation.

---

## Slide 23: Fault Tolerance Results

| Algorithm | 5 nodes | 50 nodes | 100 nodes |
|---|---|---|---|
| Bully | 7.0s | 7.0s | 7.0s |
| Ring | 6.4s | 53s | 102s |
| PraSLE (clique) | 3.0s | 1.6s | 0.2s |
| **Adaptive-PraSLE** | 4.0s | **0.037s** | **0.25s** |

**Recovery improves with network size.** *(Show Figure 6.3)*

> **Speaker Notes:** For fault tolerance, we crash the leader node mid-simulation and measure recovery time until all surviving nodes agree on a new leader. An interesting finding is that recovery actually improves with network size for PraSLE variants. At 5 nodes, recovery takes about 3 seconds, essentially one full election cycle. But at 50 to 100 nodes, most topologies recover in under a second, with PraSLE-ring achieving 34 milliseconds at 50 nodes. This is because more nodes simultaneously detect the failure and participate in the reset cycle, allowing the minimum score to propagate faster through more entry points. Ring shows the opposite pattern: recovery degrades from 6.4 seconds at 5 nodes to 102 seconds at 100 nodes, with 13 to 21 percent of trials failing to recover entirely. Bully remains constant at 7 seconds, bounded by its timeout-based detection.

---

## Slide 24: Noise Resilience Results

**At 100 nodes:**
| Algorithm | 0% loss | 10% loss | 50% loss |
|---|---|---|---|
| Ring | 7.1s | **117s** (24% fail) | 105s |
| PraSLE | 2.8s | 2.8s | 2.8s |
| **Adaptive-PraSLE** | **1.6s** | **1.6s** | **1.6s** |

*(Show Figures 6.4-6.6)*

> **Speaker Notes:** Real IoT deployments experience significant packet loss. Our noise experiments test convergence under 10, 30, and 50 percent packet loss. The results are striking. Ring fails catastrophically: even 10 percent loss causes a 16x slowdown to 117 seconds at 100 nodes, with 24 percent of trials failing to converge entirely. This is because Ring uses sequential point-to-point forwarding. At 50 nodes with 10 percent loss, the probability that at least one of 50 sequential messages is lost is 1 minus 0.9 to the power of 50, which is 99.5 percent. So nearly every trial requires retransmission. PraSLE variants, by contrast, are virtually unaffected. Standard PraSLE's convergence times change by less than 0.4 percent even at 50 percent loss, and Adaptive-PraSLE varies by up to 3 percent. This is because the broadcast communication pattern gives every message a fresh chance each round, and the self-stabilizing mechanism retries automatically.

---

## Slide 25: Trade-off Analysis

| Algorithm | Convergence | Messages |
|---|---|---|
| Ring | 7.1s | 16,400 |
| Bully | 4.5s | 13,400 |
| PraSLE (clique) | 2.8s | 27,200 |
| **Adaptive-PraSLE (clique)** | **1.6s** | **2,100** |

*(Show Figure 6.8)*

> **Speaker Notes:** This trade-off chart plots message overhead against convergence time at 100 nodes. The ideal position is the bottom-left corner: fast convergence with few messages. Ring occupies the worst position with both the highest convergence time and 16,400 messages. Bully is moderate. Standard PraSLE converges quickly but generates the most messages of any algorithm due to unconditional broadcasting. Adaptive-PraSLE with clique topology occupies the best position: fastest convergence at 1.6 seconds with the fewest messages at 2,100. Based on this analysis, our recommendations are: for small networks under 10 nodes, any algorithm works and Bully is simplest; for large networks over 50 nodes, avoid Ring and use Adaptive-PraSLE with clique topology; for lossy environments, PraSLE variants are clearly superior; and for energy-constrained deployments, Adaptive-PraSLE minimizes total bytes transmitted.

---

## Slide 26: Answering the Research Questions

**RQ1:** 44% faster convergence, 74-92% fewer messages, sub-second recovery. Self-stabilization preserved.

**RQ2:** Outperforms all baselines. Ring fails under loss. Bully stable but no context awareness.

**RQ3:** +75% per-message size, but 86% reduction in total bytes. Fits Class 1 memory.

> **Speaker Notes:** Let me now directly answer the three research questions. For RQ1, the extensions yield 44 percent faster convergence at 1.6 seconds versus 2.8 seconds at 100 nodes, 74 to 92 percent fewer messages with an 86 percent reduction in total bytes, and sub-second fault recovery at scale. Self-stabilization is preserved through the periodic reset cycle mechanism. For RQ2, Adaptive-PraSLE outperforms all baselines in every metric we evaluated. Ring fails catastrophically under packet loss, reaching 117 seconds at 100 nodes with just 10 percent loss. Bully is stable but offers no context awareness for leader selection. Standard PraSLE performs well but has high message overhead from unconditional broadcasting. For RQ3, messages grow from 8 to 14 bytes, a 75 percent increase per message. However, the message count drops by 74 to 92 percent, resulting in an 86 percent reduction in total bytes transmitted at 100 nodes with clique topology. The additional per-node state for the backup list (3 entries) and EWMA counters fits within Class 1 device memory constraints.

---

# 6. Conclusion

---

## Slide 27: Scientific Contributions

**Contributions:**
- First protocol combining self-stabilization with energy-aware and link-quality-aware leader election
- Contiki-NG implementation with modular feature toggles
- Rigorous evaluation: 6,400 simulation runs, 10 algorithm variants

**Results:** 44% faster, 74-92% fewer messages, robust to 50% packet loss, sub-second recovery at scale

> **Speaker Notes:** To summarize the scientific contributions: we designed Adaptive-PraSLE, which is to the best of our knowledge the first protocol that combines self-stabilization with energy-aware and link-quality-aware leader election for constrained IoT networks. We implemented it in Contiki-NG with modular feature toggles that allow each extension to be independently enabled or disabled. And we conducted a rigorous evaluation across 6,400 simulation runs comparing 10 algorithm variants. The key results are 44 percent faster convergence than PraSLE, 74 to 92 percent fewer messages, virtual immunity to up to 50 percent packet loss, and sub-second fault recovery at scale with PraSLE-ring achieving 34 milliseconds at 50 nodes. We also demonstrated that classical algorithms, particularly Ring, are unsuitable for IoT deployments at scale.

---

## Slide 28: Threats to Validity

| Threat | Impact |
|---|---|
| Simulation vs. real hardware | UDGM simplifies real propagation |
| Uniform nodes | Heterogeneous hardware may differ |
| Static topology | Mobility not tested |
| Parameter sensitivity | Weights from preliminary experiments |
| Benign environment | No Byzantine model |

> **Speaker Notes:** Several factors limit the generalizability of these results. All experiments were conducted in Cooja, whose Unit Disk Graph radio model simplifies real-world propagation effects such as multipath fading and environmental interference. All nodes are identical, while real deployments often mix devices with different capabilities. All topologies are static, while many IoT and robotic applications involve mobile nodes. The scoring weights of 30/30/20/20 were chosen from preliminary experiments without a formal sensitivity analysis. And the protocol assumes a benign environment with no security or Byzantine fault model. Results should be validated on physical IoT hardware, and hardware testbed validation is identified as the most immediate next step.

---

## Slide 29: Future Work

- Hardware testbed validation on CC2538/CC26xx
- Mobile network support with dynamic neighbor discovery
- Adaptive scoring weight tuning
- Security extensions (HMAC, Byzantine tolerance)
- Heterogeneous network evaluation
- RPL integration (leader as DODAG root)

> **Speaker Notes:** Several directions remain open. The most immediate next step is deploying Adaptive-PraSLE on physical IoT motes and measuring performance under real radio conditions. Mobile network support would require dynamic neighbor discovery, faster reset cycles, and potentially incorporating position into the scoring function. Adaptive weight tuning through online learning could adjust the scoring weights based on observed network conditions. Security extensions including message authentication via HMAC and Byzantine fault tolerance would be necessary for adversarial environments. Evaluating with heterogeneous networks of mixed device capabilities is another open question. And integrating with the RPL routing protocol, where the elected leader serves as the DODAG root, could lead to a more unified IoT protocol stack.

---

## Slide 30: Q&A

**Thank you!**

**Questions?**

Source code: https://github.com/Pushpit07/PraSLE-Simulation-in-Contiki-NG-Cooja

> **Speaker Notes:** Thank you for your attention. I am happy to take any questions.

---

# Appendix

---

## Slide 31: What is self-stabilization?

- Dijkstra, 1974: recovery from any arbitrary initial state
- Convergence + Closure
- Handles transient faults, not persistent malicious behavior
- In Adaptive-PraSLE: reset cycle re-converges even from fully corrupted state

> **Speaker Notes:** Self-stabilization was introduced by Dijkstra in 1974. It means a system recovers to correct behavior from any arbitrary starting state. It satisfies convergence, meaning eventually reaching a legitimate state, and closure, meaning staying in legitimate states. Unlike Byzantine fault tolerance which targets malicious behavior, self-stabilization handles transient corruption. In our protocol, even if every node's state is completely corrupted, the next reset cycle will clear everything and re-converge to the correct leader.

---

## Slide 32: Why PraSLE as the base protocol?

- Lightweight: O(1) space, fits Class 0/1 devices
- Self-stabilizing: automatic recovery
- Extensible: modular design
- Alternatives: Bully/Ring not self-stabilizing, Paxos/Raft too heavy, LEACH/HEED not self-stabilizing

> **Speaker Notes:** PraSLE was selected because it combines self-stabilization with a lightweight design tailored to constrained IoT. It uses O(1) space per node and works on Class 0 and Class 1 devices. Its current limitations in energy awareness, link quality, and adaptive timing match exactly the gaps we aim to fill, making it both a solid foundation and an ideal target for extension. The alternatives were each unsuitable: Bully and Ring are not self-stabilizing, Paxos and Raft implement full consensus which is disproportionate, LEACH and HEED are energy-aware but not self-stabilizing, and population protocols assume pairwise interactions that do not match IoT broadcast communication.

---

## Slide 33: How does Jacobson's algorithm work?

- SRTT = 7/8 · SRTT + 1/8 · RTT_sample
- RTTVAR = 3/4 · RTTVAR + 1/4 · |RTT_sample - SRTT|
- RTO = SRTT + 4 · RTTVAR, clamped to [150ms, 5000ms]

> **Speaker Notes:** Jacobson's algorithm from 1988 maintains an exponentially weighted moving average of round-trip times and their variance. The timeout is the smoothed RTT plus four times the variance. In stable conditions the variance is small giving tight timeouts. In variable conditions the variance grows giving more conservative timeouts. We clamp to 150 milliseconds minimum and 5 seconds maximum.

---

## Slide 34: Why Case 2 (standardized T=2s)?

- Case 1: PraSLE broadcasts 2x faster than Bully/Ring (unfair advantage)
- Case 2: T=2s for all, equalizes broadcast rate
- Differences reflect algorithm design, not parameter tuning

> **Speaker Notes:** In Case 1 using original paper parameters, PraSLE uses T=1 second while Bully and Ring use 2-second heartbeats. This means PraSLE broadcasts twice as frequently, giving it a structural advantage from parameter tuning rather than algorithmic design. Case 2 standardizes T=2 seconds for all algorithms, equalizing the broadcast rate. This ensures that measured differences in convergence time and message overhead reflect inherent algorithmic efficiency.

---

## Slide 35: Why does Ring fail under packet loss?

- Sequential forwarding: one lost message stalls the entire election
- At 50 nodes, 10% loss: P(at least 1 lost) = 1-(0.9)^50 = 99.5%
- 13-36% of trials failed to converge
- PraSLE: broadcast gives fresh chance each round

> **Speaker Notes:** Ring passes election messages sequentially around a ring. If any single message in the chain is lost, the election stalls. At 50 nodes with 10 percent loss, the probability of at least one loss is 99.5 percent. At higher loss rates, cascading failures exceed the simulation timeout, causing 13 to 36 percent of trials to fail entirely. PraSLE does not have this problem because broadcast communication gives every message a fresh chance each round.

---

## Slide 36: What is the attacker model?

- Benign environment: all nodes honest
- Covers transient faults only
- Byzantine, Sybil, tampering out of scope
- Rationale: lightweight design goal conflicts with Byzantine overhead

> **Speaker Notes:** The thesis assumes a benign environment where all nodes are honest. We target transient faults: state corruption, message loss, node crashes, and link failures. We do not handle Byzantine faults, Sybil attacks, or message tampering. This is deliberate because adding Byzantine tolerance would significantly increase protocol complexity and message overhead, conflicting with the lightweight design goal for Class 0 and Class 1 devices. Security extensions are identified as future work.

---

## Slide 37: How does controlled handover work?

1. Leader energy < 20%
2. Select successor, send HANDOVER_REQ
3. Successor responds HANDOVER_ACK, assumes role
4. Old leader steps down, both broadcast

Brief overlap by design. 30s minimum term. 3 retries.

> **Speaker Notes:** When the leader's energy drops below 20 percent, it selects the best successor and sends a handover request. The successor acknowledges and assumes the leader role. The old leader receives the acknowledgment and steps down, and both broadcast the updated leadership. There is a brief overlap where both consider themselves leader, but this is preferred over a leaderless gap. It is resolved within one round-trip time. Safeguards include a 30-second minimum leadership term, 3 retries on the acknowledgment, and fallback to the next backup candidate.

---

## Slide 38: Why R=3 for reset cycles?

- R=2: no steady state, conditional broadcasting useless
- R=3: initial election + steady state + reset
- R>3: more efficient but slower corruption recovery
- Worst case: R × K × T = 6s (10-node clique)

> **Speaker Notes:** R=2 would mean resetting every other cycle, leaving no steady-state period and negating the benefit of conditional broadcasting entirely. R=3 gives one initial election cycle, one steady-state cycle with minimal traffic, and one reset cycle. Higher R values would increase steady-state efficiency but extend the window during which corrupted state could persist undetected. For a 10-node clique with T=1 second and K=2, worst-case recovery is 6 seconds. Finding the optimal R for different conditions is future work.

---

## Slide 39: Message size 14 vs 8 bytes. Justified?

- Per-message: +75%
- Message count: -74 to -92%
- Total bytes at 100 nodes: 217,600B to 29,400B = **86% reduction**

> **Speaker Notes:** The per-message cost is 75 percent higher, but the message count drops by 74 to 92 percent. At 100 nodes with clique topology, total bytes go from 217,600 for PraSLE to 29,400 for Adaptive-PraSLE, an 86 percent reduction. The 6 extra bytes carry energy level, RSSI, neighbor count, flags, and sequence number, enabling all five extensions.

---

## Slide 40: How does energy estimation work?

**Simulation:** Energest tracks TX/RX/CPU/sleep time. Duty cycle as proxy.
**Real hardware:** On-chip battery monitor, ADC, or external BMS. Same scoring function.

> **Speaker Notes:** In simulation, we use Contiki-NG's Energest module which tracks time spent in each power state. The radio duty cycle serves as a proxy for energy consumption, and the battery is drained proportionally. On real hardware, you would use actual battery monitoring through an on-chip battery monitor like the CC26xx's AON_BATMON, an ADC with a voltage divider, or an external battery management system. The scoring function itself remains identical; only the energy input source changes. This is a threat to validity since real battery discharge curves are non-linear and temperature-dependent.

---

## Slide 41: Topology effect on performance

- Convergence: topology-independent for PraSLE (all ~2.8s at 100 nodes)
- Messages: topology-dependent. Clique lowest (2,100), line highest (10,000)
- Recommendation: clique for minimum overhead

> **Speaker Notes:** Convergence time is topology-independent for standard PraSLE because the total cycle time equals T regardless of K. All topologies converge at about 2.8 seconds at 100 nodes. Message overhead is topology-dependent. Clique with K=2 generates the fewest messages because the minimum propagates to all neighbors simultaneously. Line with K=N generates the most because values must propagate hop-by-hop. For Adaptive-PraSLE, clique has the lowest message count at 2,100 but slightly slower convergence at 1.6 seconds compared to 1.5 seconds for other topologies, which generate 9,300 to 10,000 messages.

---

## Slide 42: FLP impossibility and this work

- FLP (1985): no deterministic consensus in asynchronous systems
- This work: partial synchrony, sidesteps FLP using timeouts
- Adaptive timeouts estimate the unknown delay bound dynamically

> **Speaker Notes:** Fischer, Lynch, and Paterson proved in 1985 that no deterministic algorithm can guarantee consensus in a purely asynchronous system if even one process may fail. Our work operates under partial synchrony, where messages are usually delivered within a bounded time but the bound may vary, and sidesteps FLP by using timeouts. The adaptive timeout mechanism directly addresses this by dynamically estimating the unknown delay bound rather than using a fixed value. This connects to Chandra and Toueg's failure detector theory, where the Omega detector, the weakest sufficient for consensus, amounts to eventual leader election.

---

## Slide 43: Could this work for robotic swarms?

- Static teams: applicable without modification
- Mobile swarms: need dynamic neighbor discovery, faster resets, position in scoring
- Controlled rotation valuable for battery-limited robots

> **Speaker Notes:** For static robot teams like those on a factory floor, the protocol is applicable without modification. For mobile swarms, several adaptations would be needed: dynamic neighbor discovery as nodes enter and leave range, faster reset cycles to adapt to topology changes, and potentially incorporating position or velocity into the scoring function. The controlled rotation mechanism would be especially valuable for battery-limited robots. The modular design with configurable weights and feature toggles supports this customization.

---

## Slide 44: Why no ablation study?

- Already 6,400 runs (~25 hours)
- Full ablation: 2^6 = 64 combinations
- Feature toggles implemented for future targeted studies

> **Speaker Notes:** The evaluation already includes 6,400 simulation runs requiring approximately 25 hours of compute time. A full ablation study with all 64 possible toggle combinations would multiply this significantly. The feature toggles are implemented and available for future targeted ablation. We focused on answering the three research questions by comparing the complete Adaptive-PraSLE against baselines. Isolating the backup list's individual contribution to fault recovery is identified as a specific gap for future work.

---

## Slide 45: PraSLE-ring vs. classical Ring algorithm

**Ring:** Sequential token passing. One lost message stalls everything. Terminates after one pass.

**PraSLE-ring:** Broadcast to ring neighbors every round. Lost messages re-sent next round. Continuous operation with resets.

> **Speaker Notes:** This is a common source of confusion. Both restrict communication to ring neighbors, but they work fundamentally differently. The classical Ring algorithm passes a single election token around sequentially. If any message is lost, the entire election stalls. It terminates after one pass and has no built-in recovery. PraSLE-ring uses broadcast communication where every node sends its best pair to both ring neighbors every round. If a message is lost, the same information is sent again next round. It runs continuously with periodic resets. The "ring" in PraSLE-ring refers to the logical topology, meaning which neighbors are accepted, not the communication pattern. This explains why Ring degrades to 117 seconds under 10 percent loss while PraSLE-ring stays at 2.8 seconds.

---

## Slide 46: How does the retry mechanism work?

**PraSLE:** Broadcasts every round unconditionally. No explicit retry; redundancy is structural. <0.4% variation at 50% loss.

**Adaptive-PraSLE:** Conditional broadcast. Recovery via reset cycles every 3 election cycles. Up to 3% variation at 50% loss.

> **Speaker Notes:** In standard PraSLE, there is no explicit retry mechanism. Every round, each node broadcasts its current pair regardless of whether values changed. If a broadcast is lost, the same information is sent again next round. The redundancy is built into the protocol's periodic structure, which is why PraSLE shows less than 0.4 percent convergence variation even at 50 percent packet loss. Adaptive-PraSLE works differently because it only broadcasts when values change. Lost messages are not immediately re-sent. Recovery comes from the reset cycle mechanism: every 3 election cycles, all nodes reset and trigger fresh broadcasts. This is slower than per-round redundancy, explaining the up to 3 percent variation at 50 percent loss.

---

## Slide 47: Why Bully and Ring as baselines?

- Canonical classical algorithms from textbooks
- Bully: fast, O(n^2) messages. Ring: simple, O(n·d) time.
- Demonstrate why IoT-specific design is necessary
- Strengthen external validity

> **Speaker Notes:** Bully and Ring are the canonical classical leader election algorithms covered in standard distributed systems textbooks and surveys. They span two ends of the complexity spectrum: Bully is fast but message-heavy, Ring is simple but slow. Both assume reliable communication and no energy constraints. Including them demonstrates why IoT-specific context-aware design is fundamentally necessary, and strengthens external validity beyond comparing only against PraSLE.

---

## Slide 48: Receiver-side vs. sender-side filtering

| | Broadcast + filter | Unicast per neighbor |
|---|---|---|
| Radio TX | 1 | K |
| Energy | Low | K times higher |

Wireless physical layer is broadcast regardless. Receiver filtering costs ~10-50 CPU cycles.

> **Speaker Notes:** We broadcast to all nodes and filter at the receiver rather than unicasting to each neighbor individually. In wireless networks, the physical layer is broadcast regardless, so all nodes in range receive the signal whether addressed to them or not. Broadcast requires one radio transmission per round, while unicast to K neighbors requires K transmissions. In a 10-node clique, that would be 9 transmissions instead of 1. Receiver-side filtering costs only about 10 to 50 CPU cycles for a linear search through a small neighbor array, which is negligible compared to the energy cost of additional radio transmissions.

---

## Slide 49: Why NullNet for Ring?

- Ring needs only single-hop to two neighbors. NullNet is sufficient.
- IPv6/RPL adds unnecessary routing overhead for a predetermined topology.
- Bully/PraSLE use IPv6 multicast for broadcast communication.
- Does not affect comparison fairness.

> **Speaker Notes:** The Ring algorithm uses NullNet, a minimal network layer in Contiki-NG, rather than the full IPv6 stack. Ring only needs to send messages to two neighboring nodes, and NullNet provides lightweight single-hop communication which is sufficient. Adding IPv6 with RPL would introduce unnecessary routing overhead for a topology that is predetermined. Bully and PraSLE use the full IPv6 stack because they rely on UDP multicast for broadcast communication. This implementation difference does not affect the fairness of the comparison because Ring's sequential token-passing behavior is faithfully implemented regardless of the network layer.

---

## Slide 50: Why leader election, not full consensus?

- Leader election is a narrower problem than consensus
- Consensus (Raft/Paxos): replicated logs, majority quorums, multi-round exchanges
- IoT only needs agreement on a single coordinator identity
- PraSLE: O(1) space per node, O(K·T) time, fits Class 0/1 devices

> **Speaker Notes:** Leader election is a narrower problem than consensus. Raft and Paxos implement full distributed state machines with replicated logs, majority quorums, and multiple message round-trips per decision. IoT devices only need to agree on a single coordinator identity, not total ordering or durable decisions. The overhead of consensus with persistent state, 2f+1 replicas, and multi-round exchanges is disproportionate for this narrower task. PraSLE achieves the same coordination goal with O(1) space per node and O(K times T) time, which fits the kilobytes of RAM and battery-powered reality of Class 0 and Class 1 IoT devices.

---

## Slide 51: Limitations of original PraSLE

1. **Context-blind selection:** picks leaders by node ID alone
2. **Fixed timeouts:** static T causes false suspicions or delayed detection
3. **Permanent leadership:** leader serves until failure, concentrating energy drain
4. **Full re-election on failure:** K full rounds before new leader
5. **High message overhead:** unconditional broadcasts every round

> **Speaker Notes:** Five limitations motivated the extensions. Context-blind selection means PraSLE picks leaders by node ID alone, so a node with nearly-depleted battery can become leader and fail shortly after. Fixed timeouts cause either false failure suspicions when too short or delayed detection when too long as network conditions change. Permanent leadership concentrates energy drain on one node. Full re-election on failure means K complete rounds must finish before a new leader is established. High message overhead comes from unconditional broadcasting every round even in stable networks.

---

## Slide 52: Position relative to LEACH and HEED

| | LEACH/HEED | PraSLE | Adaptive-PraSLE |
|---|---|---|---|
| Energy-aware | Yes | No | Yes |
| Self-stabilizing | No | Yes | Yes |
| Multi/single leader | Multiple cluster heads | Single leader | Single leader |
| Centralized | Base station | Decentralized | Decentralized |

> **Speaker Notes:** LEACH and HEED solve a different but related problem: cluster-head selection for data aggregation toward a base station. They produce multiple cluster heads and assume a centralized base station. Our work does single-leader election for flat, decentralized coordination. LEACH and HEED provide energy awareness but not self-stabilization. PraSLE provides self-stabilization without energy awareness. Adaptive-PraSLE combines both properties. LEACH's rotation concept influenced controlled leader rotation, and HEED's multi-criteria selection informed the composite scoring function.

---

## Slide 53: Preserving self-stabilization with extensions

Four risks addressed:
1. **Score instability:** scores fixed per cycle, EWMA smoothing
2. **Timeout divergence:** clamped to [150ms, 5s], reset cycle re-syncs
3. **Handover oscillation:** 30s minimum leadership term
4. **Backup list corruption:** advisory only; full election always runs in background

Key mechanism: periodic reset every R=3 cycles clears all state.

> **Speaker Notes:** Four risks to self-stabilization are addressed. Scores are computed at the start of each election cycle and held constant throughout, so the minimum-finding algorithm always converges, and EWMA smoothing reduces noise. Upper and lower bounds clamp corrupted timeouts to valid ranges, and the reset cycle re-synchronizes nodes. A minimum leadership term of 30 seconds prevents immediate re-handover. The backup list is advisory, not authoritative, and a full election always runs in the background to correct any incorrect selection. The periodic reset cycle every R=3 cycles clears all state and runs a fresh K-round election, providing the same recovery guarantee as original PraSLE.

---

## Slide 54: Conditional vs. unconditional broadcasting

| | Unconditional | Conditional |
|---|---|---|
| When | Every round | Only when (min, leader) changes |
| Messages (100 nodes) | 27,200-38,700 | 2,100-10,000 |
| Reduction | - | 74-92% |
| Trade-off | Redundancy corrects corrupted values | Reset cycles needed for correction |

> **Speaker Notes:** Unconditional broadcasting sends every round regardless of change. At 100 nodes, standard PraSLE sends 27,200 to 38,700 messages. Conditional broadcasting sends messages only when the minimum-leader pair changes, reducing this to 2,100 to 10,000 messages, a 74 to 92 percent reduction. The trade-off is that a corrupted value at a receiving node won't be corrected until the sender's value changes again. Periodic reset cycles every R=3 election cycles force all nodes to reinitialize and re-propagate, preserving the self-stabilization guarantee.

---

## Slide 55: Composite scoring function and weights

**Score = 0.3·Energy + 0.3·LinkQuality + 0.2·Connectivity + 0.2·CPU**

- Lower score = better candidate
- All components normalized to [0, 1]
- Energy and link quality weighted highest because they directly affect leader sustainability
- Weights configurable at compile time

> **Speaker Notes:** The score combines four normalized components: energy at 30 percent, link quality at 30 percent, connectivity at 20 percent, and CPU availability at 20 percent. Lower scores indicate better candidates. Energy and link quality are weighted highest because a leader needs sufficient battery to avoid premature failure and reliable links to communicate with followers. Connectivity and CPU are secondary factors. The 30/30/20/20 split was chosen based on preliminary experiments balancing energy preservation with communication reliability. The weights are configurable at compile time, and adaptive weight tuning is identified as future work.

---

## Slide 56: Why Contiki-NG and Cooja?

- Cooja provides: cycle-accurate MSP430 emulation, deterministic experiments, fault injection, Energest energy accounting
- Neither RIOT nor Zephyr has equivalent integrated simulator
- 6,400 simulation runs require controlled, reproducible conditions
- PraSLE also targets Contiki-NG, enabling direct comparison

> **Speaker Notes:** Contiki-NG ships with the Cooja network simulator, which provides cycle-accurate MSP430 emulation, deterministic reproducible experiments, programmable fault injection, and integrated energy accounting through the Energest module. Neither RIOT nor Zephyr provides an equivalent integrated simulator. For evaluating a distributed algorithm across 6,400 simulation runs with controlled conditions, this integration was necessary. PraSLE also targets constrained IoT devices on Contiki-NG, so implementing both in the same environment enabled direct comparison under identical conditions.

---

## Slide 57: Would convergence times hold on real hardware?

- Absolute times (1.6-7.1s) reflect conservative timeout safety margins
- Actual message delivery in Cooja is millisecond-scale
- **Relative** differences between algorithms are more meaningful
- Key finding holds: PraSLE = O(K·T) regardless of size; Ring degrades to 100+s under loss
- Reducing T proportionally reduces all times, preserving relative ordering

> **Speaker Notes:** The absolute convergence times of 1.6 to 7.1 seconds reflect conservative safety margins built into the timeout parameters. Actual message delivery in Cooja is millisecond-scale. The relative differences between algorithms are more meaningful than absolute values. On real hardware with actual packet loss and interference, the safety margins become necessary. The key finding holds: PraSLE variants converge in O(K times T) regardless of network size, while Ring degrades to over 100 seconds under packet loss. Reducing T would proportionally reduce all convergence times while preserving the relative ordering.

---

## Slide 58: Why does Adaptive-PraSLE converge faster?

- Speed comes from conditional broadcasting, not despite extra computation
- Standard PraSLE: unconditional broadcasts cause redundant messages even after convergence
- Adaptive-PraSLE: stops sending when values stabilize, reducing channel contention
- Score computation overhead is negligible compared to time saved

> **Speaker Notes:** The speed advantage comes from conditional broadcasting, not despite the additional computation. Standard PraSLE broadcasts unconditionally every round, meaning nodes keep processing redundant messages even after convergence. Adaptive-PraSLE only sends messages when values change, so once the minimum score propagates, message traffic drops to near zero. Less channel contention means messages arrive faster in subsequent rounds. The composite score computation adds negligible overhead compared to the time saved by avoiding redundant broadcasts.

---

## Slide 59: Fault recovery improves with network size

- More nodes detect leader failure simultaneously
- More participants broadcasting means the minimum score propagates faster
- 5-10 nodes: recovery around 3s (one full election cycle)
- 50-100 nodes: recovery under 1s (PraSLE-ring: 34ms at 50 nodes)

> **Speaker Notes:** At larger network sizes, more nodes simultaneously detect the leader failure and participate in the reset cycle. With more participants broadcasting, the minimum score propagates faster because it has more entry points into the network. At 5 to 10 nodes, recovery takes about 3 seconds, one full election cycle. At 50 to 100 nodes, most topologies recover in under 1 second. PraSLE-ring achieves 34 milliseconds at 50 nodes. The protocol gets better at recovery as the network grows.

---

## Slide 60: Threats to validity

1. **Simulation vs. real hardware:** UDGM simplifies real propagation
2. **Uniform nodes:** homogeneous hardware assumed
3. **Static topology:** no mobility tested
4. **Parameter sensitivity:** weights from preliminary experiments, no formal sensitivity analysis
5. **Increased complexity:** larger messages and state may not suit all deployments

> **Speaker Notes:** Cooja's UDGM radio model simplifies real-world propagation effects like multipath fading and environmental interference. All experiments assume homogeneous hardware, and heterogeneous networks may behave differently. No mobility was tested, though many IoT and robotic applications involve mobile nodes. The scoring weights of 30/30/20/20 were chosen from preliminary experiments without formal sensitivity analysis. The increased complexity from larger messages and additional state may not be favorable in all deployment scenarios.

---

## Slide 61: Handling security threats

- Current: benign environment, no authentication
- Malicious node could falsify score to become leader
- Self-stabilization provides *some* resilience (recovery after transient attacks)
- Persistent attacker can permanently disrupt elections
- Future: HMAC with pre-shared keys, neighbor attestation, Byzantine extensions

> **Speaker Notes:** The protocol assumes a benign environment. A malicious node could falsify a low score to become leader or broadcast incorrect values to disrupt elections. Self-stabilization provides some resilience because if an attacker temporarily corrupts state, the next reset cycle will re-converge. A persistent attacker who continuously injects false values can permanently disrupt elections. Future work includes message authentication using HMAC with pre-shared keys, score verification through neighbor attestation, and Byzantine fault-tolerant extensions. The challenge is keeping overhead low enough for constrained devices.

---

## Slide 62: Direct answers to research questions

- **RQ1:** 44% faster convergence (1.6s vs 2.8s), 74-92% fewer messages, sub-second fault recovery. Self-stabilization preserved via reset cycles.
- **RQ2:** Adaptive-PraSLE outperforms all baselines. Ring fails under loss (117s). Bully fixed at ~5s. PraSLE solid but high overhead.
- **RQ3:** Messages +75% per packet, but total bytes -86%. Additional state fits Class 1 memory.

> **Speaker Notes:** RQ1 asks about the impact of extensions on convergence, stability, and lifetime. The extensions yield 44 percent faster convergence at 1.6 seconds versus 2.8 seconds at 100 nodes, 74 to 92 percent fewer messages, and sub-second fault recovery at scale. Self-stabilization is preserved through periodic reset cycles. RQ2 asks about performance versus baselines. Adaptive-PraSLE consistently outperforms all baselines. Ring fails under packet loss with 117 seconds at 100 nodes with 10 percent loss. Bully is stable but fixed at about 5 seconds regardless of conditions. RQ3 asks about resource overhead versus gains. Messages grow from 8 to 14 bytes per packet, but total bytes transmitted decrease by 86 percent at 100 nodes in clique topology. The additional state for backup list entries, EWMA counters, and scoring history fits within Class 1 device memory constraints.

