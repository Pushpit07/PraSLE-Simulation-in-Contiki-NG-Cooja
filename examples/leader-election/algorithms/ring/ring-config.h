/*
 * Copyright (c) 2024, TU Dresden
 * All rights reserved.
 *
 * Ring Leader Election Algorithm Configuration
 *
 * This file defines the configuration parameters, message types, state machine,
 * and message structure for the Ring leader election algorithm.
 */

#ifndef RING_CONFIG_H_
#define RING_CONFIG_H_

#include "project-conf.h"

/*---------------------------------------------------------------------------*/
/* TIMING CONFIGURATION - Tuned for Ring Algorithm in Wireless Networks */
/*---------------------------------------------------------------------------*/
/*
 * Two timing profiles are available:
 * - Normal mode (default): Conservative timeouts for real wireless networks
 * - Fast mode (RING_FAST_MODE=1): Reduced timeouts for quick testing/simulation
 *
 * To enable fast mode: make ALGORITHM=ring TARGET=cooja FAST_MODE=1
 *
 * TIMING RATIONALE FOR RING ALGORITHM:
 * ====================================
 * Unlike Bully (broadcast-based), Ring messages must traverse the entire ring
 * sequentially. This means timeouts must account for:
 *   - N message hops (where N = ring size)
 *   - Processing time at each node
 *   - Wireless network delays per hop
 *
 * Therefore, Ring timeouts are generally longer than Bully timeouts.
 */

#ifdef RING_FAST_MODE
/*---------------------------------------------------------------------------*/
/* FAST MODE: Reduced timeouts for testing (~1-2 second convergence) */
/*---------------------------------------------------------------------------*/

#ifndef ELECTION_TIMEOUT
#define ELECTION_TIMEOUT    (1 * CLOCK_SECOND)
#endif

/**
 * COORDINATOR_TIMEOUT: How long to wait before declaring coordinator dead
 * - Set to 3 seconds = 3x ALIVE_INTERVAL (tolerates 2 missed heartbeats + delay)
 */
#ifndef COORDINATOR_TIMEOUT
#define COORDINATOR_TIMEOUT (3 * CLOCK_SECOND)
#endif

/**
 * ALIVE_INTERVAL: How often coordinator sends ALIVE heartbeat messages
 * - Set to 1 second for fast failure detection in simulations
 */
#ifndef ALIVE_INTERVAL
#define ALIVE_INTERVAL      (1 * CLOCK_SECOND)
#endif

#ifndef RANDOM_DELAY_MAX
#define RANDOM_DELAY_MAX    (1 * CLOCK_SECOND)
#endif

#else /* Normal mode */
/*---------------------------------------------------------------------------*/
/* NORMAL MODE: Conservative timeouts for real wireless networks             */
/*---------------------------------------------------------------------------*/
/*
 * RING ALGORITHM REFERENCE (Chang-Roberts):
 * E. Chang and R. Roberts, "An improved algorithm for decentralized
 * extrema-finding in circular configurations of processes,"
 * Communications of the ACM, vol. 22, no. 5, pp. 281-283, May 1979.
 * DOI: 10.1145/359104.359108
 *
 * ORIGINAL PAPER TIMING MODEL:
 * The Chang-Roberts algorithm assumes synchronous, reliable communication.
 * Message complexity: O(N²) worst case, O(N log N) average case.
 * Election requires 2N-1 to 3N-1 messages in worst case.
 *
 * For practical implementation, timeouts must account for:
 *   - Message must traverse entire ring (N hops)
 *   - Each hop has delay T (max single-hop message time)
 *   - Total ring traversal: N × T
 *
 * TIMING ADAPTATION FOR IoT/WSN:
 * For 802.15.4-based wireless sensor networks:
 *   - T (single-hop delay) = 50ms typical, 200ms worst case
 *   - For N = 10 nodes: election timeout = 10 × 200ms = 2 seconds
 *   - For N = 100 nodes: election timeout = 100 × 200ms = 20 seconds
 *   - We use 5 seconds as balance for typical network sizes (10-50 nodes)
 *   - Heartbeat interval = 2 seconds (same as Bully for fair comparison)
 */

