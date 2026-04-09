# Thesis Defense Preparation: Potential Questions & Answers

## 1. Motivation & Problem Statement

### Q1: Why did you choose leader election specifically, rather than full consensus (e.g., Raft/Paxos) for IoT networks?
**A:** Leader election is a narrower problem than consensus. Consensus protocols like Raft and Paxos implement full distributed state machines with replicated logs, majority quorums, and multiple message round-trips per decision. IoT devices only need to agree on a single coordinator identity -- they don't need total ordering or durable decisions. The overhead of consensus (persistent state, 2f+1 replicas, multi-round exchanges) is disproportionate for this narrower task. PraSLE achieves the same coordination goal with O(1) space per node and O(K*T) time, which fits the kilobytes-of-RAM, battery-powered reality of Class 0/1 IoT devices.

### Q2: What are the practical limitations of the original PraSLE that motivated your extensions?
**A:** Five specific limitations: (1) **Context-blind selection** -- PraSLE picks leaders by node ID alone, so a node with nearly-depleted battery can become leader and fail shortly after. (2) **Fixed timeouts** -- the static T parameter causes either false failure suspicions (too short) or delayed detection (too long) as network conditions change. (3) **Permanent leadership** -- a leader serves until failure, concentrating energy drain on one node. (4) **Full re-election on failure** -- K full rounds must complete before a new leader is established. (5) **High message overhead** -- unreliable mode broadcasts every round unconditionally even in stable networks.

### Q3: How does your work position itself relative to energy-aware clustering protocols like LEACH and HEED?
**A:** LEACH and HEED solve a different but related problem: cluster-head selection for data aggregation toward a base station. They produce multiple cluster heads and assume a centralized base station. My work does single-leader election for flat, decentralized coordination. The key gap is that LEACH/HEED provide energy awareness but not self-stabilization -- if state becomes corrupted, they cannot recover without restarting. Conversely, PraSLE provides self-stabilization without energy awareness. Adaptive-PraSLE bridges this gap by combining both properties. I did draw design inspiration from these protocols: LEACH's rotation concept influenced controlled leader rotation, and HEED's multi-criteria selection informed the composite scoring function.

---

## 2. Technical Design & Self-Stabilization

### Q4: How do you preserve PraSLE's self-stabilization guarantee when you add the five extensions?
**A:** Four specific risks are addressed: (1) **Score instability** -- scores are computed at the start of each election cycle and held constant throughout, so the minimum-finding algorithm always converges. EWMA smoothing reduces noise. (2) **Timeout divergence** -- upper and lower bounds clamp corrupted timeouts to valid ranges, and the reset cycle re-synchronizes nodes. (3) **Handover oscillation** -- a minimum leadership term of 30 seconds prevents immediate re-handover. (4) **Backup list corruption** -- the backup list is advisory, not authoritative; a full election always runs in the background to correct any incorrect selection. The key mechanism is the periodic reset cycle: every R=3 cycles, all nodes reset state and run a fresh K-round election, providing the same recovery guarantee as PraSLE's continuous broadcasting.

### Q5: Why did you choose conditional broadcasting over PraSLE's unconditional broadcasting?
**A:** Unconditional broadcasting (sending every round regardless of change) generates significant overhead -- at 100 nodes, standard PraSLE sends 27,200-38,700 messages. Conditional broadcasting sends messages only when the (min, leader) pair changes, reducing this to 2,100-10,000 messages (74-92% reduction). The trade-off is that a corrupted value at a receiving node won't be corrected until the sender's value changes again. To compensate, periodic reset cycles (every R=3 election cycles) force all nodes to reinitialize and re-propagate, preserving the self-stabilization guarantee.

### Q6: Explain your composite scoring function. Why those specific weights (30/30/20/20)?
**A:** The score combines four normalized components: energy (30%), link quality (30%), connectivity (20%), and CPU availability (20%). Lower scores = better candidates. Energy and link quality are weighted highest because they most directly affect whether a leader can sustain its role: a leader needs sufficient battery to avoid premature failure and reliable links to communicate with followers. Connectivity and CPU are secondary factors. The specific 30/30/20/20 split was chosen based on preliminary experiments balancing energy preservation with communication reliability. The weights are configurable at compile time, and adaptive weight tuning is identified as future work.

