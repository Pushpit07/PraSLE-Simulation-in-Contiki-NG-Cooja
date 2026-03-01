/*
 * Copyright (c) 2024, TU Dresden
 * All rights reserved.
 *
 * Adaptive-PraSLE: Enhanced Self-Stabilizing Leader Election for IoT Networks
 *
 * BASE ALGORITHM REFERENCE (PraSLE):
 * M. Conard and A. Ebnenasir, "A Practical Self-Stabilizing Leader Election
 * for Networks of Resource-Constrained IoT Devices,"
 * 17th European Dependable Computing Conference (EDCC), pp. 127-134, 2021.
 * DOI: 10.1109/EDCC53658.2021.00025
 *
 * This implementation extends standard PraSLE with:
 *   1. Energy-aware leader selection
 *   2. Link-quality aware scoring
 *   3. Controlled leader rotation/handover
 *   4. Adaptive timeout management
 *   5. Backup-based fast recovery
 *
 * TIMING MODEL:
 * Inherits PraSLE timing parameters (T, K) for fair comparison:
 *   - Normal mode: T = 1.0s (Algorithm 1, Line 8)
 *   - Fast mode: T = 0.1s (Paper Table I, IoT-Lab experiments)
 */

#ifndef ADAPTIVE_PRASLE_CONFIG_H_
#define ADAPTIVE_PRASLE_CONFIG_H_

#include "contiki.h"
#include "project-conf.h"
#include "election-common.h"
#include <stdbool.h>
#include <stdint.h>

/*===========================================================================*/
/* UDP PORT CONFIGURATION                                                    */
/*===========================================================================*/
/*
 * UDP port for Adaptive-PraSLE messages.
 * Uses the common election port defined in election-common.h for consistency
 * with PraSLE and fair comparison.
 */
#define ADAPTIVE_PRASLE_UDP_PORT ELECTION_UDP_PORT

/*===========================================================================*/
/* FEATURE TOGGLES                                                           */
/* Enable/disable individual features via compile flags or defaults here     */
/*===========================================================================*/

#ifndef ADAPTIVE_ENERGY_AWARE
#define ADAPTIVE_ENERGY_AWARE        1  /* Enable energy-aware ranking */
#endif

#ifndef ADAPTIVE_LINK_QUALITY_AWARE
#define ADAPTIVE_LINK_QUALITY_AWARE  1  /* Enable link-quality ranking */
#endif

#ifndef ADAPTIVE_LEADER_ROTATION
#define ADAPTIVE_LEADER_ROTATION     1  /* Enable leader rotation */
#endif

#ifndef ADAPTIVE_TIMEOUTS
#define ADAPTIVE_TIMEOUTS            1  /* Enable adaptive timeouts */
#endif

#ifndef ADAPTIVE_BACKUP_LIST
#define ADAPTIVE_BACKUP_LIST         1  /* Enable backup leader list */
#endif

/*===========================================================================*/
/* ALGORITHM PARAMETERS                                                      */
/*===========================================================================*/

/* Maximum number of neighbors per node */
#ifndef MAX_NEIGHBORS
#define MAX_NEIGHBORS 8
#endif

/* Maximum number of nodes in network */
#ifndef N_MAX
#define N_MAX 100
#endif

/*===========================================================================*/
/* NETWORK TOPOLOGY CONFIGURATION (must be before K_ROUNDS)                  */
/*===========================================================================*/

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
 * K_ROUNDS: Number of rounds (paper parameter K)
 *
 * Paper: "K can initially be the diameter of the network"
 *
 * TOPOLOGY-AWARE DEFAULTS (matching PraSLE for fair comparison):
 * - Clique: K = 2 (diameter = 1, add 1 for safety margin)
 * - Ring:   K = (N+1)/2 (diameter = N/2)
 * - Line:   K = N (diameter = N-1)
 * - Mesh:   K ≈ 2*sqrt(N)
 *
 * Can be overridden via Makefile: make ALGORITHM=adaptive-prasle PRASLE_K_ROUNDS=5
 */