/**
 * ELECTION_TIMEOUT: How long to wait for election to complete
 * - Chang-Roberts: Election message must traverse entire ring
 * - Worst case: 3N-1 messages, but with parallel forwarding: ~N × T
 * - Set to 5 seconds (supports up to ~25 nodes with 200ms/hop)
 * - For larger networks, this should scale with RING_SIZE
 */
#ifndef ELECTION_TIMEOUT
#define ELECTION_TIMEOUT    (5 * CLOCK_SECOND)
#endif

/**
 * COORDINATOR_TIMEOUT: How long to wait before declaring coordinator dead
 * - Set to 2T = 4 seconds (matches Bully for fair comparison)
 * - Tolerates one missed heartbeat in lossy networks
 */
#ifndef COORDINATOR_TIMEOUT
#define COORDINATOR_TIMEOUT (4 * CLOCK_SECOND)
#endif

/**
 * ALIVE_INTERVAL: How often coordinator sends ALIVE heartbeat messages
 * - Set to T = 2 seconds (matches Bully for fair comparison)
 * - Heartbeat circulates around the ring
 */
#ifndef ALIVE_INTERVAL
#define ALIVE_INTERVAL      (2 * CLOCK_SECOND)
#endif

/**
 * RANDOM_DELAY_MAX: Random startup delay to prevent synchronized elections
 * - Prevents multiple nodes starting election simultaneously
 * - Set to T = 2 seconds
 */
#ifndef RANDOM_DELAY_MAX
#define RANDOM_DELAY_MAX    (2 * CLOCK_SECOND)
#endif

#endif /* RING_FAST_MODE */

/*---------------------------------------------------------------------------*/
/* RING TOPOLOGY CONFIGURATION */
/*---------------------------------------------------------------------------*/
/**
 * RING_SIZE: Number of nodes in the logical ring
 *
 * TOPOLOGY:
 * - Nodes are numbered 1 to RING_SIZE
 * - Ring structure: 1 -> 2 -> 3 -> ... -> RING_SIZE -> 1
 * - Each node knows its successor: next_node = (node_id % RING_SIZE) + 1
 *
 * IMPORTANT:
 * - This must match the number of motes in your Cooja simulation
 * - Can be overridden in project-conf.h
 */
#ifndef RING_SIZE
#define RING_SIZE 10
#endif

/*---------------------------------------------------------------------------*/
/* MESSAGE TYPES - Ring Algorithm Messages */
/*---------------------------------------------------------------------------*/
/*
 * MSG_ELECTION (1): Election message circulating the ring
 *   - Initiated by any node detecting coordinator failure
 *   - Each node compares its ID with candidate, keeps higher
 *   - When message returns to initiator, winner is determined
 *
 * MSG_COORDINATOR (2): Coordinator announcement circulating the ring
 *   - Sent by winner after election completes
 *   - Informs all nodes of the new leader
 *   - Terminates when it returns to the sender
 *
 * MSG_ALIVE (3): Heartbeat from coordinator
 *   - Sent periodically by coordinator around the ring
 *   - Proves coordinator is still functioning
 *   - Nodes reset their coordinator_timer upon receipt
 *
 * MSG_ACK (4): Acknowledgment message for dynamic ring reconfiguration
 *   - Confirms receipt of ELECTION or COORDINATOR messages
 *   - Used to detect failed nodes and reconfigure the ring
 */
#define MSG_ELECTION    1
#define MSG_COORDINATOR 2
#define MSG_ALIVE       3
#define MSG_ACK         4

/*---------------------------------------------------------------------------*/
/* DYNAMIC RING RECONFIGURATION CONFIGURATION */
/*---------------------------------------------------------------------------*/
/*
 * Enable/disable dynamic ring reconfiguration.
 * When enabled, the ring algorithm can detect failed nodes and
 * automatically reconfigure the ring topology to skip them.
 */
#ifndef ENABLE_DYNAMIC_RING
#define ENABLE_DYNAMIC_RING 1
#endif

#if ENABLE_DYNAMIC_RING

/**
 * ACK_TIMEOUT: How long to wait for acknowledgment of ELECTION/COORDINATOR
 * - Should be short enough for quick failure detection
 * - But long enough to allow for network delays
 * - Set to 500ms for fast failure detection in simulations
 */
#ifndef ACK_TIMEOUT
#define ACK_TIMEOUT         (CLOCK_SECOND / 2)
#endif

