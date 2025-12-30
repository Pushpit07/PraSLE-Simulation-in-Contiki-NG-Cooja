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
/* FAST MODE: Reduced timeouts for testing (~2-4 second convergence) */
/*---------------------------------------------------------------------------*/

/**
 * ELECTION_TIMEOUT: Maximum time for election message to complete ring circuit
 * - Fast mode: 4 seconds (suitable for 10-node ring in simulation)
 */
#ifndef ELECTION_TIMEOUT
#define ELECTION_TIMEOUT    (4 * CLOCK_SECOND)
#endif

/**
 * COORDINATOR_TIMEOUT: Time to wait before declaring coordinator dead
 * - Fast mode: 8 seconds (2x ALIVE_INTERVAL)
 */
#ifndef COORDINATOR_TIMEOUT
#define COORDINATOR_TIMEOUT (8 * CLOCK_SECOND)
#endif

/**
 * ALIVE_INTERVAL: How often coordinator sends ALIVE heartbeat around ring
 * - Fast mode: 4 seconds (time for ALIVE to complete ring circuit)
 */
#ifndef ALIVE_INTERVAL
#define ALIVE_INTERVAL      (4 * CLOCK_SECOND)
#endif

/**
 * RANDOM_DELAY_MAX: Random startup delay to prevent synchronized elections
 * - Fast mode: 1 second
 */
#ifndef RANDOM_DELAY_MAX
#define RANDOM_DELAY_MAX    (1 * CLOCK_SECOND)
#endif

#else /* Normal mode */
/*---------------------------------------------------------------------------*/
/* NORMAL MODE: Conservative timeouts for real wireless networks */
/*---------------------------------------------------------------------------*/

/**
 * ELECTION_TIMEOUT: Maximum time for election message to complete ring circuit
 *
 * CALCULATION:
 * - Ring size: N nodes
 * - Per-hop delay: ~1-2 seconds (wireless + processing)
 * - Total circuit time: N * 1-2 seconds
 * - Safety margin: 1.5x
 *
 * For 10 nodes: 10 * 1.2s = 12s is reasonable
 * Set to 12 seconds to handle delays and packet loss
 */
#ifndef ELECTION_TIMEOUT
#define ELECTION_TIMEOUT    (12 * CLOCK_SECOND)
#endif

/**
 * COORDINATOR_TIMEOUT: How long to wait before declaring coordinator dead
 *
 * DESIGN:
 * - Must be > ALIVE_INTERVAL + ring circuit time
 * - Allows for one missed heartbeat before triggering election
 * - Set to 20 seconds (~1.6x ALIVE_INTERVAL)
 */
#ifndef COORDINATOR_TIMEOUT
#define COORDINATOR_TIMEOUT (20 * CLOCK_SECOND)
#endif

/**
 * ALIVE_INTERVAL: How often coordinator sends ALIVE heartbeat around ring
 *
 * DESIGN:
 * - ALIVE must complete ring circuit before next one is sent
 * - Similar to election circuit time
 * - Set to 12 seconds (full ring circulation time)
 */
#ifndef ALIVE_INTERVAL
#define ALIVE_INTERVAL      (12 * CLOCK_SECOND)
#endif

/**
 * RANDOM_DELAY_MAX: Random startup delay to prevent synchronized elections
 *
 * PURPOSE:
 * - Staggers node startup to prevent all nodes starting elections simultaneously
 * - Reduces collision during initial leader election
 */
#ifndef RANDOM_DELAY_MAX
#define RANDOM_DELAY_MAX    (5 * CLOCK_SECOND)
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
 */
#define MSG_ELECTION    1
#define MSG_COORDINATOR 2
#define MSG_ALIVE       3

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
 *   type:           Message type (MSG_ELECTION, MSG_COORDINATOR, MSG_ALIVE)
 *   initiator_id:   Node that originally started this message chain
 *                   - For ELECTION: Node that started the election
 *                   - For COORDINATOR: Node that won the election
 *                   - For ALIVE: The coordinator
 *   candidate_id:   Current best candidate (highest ID seen so far)
 *                   - Updated as message traverses the ring
 *                   - When message returns to initiator, this is the winner
 *   sequence:       Election sequence number
 *                   - Incremented for each new election
 *                   - Used to distinguish old vs new elections
 *   target_node_id: Which node should process this message
 *                   - Set to next node in ring
 *                   - Receivers filter based on this field
 *
 * MESSAGE SIZE: 9 bytes total
 *   - Compact for efficient wireless transmission
 *   - Includes all necessary information for ring algorithm
 */
typedef struct {
  uint8_t type;             /* Message type identifier (1-3) */
  uint16_t initiator_id;    /* Node that started this message chain */
  uint16_t candidate_id;    /* Current best candidate (highest ID) */
  uint16_t sequence;        /* Sequence number for this election */
  uint16_t target_node_id;  /* Next node in ring to process message */
} ring_msg_t;

#endif /* RING_CONFIG_H_ */