#ifndef K_ROUNDS
#ifdef PRASLE_K_ROUNDS
#define K_ROUNDS PRASLE_K_ROUNDS
#else
  #if NETWORK_TOPOLOGY == TOPOLOGY_CLIQUE
    #define K_ROUNDS 2
  #elif NETWORK_TOPOLOGY == TOPOLOGY_RING
    #define K_ROUNDS ((NETWORK_SIZE + 1) / 2)
  #elif NETWORK_TOPOLOGY == TOPOLOGY_LINE
    #define K_ROUNDS NETWORK_SIZE
  #elif NETWORK_TOPOLOGY == TOPOLOGY_MESH
    /* Approximate: 2 * sqrt(N) - using lookup for common sizes */
    #if NETWORK_SIZE > 64
      #define K_ROUNDS 16
    #elif NETWORK_SIZE > 25
      #define K_ROUNDS 10
    #elif NETWORK_SIZE > 9
      #define K_ROUNDS 6
    #else
      #define K_ROUNDS 4
    #endif
  #else
    #define K_ROUNDS 10  /* Fallback: conservative default */
  #endif
#endif
#endif

/*===========================================================================*/
/* TIMING CONFIGURATION - Two Profiles Available                              */
/*===========================================================================*/
/*
 * Two timing profiles are available (matching PraSLE for fair comparison):
 * - Normal mode (default): Conservative timeouts for real wireless networks
 * - Fast mode (PRASLE_FAST_MODE): Reduced timeouts for quick testing/simulation
 *
 * To enable fast mode: make ALGORITHM=adaptive-prasle TARGET=cooja FAST_MODE=1
 */

#ifdef PRASLE_FAST_MODE
/*---------------------------------------------------------------------------*/
/* FAST MODE: Reduced round duration for testing                              */
/*---------------------------------------------------------------------------*/
/*
 * Matches PraSLE FAST_MODE timing from paper Table I (IoT-Lab experiments):
 *   - T = 0.1s (same as prasle-config.h)
 *   - Startup delay = 0.25s max
 *
 * This ensures fair comparison between PraSLE and Adaptive-PraSLE.
 */

#ifndef T_SECONDS
#define T_SECONDS 0.1
#endif

#ifndef STARTUP_DELAY_MAX
#define STARTUP_DELAY_MAX (CLOCK_SECOND / 4)  /* 0.25s max random delay */
#endif

#else /* Normal mode */
/*---------------------------------------------------------------------------*/
/* NORMAL MODE: Conservative timeouts for real wireless networks             */
/*---------------------------------------------------------------------------*/

/* T: Maximum network latency in seconds (base value) */
#ifndef T_SECONDS
#ifdef PRASLE_T_SECONDS
#define T_SECONDS PRASLE_T_SECONDS
#else
#define T_SECONDS 1.0
#endif
#endif

#ifndef STARTUP_DELAY_MAX
#define STARTUP_DELAY_MAX CLOCK_SECOND  /* 1.0s max random delay */
#endif

#endif /* PRASLE_FAST_MODE */

/*---------------------------------------------------------------------------*/
/* STANDARDIZED MODE: Identical parameters for fair cross-algorithm comparison */
/*---------------------------------------------------------------------------*/
/*
 * When STANDARDIZED_PARAMS is defined (via PARAM_CASE=2 in Makefile),
 * all algorithms use the same timing parameters to enable fair comparison
 * of algorithm efficiency rather than parameter tuning.
 *
 * For Adaptive-PraSLE, T is equivalent to the heartbeat interval in Bully/Ring:
 *   - T = 2.0s (matches Bully/Ring ALIVE_INTERVAL)
 *   - STARTUP_DELAY_MAX = 2.0s (matches Bully/Ring)
 *
 * This makes the broadcast rate equal across all algorithms,
 * enabling fair comparison of message overhead.
 */
#ifdef STANDARDIZED_PARAMS
#undef T_SECONDS
#undef STARTUP_DELAY_MAX

#define T_SECONDS 2.0
#define STARTUP_DELAY_MAX (2 * CLOCK_SECOND)
#endif /* STANDARDIZED_PARAMS */

#define CLOCK_SECOND_FLOAT ((float)CLOCK_SECOND)
#define T_VALUE ((clock_time_t)(T_SECONDS * CLOCK_SECOND_FLOAT))

/* Convert between milliseconds and clock ticks (use definitions from election-common.h if available) */
#ifndef MS_TO_CLOCK
#define MS_TO_CLOCK(ms) ((clock_time_t)(((ms) * CLOCK_SECOND) / 1000))
#endif
#ifndef CLOCK_TO_MS
#define CLOCK_TO_MS(ticks) ((uint32_t)(((ticks) * 1000) / CLOCK_SECOND))
#endif

/*===========================================================================*/
/* ENERGY-AWARE CONFIGURATION                                                */
/*===========================================================================*/