/**
 * MAX_RETRIES: Number of retries before marking a node as unreachable
 * - After MAX_RETRIES failed attempts, node is marked unreachable
 * - Ring is reconfigured to skip the unreachable node
 * - Reduced to 2 for faster failure detection (total wait: 1s per failed node)
 */
#ifndef MAX_RETRIES
#define MAX_RETRIES         2
#endif

/**
 * NODE_RECOVERY_INTERVAL: How often to probe unreachable nodes for recovery
 * - Unreachable nodes may come back online (partition heals, node restarts)
 * - Periodic probing allows them to rejoin the ring
 */
#ifndef NODE_RECOVERY_INTERVAL
#define NODE_RECOVERY_INTERVAL (30 * CLOCK_SECOND)
#endif

#endif /* ENABLE_DYNAMIC_RING */

/*---------------------------------------------------------------------------*/
/* NODE STATE MACHINE */
/*---------------------------------------------------------------------------*/
/**
 * Ring algorithm state machine:
 *
 *   STATE_NORMAL: Regular operation with a known leader
 *     - Forwards messages around the ring
 *     - Monitors coordinator via ALIVE messages
 *     - Transitions to STATE_ELECTION on coordinator timeout
 *
 *   STATE_ELECTION: Currently participating in election process
 *     - Initiated election or forwarding election message
 *     - Waiting for election message to complete ring circuit
 *     - Transitions to STATE_NORMAL when coordinator is determined
 *
 *   STATE_WAITING_COORDINATOR: Waiting for coordinator announcement
 *     - Election completed, waiting for COORDINATOR message
 *     - Transitions to STATE_NORMAL when COORDINATOR received
 *
 * STATE TRANSITIONS:
 *   NORMAL -> ELECTION: Start election (coordinator timeout or startup)
 *   ELECTION -> WAITING_COORDINATOR: Election message returned to initiator
 *   WAITING_COORDINATOR -> NORMAL: COORDINATOR message received
 *   ELECTION -> NORMAL: Received COORDINATOR from another node
 */
typedef enum {
  STATE_NORMAL,                 /* Normal operation with known leader */
  STATE_ELECTION,               /* Election in progress */
  STATE_WAITING_COORDINATOR     /* Waiting for coordinator announcement */
} ring_state_t;

/*---------------------------------------------------------------------------*/
/* MESSAGE STRUCTURE */
/*---------------------------------------------------------------------------*/
/**
 * Ring algorithm message structure
 *
 * FIELDS:
 *   type:           Message type (MSG_ELECTION, MSG_COORDINATOR, MSG_ALIVE, MSG_ACK)
 *   initiator_id:   Node that originally started this message chain
 *                   - For ELECTION: Node that started the election
 *                   - For COORDINATOR: Node that won the election
 *                   - For ALIVE: The coordinator
 *                   - For ACK: Node sending the acknowledgment
 *   candidate_id:   Current best candidate (highest ID seen so far)
 *                   - Updated as message traverses the ring
 *                   - When message returns to initiator, this is the winner
 *   sequence:       Election sequence number
 *                   - Incremented for each new election
 *                   - Used to distinguish old vs new elections
 *   target_node_id: Which node should process this message
 *                   - Set to next node in ring
 *                   - Receivers filter based on this field
 *   sender_node_id: Node that sent this message (for ACK routing)
 *                   - Used to route ACK back to the sender
 *   ack_sequence:   ACK correlation sequence number
 *                   - Used to match ACK with pending message
 *
 * MESSAGE SIZE: 13 bytes total
 *   - Compact for efficient wireless transmission
 *   - Includes all necessary information for ring algorithm with dynamic reconfiguration
 */
typedef struct {
  uint8_t type;             /* Message type identifier (1-4) */
  uint16_t initiator_id;    /* Node that started this message chain */
  uint16_t candidate_id;    /* Current best candidate (highest ID) */
  uint16_t sequence;        /* Sequence number for this election */
  uint16_t target_node_id;  /* Next node in ring to process message */
  uint16_t sender_node_id;  /* Node that sent this message (for ACK routing) */
  uint16_t ack_sequence;    /* ACK correlation sequence number */
} ring_msg_t;

#endif /* RING_CONFIG_H_ */