### Q7: How does the adaptive timeout mechanism work, and why did you base it on Jacobson's TCP algorithm?
**A:** The mechanism tracks message round-trip times using Jacobson's RTT estimation: SRTT = (7/8)*SRTT + (1/8)*RTT_sample, RTTVAR = (3/4)*RTTVAR + (1/4)*|RTT_sample - SRTT|, Timeout = SRTT + 4*RTTVAR. When network conditions are stable, RTTVAR is small and timeouts are tight, enabling fast failure detection. When conditions are variable (congestion, interference), RTTVAR increases, extending timeouts to avoid false suspicions. I chose Jacobson's algorithm because it has been proven effective for decades in TCP for exactly this problem -- balancing fast detection against false positives in variable-latency networks. The timeout is clamped to [150ms, 5000ms] to prevent extreme values.

### Q8: What happens during the brief "overlap period" in your controlled handover protocol?
**A:** Between step 4 (successor sends HANDOVER_ACK and assumes leader role) and step 5 (old leader receives ACK and steps down), both nodes may briefly consider themselves leader. This is by design -- the alternative of stepping down *before* confirmation would create a leaderless gap, potentially triggering unnecessary failure detection and re-elections across the network. The overlap is resolved within one message round-trip (milliseconds in practice), and PraSLE's self-stabilization guarantees eventual convergence to a single leader regardless. Safeguards include: minimum 30-second leadership term (prevents oscillation), 3-second ACK timeout with up to 3 retries, and fallback to backup candidates if the primary successor fails.

---

## 3. Implementation

### Q9: Why did you choose Contiki-NG and Cooja over alternatives like RIOT or Zephyr?
**A:** The distinguishing factor is simulation support. Contiki-NG ships with the Cooja network simulator, which provides cycle-accurate MSP430 emulation, deterministic reproducible experiments, programmable fault injection, and integrated energy accounting through the Energest module. Neither RIOT nor Zephyr provides an equivalent integrated simulator. For evaluating a distributed algorithm across 6,400 simulation runs with controlled conditions, this integration was essential. Additionally, PraSLE (the base protocol) also targets constrained IoT devices, so implementing both in Contiki-NG enabled direct comparison under identical conditions.

### Q10: How does your energy estimation work in simulation, and would it be different on real hardware?
**A:** In simulation, I use Contiki-NG's Energest module, which tracks time spent in each power state (TX, RX, CPU active, sleep). The radio duty cycle (TX+RX time / total time) serves as a proxy for energy consumption, and the simulated battery is drained proportionally. On real hardware, you would need actual battery monitoring -- either an on-chip battery monitor (e.g., CC26xx's AON_BATMON), an ADC with a voltage divider, or an external BMS via I2C/UART. The scoring function itself would remain the same; only the energy input source changes. This is a threat to validity -- real battery discharge curves are non-linear and temperature-dependent.

### Q11: Why did you use UDP multicast (ff02::1) for all communication rather than unicast?
**A:** One multicast transmission reaches all neighbors with a single radio operation. Unicast to k neighbors would require k separate transmissions, each consuming energy. Since wireless transmissions are inherently broadcast anyway (all nodes in range receive them), multicast exploits this physical reality. Logical topology constraints are enforced at the receiver side -- each node filters messages from non-logical-neighbors. This keeps the communication code topology-independent while being energy-efficient.

---

## 4. Evaluation & Results

### Q12: Why did you choose Case 2 (standardized parameters T=2s) over Case 1 (original paper parameters) for your main evaluation?
**A:** In Case 1, PraSLE uses T=1s while Bully/Ring use a heartbeat interval of 2s, meaning PraSLE broadcasts twice as frequently. This gives PraSLE a structural advantage in convergence speed that comes from parameter tuning, not algorithmic design. By standardizing T=2s for all algorithms in Case 2, the broadcast rate is equalized. This means measured differences in convergence time, message overhead, and fault recovery reflect inherent algorithmic efficiency rather than how aggressively each protocol communicates.

### Q13: Your convergence times are dominated by timeout values (seconds). Would these results hold on real hardware?
**A:** The absolute convergence times (1.6-7.1 seconds) reflect conservative safety margins built into the timeout parameters -- actual message delivery in Cooja is millisecond-scale. The *relative* differences between algorithms are more meaningful than absolute values. On real hardware with actual packet loss and interference, the safety margins become necessary. The key finding holds: PraSLE variants converge in O(K*T) regardless of network size, while Ring degrades to 100+ seconds under packet loss. Reducing T would proportionally reduce all convergence times while preserving the relative ordering.

### Q14: How do you explain that Adaptive-PraSLE converges faster (1.6s) than standard PraSLE (2.8s) despite additional computation?
**A:** The speed advantage comes from conditional broadcasting, not despite the additional computation. Standard PraSLE broadcasts unconditionally every round, which means nodes keep processing redundant messages even after convergence. Adaptive-PraSLE only sends messages when values change, so once the minimum score propagates, message traffic drops to near zero. Less channel contention means messages arrive faster in subsequent rounds. The composite score computation adds negligible overhead compared to the time saved by avoiding redundant broadcasts.