/* Weight for energy component in composite score (0-100) */
#ifndef ENERGY_WEIGHT
#define ENERGY_WEIGHT               30
#endif

/* Energy level below which leader handover is triggered (percentage) */
#ifndef ENERGY_CRITICAL_THRESHOLD
#define ENERGY_CRITICAL_THRESHOLD   20
#endif

/* Energy level considered "low" - avoid selecting as new leader */
#ifndef ENERGY_LOW_THRESHOLD
#define ENERGY_LOW_THRESHOLD        40
#endif

/* How often to update energy estimate */
#ifndef ENERGY_UPDATE_INTERVAL
#define ENERGY_UPDATE_INTERVAL      (5 * CLOCK_SECOND)
#endif

/* Initial simulated battery level (percentage) */
#ifndef INITIAL_BATTERY_LEVEL
#define INITIAL_BATTERY_LEVEL       100
#endif

/* Battery drain rate per second of radio activity (for simulation) */
#ifndef BATTERY_DRAIN_RATE
#define BATTERY_DRAIN_RATE          1
#endif

/*===========================================================================*/
/* LINK QUALITY CONFIGURATION                                                */
/*===========================================================================*/

/* Weight for link quality component in composite score (0-100) */
#ifndef LINK_QUALITY_WEIGHT
#define LINK_QUALITY_WEIGHT         30
#endif

/*===========================================================================*/
/* CONNECTIVITY (NEIGHBOR COUNT) CONFIGURATION                               */
/*===========================================================================*/

/* Weight for neighbor count component in composite score (0-100) */
#ifndef CONNECTIVITY_WEIGHT
#define CONNECTIVITY_WEIGHT         20
#endif

/* Minimum neighbors required to be considered a leader candidate */
#ifndef MIN_NEIGHBORS_FOR_LEADER
#define MIN_NEIGHBORS_FOR_LEADER     2
#endif

/*===========================================================================*/
/* CPU AVAILABILITY CONFIGURATION                                            */
/*===========================================================================*/

/* Weight for CPU availability component in composite score (0-100) */
#ifndef CPU_WEIGHT
#define CPU_WEIGHT                  20
#endif

/* CPU usage percentage considered "busy" */
#ifndef CPU_HIGH_THRESHOLD
#define CPU_HIGH_THRESHOLD          80
#endif

/*===========================================================================*/
/* LINK FRESHNESS CONFIGURATION                                              */
/*===========================================================================*/

/* Minimum freshness to trust link data (0-16 scale) */
#ifndef FRESHNESS_MIN_THRESHOLD
#define FRESHNESS_MIN_THRESHOLD      4
#endif

/* Enable freshness as confidence multiplier for link quality */
#ifndef FRESHNESS_CONFIDENCE_SCALE
#define FRESHNESS_CONFIDENCE_SCALE   1
#endif

/* RSSI threshold for "excellent" link (dBm) */
#ifndef RSSI_GOOD_THRESHOLD
#define RSSI_GOOD_THRESHOLD        -60
#endif

/* RSSI threshold for "poor" link (dBm) */
#ifndef RSSI_POOR_THRESHOLD
#define RSSI_POOR_THRESHOLD        -85
#endif

/* Maximum acceptable ETX before neighbor is considered unreliable */
#ifndef ETX_MAX_ACCEPTABLE
#define ETX_MAX_ACCEPTABLE           4
#endif

/* Minimum acceptable LQI (0-255 scale) */
#ifndef LQI_MIN_ACCEPTABLE
#define LQI_MIN_ACCEPTABLE          50
#endif

/* ETX divisor for fixed-point representation (per Contiki-NG) */
#ifndef LINK_STATS_ETX_DIVISOR
#define LINK_STATS_ETX_DIVISOR     128
#endif

/* Unknown RSSI value */
#define RSSI_UNKNOWN               -127

/*===========================================================================*/
/* LEADER ROTATION CONFIGURATION                                             */
/*===========================================================================*/

/* Minimum time as leader before rotation allowed (prevents oscillation) */
#ifndef MIN_LEADERSHIP_TERM
#define MIN_LEADERSHIP_TERM         (30 * CLOCK_SECOND)
#endif

/* Timeout waiting for handover acknowledgement */
#ifndef HANDOVER_ACK_TIMEOUT
#define HANDOVER_ACK_TIMEOUT        (3 * CLOCK_SECOND)
#endif

/* Maximum retries for handover before fallback to next backup */
#ifndef MAX_HANDOVER_RETRIES
#define MAX_HANDOVER_RETRIES         3
#endif

