/*
 * Copyright (c) 2024, TU Dresden
 * All rights reserved.
 *
 * PraSLE-Custom (Customized Practical Self-Stabilizing Leader Election)
 * This is a placeholder for custom modifications to the PraSLE algorithm.
 * Modify this file to implement your customizations.
 */

#ifndef PRASLE_CUSTOM_CONFIG_H_
#define PRASLE_CUSTOM_CONFIG_H_

#include "project-conf.h"

/*---------------------------------------------------------------------------*/
/* ALGORITHM PARAMETERS */
/*---------------------------------------------------------------------------*/

/* Maximum number of neighbors */
#ifndef MAX_NEIGHBORS
#define MAX_NEIGHBORS 8
#endif

/* Maximum number of nodes in network */
#ifndef N_MAX
#define N_MAX 100
#endif

/* K: Number of rounds (should be at least network diameter) */
#ifndef K_ROUNDS
#ifdef PRASLE_K_ROUNDS
#define K_ROUNDS PRASLE_K_ROUNDS
#else
#define K_ROUNDS 10
#endif
#endif

/* T: Maximum network latency in seconds */
#ifndef T_SECONDS
#ifdef PRASLE_T_SECONDS
#define T_SECONDS PRASLE_T_SECONDS
#else
#define T_SECONDS 1.0
#endif
#endif

#define CLOCK_SECOND_FLOAT ((float)CLOCK_SECOND)
#define T_VALUE ((clock_time_t)(T_SECONDS * CLOCK_SECOND_FLOAT))

/*---------------------------------------------------------------------------*/
/* NETWORK TOPOLOGY CONFIGURATION */
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

#endif /* PRASLE_CUSTOM_CONFIG_H_ */
