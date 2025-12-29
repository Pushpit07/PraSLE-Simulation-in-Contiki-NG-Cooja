/*
 * Copyright (c) 2024, TU Dresden
 * All rights reserved.
 *
 * Leader Election Algorithm Framework - Common Definitions
 */

#ifndef ELECTION_COMMON_H_
#define ELECTION_COMMON_H_

#include "contiki.h"
#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------*/
/* Algorithm Identifiers */
/*---------------------------------------------------------------------------*/
#define ALGORITHM_BULLY         1
#define ALGORITHM_RING          2
#define ALGORITHM_PRASLE        3
#define ALGORITHM_PRASLE_CUSTOM 4

/* Current algorithm (set by Makefile via -DCURRENT_ALGORITHM=X) */
#ifndef CURRENT_ALGORITHM
#define CURRENT_ALGORITHM ALGORITHM_BULLY
#endif

/*---------------------------------------------------------------------------*/
/* Algorithm Name Strings (for logging and CSV output) */
/*---------------------------------------------------------------------------*/
#if CURRENT_ALGORITHM == ALGORITHM_BULLY
#define ALGORITHM_NAME "bully"
#define ALGORITHM_NAME_UPPER "BULLY"
#elif CURRENT_ALGORITHM == ALGORITHM_RING
#define ALGORITHM_NAME "ring"
#define ALGORITHM_NAME_UPPER "RING"
#elif CURRENT_ALGORITHM == ALGORITHM_PRASLE
#define ALGORITHM_NAME "prasle"
#define ALGORITHM_NAME_UPPER "PRASLE"
#elif CURRENT_ALGORITHM == ALGORITHM_PRASLE_CUSTOM
#define ALGORITHM_NAME "adaptive-prasle"
#define ALGORITHM_NAME_UPPER "PRASLE_CUSTOM"
#else
#define ALGORITHM_NAME "unknown"
#define ALGORITHM_NAME_UPPER "UNKNOWN"
#endif

/*---------------------------------------------------------------------------*/
/* Common State Definitions */
/*---------------------------------------------------------------------------*/
typedef enum {
  ELECTION_STATE_INIT = 0,       /* Initial state before election starts */
  ELECTION_STATE_NORMAL = 1,     /* Leader known, stable operation */
  ELECTION_STATE_ELECTION = 2,   /* Election in progress */
  ELECTION_STATE_WAITING = 3,    /* Waiting for election outcome */
  ELECTION_STATE_LEADER = 4      /* I am the leader */
} election_state_t;

/* State name strings for logging */
static inline const char* election_state_name(election_state_t state) {
  switch(state) {
    case ELECTION_STATE_INIT: return "INIT";
    case ELECTION_STATE_NORMAL: return "NORMAL";
    case ELECTION_STATE_ELECTION: return "ELECTION";
    case ELECTION_STATE_WAITING: return "WAITING";
    case ELECTION_STATE_LEADER: return "LEADER";
    default: return "UNKNOWN";
  }
}

/*---------------------------------------------------------------------------*/
/* Common Network Configuration */
/*---------------------------------------------------------------------------*/
#define ELECTION_UDP_PORT 8765

/*---------------------------------------------------------------------------*/
/* Common Timing Constants */
/*---------------------------------------------------------------------------*/
#define METRICS_OUTPUT_INTERVAL (10 * CLOCK_SECOND)

/*---------------------------------------------------------------------------*/
/* Maximum Network Size */
/*---------------------------------------------------------------------------*/
#ifndef MAX_NODES
#define MAX_NODES 100
#endif

/*---------------------------------------------------------------------------*/
/* Utility Macros */
/*---------------------------------------------------------------------------*/
#define CLOCK_TO_MS(ticks) ((unsigned long)((ticks) * 1000 / CLOCK_SECOND))
#define MS_TO_CLOCK(ms) ((clock_time_t)((ms) * CLOCK_SECOND / 1000))

/*---------------------------------------------------------------------------*/
/* Debug Logging Macros */
/*---------------------------------------------------------------------------*/
#ifdef ELECTION_DEBUG
#define ELECTION_LOG(...) printf(__VA_ARGS__)
#else
#define ELECTION_LOG(...)
#endif

#endif /* ELECTION_COMMON_H_ */