### Q15: Ring failed to converge in 13-36% of trials under packet loss. Can you explain this failure mode?
**A:** Ring uses sequential point-to-point forwarding: each node passes the election message to its successor in the ring. If any single message in this chain is lost, the entire election stalls until a timeout triggers a retry, which may also be lost. Under 10% packet loss at 50 nodes, the probability that at least one of 50 sequential messages is lost is 1-(0.9)^50 = 99.5%. So almost every trial requires retransmission. At higher loss rates, cascading failures cause the algorithm to exceed the simulation timeout entirely. PraSLE variants don't have this problem because broadcast communication gives every message a fresh chance each round, and the self-stabilizing mechanism retries automatically.

### Q16: Why does fault recovery actually *improve* with network size for PraSLE variants?
**A:** At larger network sizes, more nodes simultaneously detect the leader failure and participate in the reset cycle. With more participants broadcasting simultaneously, the minimum score propagates faster because it has more "entry points" into the network. At 5-10 nodes, recovery takes 3 seconds (essentially one full election cycle). At 50-100 nodes, most topologies recover in under 1 second -- PraSLE-ring achieves 34ms at 50 nodes. This is a desirable scalability property: the protocol gets *better* at recovery as the network grows.

### Q17: Adaptive-PraSLE messages are 14 bytes vs. PraSLE's 8 bytes. Doesn't this negate the message reduction?
**A:** The per-message cost is 75% higher (14 vs 8 bytes), but the message *count* is reduced by 74-92%. At 100 nodes with clique topology: Standard PraSLE sends 27,200 messages * 8 bytes = 217,600 bytes total. Adaptive-PraSLE sends 2,100 messages * 14 bytes = 29,400 bytes total. That's an 86% reduction in total bytes transmitted. The additional 6 bytes per message (energy level, RSSI, neighbor count, flags, sequence number) are well justified by the dramatic reduction in total traffic.

---

## 5. Limitations & Future Work

### Q18: What are the main threats to the validity of your results?
**A:** Five main threats: (1) **Simulation vs. real hardware** -- Cooja's UDGM radio model simplifies real-world propagation effects like multipath fading and environmental interference. (2) **Uniform nodes** -- all experiments assume homogeneous hardware; heterogeneous networks may behave differently. (3) **Static topology** -- no mobility was tested, though many IoT/robotic applications involve mobile nodes. (4) **Parameter sensitivity** -- the scoring weights (30/30/20/20) were chosen from preliminary experiments without formal sensitivity analysis. (5) **Increased complexity** -- larger messages and additional state may not be favorable in all deployment scenarios.

### Q19: How would you handle security threats (e.g., a malicious node falsifying its score)?
**A:** The current protocol assumes a benign environment. A malicious node could falsify a low score to become leader or broadcast incorrect values to disrupt elections. The self-stabilizing property provides *some* resilience against transient attacks (the protocol eventually recovers), but persistent attackers require explicit defenses. Future work would include message authentication (e.g., HMAC with pre-shared keys), score verification through neighbor attestation, and potentially Byzantine fault tolerance. The challenge is keeping overhead low enough for constrained devices.

### Q20: How would your protocol work in a mobile network where nodes move and topology changes?
**A:** The current design targets static/quasi-static networks. For mobility, several adaptations would be needed: (1) dynamic neighbor discovery as nodes enter/leave range, (2) faster reset cycles to adapt to topology changes, (3) potentially incorporating position or velocity into the scoring function. The adaptive timeout mechanism would help in mobile scenarios since it naturally tracks changing network latency. However, fundamental questions remain: how to set K when the diameter changes, and how to handle leader isolation when the leader moves out of range. This is explicitly identified as future work.

### Q21: Why didn't you perform an ablation study to isolate each extension's individual contribution?
**A:** The evaluation compares 10 algorithm variants (4 PraSLE topologies + 4 Adaptive-PraSLE topologies + Bully + Ring) across 4 network sizes, 4 experiment types, and 100 trials each -- already 6,400 simulation runs requiring ~25 hours. A full ablation study with all 2^6 = 64 toggle combinations would multiply this significantly. I focused on comparing the complete Adaptive-PraSLE against baselines to answer the research questions. The feature toggles are implemented and available for targeted ablation; in particular, isolating the backup list's contribution to fault recovery is identified as future work.

---

## 6. Research Questions (Direct Answers)