/*===========================================================================*/
/* ADAPTIVE TIMEOUT CONFIGURATION                                            */
/*===========================================================================*/

/* Minimum allowed timeout value (seconds) - reduced for faster convergence */
#ifndef TIMEOUT_MIN_SECONDS
#define TIMEOUT_MIN_SECONDS        0.15
#endif

/* Maximum allowed timeout value (seconds) */
#ifndef TIMEOUT_MAX_SECONDS
#define TIMEOUT_MAX_SECONDS        5.0
#endif

/* EWMA smoothing factor for Jacobson RTT estimation (α = 1/8 = 0.125) */
#ifndef TIMEOUT_ALPHA
#define TIMEOUT_ALPHA             0.125  /* Classic Jacobson formula: α = 1/8 */
#endif

/* Safety margin multiplier for timeout calculation - reduced for faster convergence */
#ifndef TIMEOUT_SAFETY_MARGIN
#define TIMEOUT_SAFETY_MARGIN      1.2
#endif

/* Initial RTT estimate in milliseconds - reduced for faster initial convergence */
#ifndef INITIAL_RTT_ESTIMATE_MS
#define INITIAL_RTT_ESTIMATE_MS    100
#endif

/* Initial RTT variance in milliseconds - reduced for faster initial convergence */
#ifndef INITIAL_RTT_VARIANCE_MS
#define INITIAL_RTT_VARIANCE_MS    25
#endif

/*===========================================================================*/
/* BACKUP LIST CONFIGURATION                                                 */
/*===========================================================================*/

/* Number of backup leaders to maintain */
#ifndef BACKUP_LIST_SIZE
#define BACKUP_LIST_SIZE             3
#endif

/* Interval between leader heartbeats */
#ifndef BACKUP_HEARTBEAT_INTERVAL
#define BACKUP_HEARTBEAT_INTERVAL   (3 * CLOCK_SECOND)
#endif

/* Number of missed heartbeats before declaring failure */
#ifndef BACKUP_FAILURE_THRESHOLD
#define BACKUP_FAILURE_THRESHOLD     3
#endif

/*===========================================================================*/
/* RESET-CYCLE CONFIGURATION (Fast Recovery like PraSLE)                     */
/*===========================================================================*/

/* Enable reset-cycle based recovery (like PraSLE) */
#ifndef ADAPTIVE_RESET_CYCLES
#define ADAPTIVE_RESET_CYCLES        1  /* Enable periodic reset for fast recovery */
#endif

/* Number of election cycles between resets */
#ifndef RESET_CYCLE_COUNT
#define RESET_CYCLE_COUNT            3  /* Reset every 3 cycles */
#endif

/*
 * Recovery time calculation:
 * Recovery = RESET_CYCLE_COUNT × K_ROUNDS × T_SECONDS
 * Default: 3 × 2 × 1.0s = ~6s (normal mode)
 *          3 × 2 × 0.5s = ~3s (fast mode)
 */

/*===========================================================================*/
/* MESSAGE TYPES                                                             */
/*===========================================================================*/

typedef enum {
  MSG_ELECTION = 0,        /* Standard election message */
  MSG_HEARTBEAT = 1,       /* Leader heartbeat */
  MSG_HANDOVER_REQ = 2,    /* Leader handover request */
  MSG_HANDOVER_ACK = 3,    /* Handover acknowledgement */
  MSG_BACKUP_UPDATE = 4,   /* Backup list update broadcast */
} adaptive_msg_type_t;

/*===========================================================================*/
/* MESSAGE STRUCTURES                                                        */
/*===========================================================================*/

/* Extended election message with energy and link quality info */
typedef struct {
  uint8_t msg_type;        /* Message type (adaptive_msg_type_t) */
  uint8_t padding;         /* Alignment padding */
  uint16_t min_value;      /* Composite ranking score */
  uint16_t leader_id;      /* Current leader ID */
  uint16_t sender_id;      /* Sender node ID */
  uint8_t energy_level;    /* Sender's battery % (0-100) */
  int8_t avg_rssi;         /* Sender's average RSSI to neighbors */
  uint8_t neighbor_count;  /* Number of valid neighbors */
  uint8_t flags;           /* Status flags */
  uint16_t seq_num;        /* Sequence number for ordering */
} __attribute__((packed)) adaptive_prasle_msg_t;

