/*
 * Copyright (c) 2024, TU Dresden
 * All rights reserved.
 *
 * PraSLE (Practical Self-Stabilizing Leader Election) Algorithm Configuration
 * Based on "A Practical Self-Stabilizing Leader Election for
 * Networks of Resource-Constrained IoT Devices" (Conard & Ebnenasir, 2021)
 *
 * ALGORITHM OVERVIEW (Paper Algorithm 1, Figure 1):
 * ================================================
 * PraSLE is a self-stabilizing, round-based leader election algorithm.
 *
 * Key properties:
 * - Self-stabilizing: Recovers from transient faults/arbitrary states
 * - Round-based: Operates in K rounds where K >= network diameter
 * - Symmetric: All nodes run identical code
 * - Tunable: Parameters K and T balance convergence speed vs robustness
 *
 * Operation:
 * 1. Each node starts with (ranking_value, my_id) as its candidate
 * 2. Nodes exchange (min, leader) pairs with neighbors each round
 * 3. Lexicographically smaller pairs are adopted
 * 4. After K rounds, all nodes converge to the same leader
 * 5. In unreliable mode, algorithm runs continuously for fault tolerance
 *
 * UNRELIABLE NETWORK MODE (default):
 * - Broadcasts every round regardless of value changes
 * - Never terminates - runs continuously for self-stabilization
 * - Recovers from message loss and transient faults
 */

#ifndef PRASLE_CONFIG_H_
#define PRASLE_CONFIG_H_

#include "project-conf.h"
#include "election-common.h"

/*---------------------------------------------------------------------------*/
/* UDP PORT CONFIGURATION */
/*---------------------------------------------------------------------------*/
/*
 * UDP port for PraSLE messages.
 * Uses the common election port defined in election-common.h
 */
#define PRASLE_UDP_PORT ELECTION_UDP_PORT

/*---------------------------------------------------------------------------*/
/* NETWORK RELIABILITY MODE */
/*---------------------------------------------------------------------------*/
/*
 * PRASLE_UNRELIABLE_MODE: Select algorithm variant
 *
 * Paper provides two variants:
 *
 * 1 = Unreliable Network Mode (RECOMMENDED for IoT):
 *     - Broadcasts (mini, leaderi) every round unconditionally
 *     - Never terminates - runs continuously
 *     - Provides self-stabilization and fault recovery
 *     - Paper modification: "Lines 23-25 moved outside if statement"
 *
 * 0 = Reliable Network Mode:
 *     - Only broadcasts when values change (optimization)
 *     - Terminates after K rounds when converged
 *     - Assumes no message loss
 */
#ifndef PRASLE_UNRELIABLE_MODE
#define PRASLE_UNRELIABLE_MODE 1
#endif

/*---------------------------------------------------------------------------*/
/* ALGORITHM PARAMETERS */
/*---------------------------------------------------------------------------*/

/* Maximum number of neighbors */
#ifndef MAX_NEIGHBORS
#define MAX_NEIGHBORS 8
#endif

/* Maximum number of nodes in network (N in paper) */
#ifndef N_MAX
#define N_MAX 100
#endif

/*---------------------------------------------------------------------------*/
/* TIMING CONFIGURATION - Two Profiles Available */
/*---------------------------------------------------------------------------*/
/*
 * Two timing profiles are available (matching Bully/Ring for fair comparison):
 * - Normal mode (default): Conservative timeouts for real wireless networks
 * - Fast mode (PRASLE_FAST_MODE): Reduced timeouts for quick testing/simulation
 *
 * To enable fast mode: make ALGORITHM=prasle TARGET=cooja FAST_MODE=1
 */

#ifdef PRASLE_FAST_MODE
/*---------------------------------------------------------------------------*/
/* FAST MODE: Reduced timeouts for testing (~1-2 second convergence) */
/*---------------------------------------------------------------------------*/

#ifndef PRASLE_T_SECONDS
#define PRASLE_T_SECONDS 0.5
#endif

#ifndef PRASLE_STARTUP_DELAY_MAX
#define PRASLE_STARTUP_DELAY_MAX (CLOCK_SECOND / 2)  /* 0.5s max random delay */
#endif

#else /* Normal mode */
/*---------------------------------------------------------------------------*/
/* NORMAL MODE: Conservative timeouts for real wireless networks */
/*---------------------------------------------------------------------------*/

#ifndef PRASLE_T_SECONDS
#define PRASLE_T_SECONDS 1.0
#endif