### Q22: Can you directly answer your three research questions?
**A:**
- **RQ1** (Impact of extensions on convergence, stability, lifetime): The extensions yield 44% faster convergence (1.6s vs 2.8s at 100 nodes), 74-92% fewer messages, and sub-second fault recovery at scale. Self-stabilization is preserved through periodic reset cycles. Energy-aware scoring and controlled rotation prevent context-blind leader selection.
- **RQ2** (Performance vs. baselines): Adaptive-PraSLE consistently outperforms all baselines. Ring fails catastrophically under packet loss (117s at 100 nodes with 10% loss). Bully is stable but fixed at ~5s regardless of conditions. Standard PraSLE is solid but has high message overhead from unconditional broadcasting.
- **RQ3** (Resource overhead vs. gains): Messages grow from 8 to 14 bytes (+75%), and per-node state increases for backup tracking and scoring history. However, total bytes transmitted decrease by 86% (clique, 100 nodes). The additional state (backup list of 3 entries, EWMA counters) fits within Class 1 device memory constraints.

---

## 7. Broader/Conceptual Questions

### Q23: What is self-stabilization, and why is it particularly important for IoT?
**A:** Self-stabilization, introduced by Dijkstra in 1974, means a system recovers to correct behavior from *any* arbitrary starting state. It satisfies two properties: convergence (eventually reaches a legitimate state) and closure (stays legitimate as long as no further faults occur). For IoT, this is critical because devices operate unattended in environments where transient faults are common -- power glitches, memory corruption from interference, unexpected reboots. Unlike traditional fault tolerance that targets specific failure models (crash, Byzantine), self-stabilization handles *any* transient corruption without manual intervention. In my protocol, even if all nodes' state is completely corrupted, the next reset cycle will re-converge to a correct leader.

### Q24: How does your work relate to the FLP impossibility result?
**A:** Fischer, Lynch, and Paterson proved that no deterministic algorithm can guarantee consensus in a purely asynchronous system if even one process may fail. Leader election is closely related to consensus. My work operates under partial synchrony (messages usually delivered within bounded time, but the bound may vary), which sidesteps FLP by using timeouts. The adaptive timeout mechanism explicitly addresses the gap between theoretical asynchrony and practical partial synchrony -- it dynamically estimates the unknown message delay bound rather than using a fixed value.

### Q25: Could your approach be applied to robotic swarms specifically? What would change?
**A:** The protocol is designed with robotic clusters in mind (it's in the title). For static robot teams (e.g., factory floor), it works as-is. For mobile swarms, the scoring function could incorporate position (centrality), velocity, or mission-specific criteria. The controlled rotation mechanism would be valuable for battery-limited robots. Key changes needed: dynamic neighbor discovery, faster adaptation to topology changes (lower R), and potentially multi-leader election for large swarms. The modular design (configurable weights, feature toggles) supports this customization.

### Q26: What is the attacker model / security scope of this thesis?
**A:** The thesis assumes a **benign environment** -- all nodes are honest and follow the protocol correctly. The fault model targets **transient faults** only: power glitches that corrupt protocol variables, message loss due to interference or collisions, unexpected node reboots into arbitrary states, and temporary link failures. These are the dominant failure modes in real IoT deployments. The protocol does **not** handle: (1) **Byzantine faults** -- malicious nodes that intentionally falsify scores, forge messages, or deviate from the protocol. A node could trivially claim a low composite score to force its own election. (2) **Permanent hardware failures** -- e.g., persistent bit-flips in program code, which require orthogonal mechanisms like code redundancy or watchdog timers. (3) **Sybil attacks** -- a node impersonating multiple identities. (4) **Eavesdropping or message tampering** -- no encryption or authentication is used. Self-stabilization provides *some* resilience because even if an attacker temporarily corrupts state, the next reset cycle will re-converge. But a persistent attacker who continuously injects false values can permanently disrupt elections. Addressing this would require message authentication (e.g., HMAC with pre-shared keys), score attestation by neighbors, or Byzantine fault-tolerant extensions -- all identified as future work. The decision to exclude security was deliberate: the thesis focuses on the algorithmic and systems contribution (context-aware self-stabilizing leader election), and adding Byzantine tolerance would significantly increase protocol complexity and message overhead, which conflicts with the lightweight design goal for Class 0/1 devices.

### Q27: Why is the reset cycle parameter R=3? What's the sensitivity?
**A:** R=3 gives: Cycle 1 = initial election, Cycle 2 = one steady-state cycle with conditional broadcasting, Cycle 3 = reset. R=2 would leave no steady-state cycle, negating conditional broadcasting entirely. Higher R would increase steady-state efficiency but extend the window where corrupted state persists undetected. For a 10-node clique with T=1s and K=2, worst-case recovery is R*K*T = 6 seconds. Finding optimal R for different conditions is identified as future work. An adaptive R (increase when stable, decrease when failures detected) would be an improvement.