/* Handover-specific message */
typedef struct {
  uint8_t msg_type;        /* MSG_HANDOVER_REQ or MSG_HANDOVER_ACK */
  uint8_t padding;         /* Alignment padding */
  uint16_t old_leader;     /* Current leader ID */
  uint16_t new_leader;     /* Proposed new leader */
  uint16_t seq_num;        /* Handover sequence number */
} __attribute__((packed)) handover_msg_t;

/* Status flags for adaptive_prasle_msg_t */
#define FLAG_IS_LEADER           0x01
#define FLAG_LOW_ENERGY          0x02
#define FLAG_HANDOVER_PENDING    0x04

/*===========================================================================*/
/* NEIGHBOR DATA STRUCTURES                                                  */
/*===========================================================================*/

/* Extended neighbor information with energy and link quality metrics */
typedef struct {
  uint16_t node_id;           /* Neighbor's node ID */
  uint16_t min_value;         /* Neighbor's min ranking value */
  uint16_t leader_id;         /* Neighbor's known leader */
  bool valid;                 /* Is this neighbor entry valid? */

  /* Energy metrics */
  uint8_t energy_level;       /* Last reported energy % (0-100) */
  clock_time_t energy_ts;     /* Timestamp of last energy report */

  /* Link quality metrics */
  int8_t rssi;                /* RSSI from this neighbor (dBm) */
  uint16_t etx;               /* ETX to this neighbor (x128 fixed-point) */
  uint8_t lqi;                /* Link Quality Indicator (0-255) */
  uint8_t missed_msgs;        /* Consecutive missed messages */
  clock_time_t last_heard;    /* Timestamp of last received message */

  /* Composite score (calculated) */
  uint16_t composite_score;   /* Weighted score for ranking (lower = better) */
} adaptive_neighbor_info_t;

/*===========================================================================*/
/* BACKUP LEADER DATA STRUCTURE                                              */
/*===========================================================================*/

/* Backup leader entry */
typedef struct {
  uint16_t node_id;           /* Backup's node ID */
  uint16_t score;             /* Backup's composite score */
  clock_time_t last_heartbeat;/* Last heartbeat timestamp */
  uint8_t missed_heartbeats;  /* Consecutive missed heartbeats */
} backup_entry_t;

/*===========================================================================*/
/* LEADER ROTATION STATE                                                     */
/*===========================================================================*/

typedef enum {
  ROTATION_IDLE = 0,          /* No rotation in progress */
  ROTATION_INITIATING = 1,    /* Leader initiating handover */
  ROTATION_WAITING_ACK = 2,   /* Waiting for successor ACK */
  ROTATION_COMPLETING = 3,    /* Finalizing handover */
} rotation_state_t;

/*===========================================================================*/
/* ALGORITHM STATE                                                           */
/*===========================================================================*/

typedef enum {
  STATE_INIT = 0,             /* Initialization */
  STATE_ELECTION = 1,         /* Election in progress */
  STATE_NORMAL = 2,           /* Normal operation (follower) */
  STATE_LEADER = 3,           /* This node is leader */
  STATE_RECOVERY = 4,         /* Recovery after leader failure */
} algorithm_state_t;

/*===========================================================================*/
/* RTT ESTIMATION STATE                                                      */
/*===========================================================================*/

typedef struct {
  uint32_t estimate_ms;       /* Smoothed RTT estimate */
  uint32_t variance_ms;       /* RTT variance */
  clock_time_t last_send;     /* Timestamp of last sent message */
  uint16_t pending_seq;       /* Sequence number of pending message */
  bool waiting_response;      /* Are we waiting for a response? */
} rtt_state_t;

/*===========================================================================*/
/* UTILITY MACROS                                                            */
/*===========================================================================*/

/* Validate weights sum to 100 */
/* Note: NodeID is NOT used in scoring - ties are broken by the is_better() comparison */
#if (ENERGY_WEIGHT + LINK_QUALITY_WEIGHT + CONNECTIVITY_WEIGHT + CPU_WEIGHT != 100)
#warning "Sum of weights is not 100 - scores may not span expected range"
#endif

/* Clamp value between min and max */
#define CLAMP(val, min_val, max_val) \
  ((val) < (min_val) ? (min_val) : ((val) > (max_val) ? (max_val) : (val)))

/* Check if a value is within range */
#define IN_RANGE(val, min_val, max_val) \
  ((val) >= (min_val) && (val) <= (max_val))

#endif /* ADAPTIVE_PRASLE_CONFIG_H_ */