#ifndef PRASLE_STARTUP_DELAY_MAX
#define PRASLE_STARTUP_DELAY_MAX CLOCK_SECOND  /* 1.0s max random delay */
#endif

#endif /* PRASLE_FAST_MODE */

/*---------------------------------------------------------------------------*/
/* NETWORK TOPOLOGY CONFIGURATION (must be before K_ROUNDS) */
/*---------------------------------------------------------------------------*/
#define TOPOLOGY_RING   1
#define TOPOLOGY_LINE   2
#define TOPOLOGY_MESH   3
#define TOPOLOGY_CLIQUE 4

#ifndef NETWORK_TOPOLOGY
#define NETWORK_TOPOLOGY TOPOLOGY_CLIQUE
#endif

#ifndef NETWORK_SIZE
#ifdef PRASLE_NETWORK_SIZE
#define NETWORK_SIZE PRASLE_NETWORK_SIZE
#else
#define NETWORK_SIZE 10
#endif
#endif

/*
 * PRASLE_K_ROUNDS: Number of rounds (paper parameter K, Line 2)
 *
 * Paper: "K can initially be the diameter of the network"
 *
 * TOPOLOGY-AWARE DEFAULTS (when PRASLE_K_ROUNDS not explicitly set):
 * - Clique: K = 2 (diameter = 1, add 1 for safety margin)
 * - Ring:   K = (N+1)/2 (diameter = N/2)
 * - Line:   K = N (diameter = N-1)
 * - Mesh:   K ≈ 2*sqrt(N)
 *
 * Can be overridden via Makefile: make ALGORITHM=prasle PRASLE_K_ROUNDS=5
 */
#ifndef PRASLE_K_ROUNDS
  #if NETWORK_TOPOLOGY == TOPOLOGY_CLIQUE
    #define PRASLE_K_ROUNDS 2
  #elif NETWORK_TOPOLOGY == TOPOLOGY_RING
    #define PRASLE_K_ROUNDS ((NETWORK_SIZE + 1) / 2)
  #elif NETWORK_TOPOLOGY == TOPOLOGY_LINE
    #define PRASLE_K_ROUNDS NETWORK_SIZE
  #elif NETWORK_TOPOLOGY == TOPOLOGY_MESH
    /* Approximate: 2 * sqrt(N) - using lookup for common sizes */
    #if NETWORK_SIZE > 64
      #define PRASLE_K_ROUNDS 16
    #elif NETWORK_SIZE > 25
      #define PRASLE_K_ROUNDS 10
    #elif NETWORK_SIZE > 9
      #define PRASLE_K_ROUNDS 6
    #else
      #define PRASLE_K_ROUNDS 4
    #endif
  #else
    #define PRASLE_K_ROUNDS 10  /* Fallback: conservative default */
  #endif
#endif

/*
 * PRASLE_RESET_CYCLES: Periodic reset interval for crash recovery
 *
 * In unreliable mode, the algorithm resets mini/leaderi to initial values
 * every N cycles. This allows recovery from leader crashes:
 * - Without reset: Crashed leader's (min,leader) values persist forever
 * - With reset: After reset, nodes re-converge to a live leader
 *
 * Set to 0 to disable periodic reset (original algorithm behavior).
 * Recommended: 3-5 cycles for crash recovery scenarios.
 *
 * Recovery time = RESET_CYCLES * K * T seconds (worst case)
 */
#ifndef PRASLE_RESET_CYCLES
#define PRASLE_RESET_CYCLES 3
#endif

#define CLOCK_SECOND_FLOAT ((float)CLOCK_SECOND)
#define PRASLE_T_VALUE ((clock_time_t)(PRASLE_T_SECONDS * CLOCK_SECOND_FLOAT))

/*---------------------------------------------------------------------------*/
/* MESSAGE STRUCTURE */
/*---------------------------------------------------------------------------*/
typedef struct {
  uint16_t min_value;    /* Ranking value (mini) */
  uint16_t leader_id;    /* Leader ID (leaderi) */
  uint16_t sender_id;    /* ID of sending node */
} prasle_msg_t;

/*---------------------------------------------------------------------------*/
/* NEIGHBOR INFORMATION */
/*---------------------------------------------------------------------------*/
typedef struct {
  uint16_t node_id;
  uint16_t min_value;
  uint16_t leader_id;
  bool valid;
} neighbor_info_t;

#endif /* PRASLE_CONFIG_H_ */
