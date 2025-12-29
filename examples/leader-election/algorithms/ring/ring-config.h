/*
 * Copyright (c) 2024, TU Dresden
 * All rights reserved.
 *
 * Ring Leader Election Algorithm Configuration
 */

#ifndef RING_CONFIG_H_
#define RING_CONFIG_H_

#include "project-conf.h"

/*---------------------------------------------------------------------------*/
/* TIMING CONFIGURATION */
/*---------------------------------------------------------------------------*/

#ifndef ELECTION_TIMEOUT
#define ELECTION_TIMEOUT    (8 * CLOCK_SECOND)
#endif

#ifndef COORDINATOR_TIMEOUT
#define COORDINATOR_TIMEOUT (15 * CLOCK_SECOND)
#endif

#ifndef ALIVE_INTERVAL
#define ALIVE_INTERVAL      (10 * CLOCK_SECOND)
#endif

#ifndef RANDOM_DELAY_MAX
#define RANDOM_DELAY_MAX    (3 * CLOCK_SECOND)
#endif

/*---------------------------------------------------------------------------*/
/* RING TOPOLOGY CONFIGURATION */
/*---------------------------------------------------------------------------*/

/* Use RING_SIZE from project-conf.h if defined */
#ifndef RING_SIZE
#define RING_SIZE 10
#endif

/*---------------------------------------------------------------------------*/
/* MESSAGE TYPES - Ring Algorithm Messages */
/*---------------------------------------------------------------------------*/
#define MSG_ELECTION    1  /* Election message circulating the ring */
#define MSG_COORDINATOR 2  /* Coordinator announcement */
#define MSG_ALIVE       3  /* Heartbeat from coordinator */

/*---------------------------------------------------------------------------*/
/* NODE STATE MACHINE */
/*---------------------------------------------------------------------------*/
typedef enum {
  STATE_NORMAL,
  STATE_ELECTION,
  STATE_WAITING_COORDINATOR
} ring_state_t;

/*---------------------------------------------------------------------------*/
/* MESSAGE STRUCTURE */
/*---------------------------------------------------------------------------*/
typedef struct {
  uint8_t type;
  uint16_t initiator_id;    /* Node that started the election */
  uint16_t candidate_id;    /* Current best candidate (highest ID) */
  uint16_t sequence;
  uint16_t target_node_id;  /* Which node should process this message */
} ring_msg_t;

#endif /* RING_CONFIG_H_ */
