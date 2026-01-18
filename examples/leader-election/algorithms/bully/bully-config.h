/*
 * Bully Algorithm Configuration
 */

#ifndef BULLY_CONFIG_H_
#define BULLY_CONFIG_H_

/*---------------------------------------------------------------------------*/
/* TIMING CONFIGURATION - Tuned for wireless sensor networks */
/*---------------------------------------------------------------------------*/
/*
 * Two timing profiles are available:
 * - Normal mode (default): Conservative timeouts for real wireless networks
 * - Fast mode (BULLY_FAST_MODE=1): Reduced timeouts for quick testing/simulation
 *
 * To enable fast mode: make ALGORITHM=bully TARGET=cooja FAST_MODE=1
 */

#ifdef BULLY_FAST_MODE
/*---------------------------------------------------------------------------*/
/* FAST MODE: Reduced timeouts for testing (~1-2 second convergence) */
/*---------------------------------------------------------------------------*/

#ifndef ELECTION_TIMEOUT
#define ELECTION_TIMEOUT    (1 * CLOCK_SECOND)
#endif

#ifndef COORDINATOR_TIMEOUT
#define COORDINATOR_TIMEOUT (4 * CLOCK_SECOND)
#endif

#ifndef ALIVE_INTERVAL
#define ALIVE_INTERVAL      (2 * CLOCK_SECOND)
#endif

#ifndef RANDOM_DELAY_MAX
#define RANDOM_DELAY_MAX    (1 * CLOCK_SECOND)
#endif

#else /* Normal mode */
/*---------------------------------------------------------------------------*/
/* NORMAL MODE: Conservative timeouts for real wireless networks */
/*---------------------------------------------------------------------------*/

/**
 * ELECTION_TIMEOUT: How long to wait for ANSWER responses during election
 * - Set to 5 seconds to handle wireless network delays and packet loss
 */
#ifndef ELECTION_TIMEOUT
#define ELECTION_TIMEOUT    (5 * CLOCK_SECOND)
#endif

/**
 * COORDINATOR_TIMEOUT: How long to wait before declaring coordinator dead
 * - Set to 10 seconds = ~1.25x ALIVE_INTERVAL (detect missed heartbeat quickly)
 */
#ifndef COORDINATOR_TIMEOUT
#define COORDINATOR_TIMEOUT (10 * CLOCK_SECOND)
#endif

/**
 * ALIVE_INTERVAL: How often coordinator sends ALIVE heartbeat messages
 * - Set to 5 seconds for standardized comparison across algorithms
 */
#ifndef ALIVE_INTERVAL
#define ALIVE_INTERVAL      (5 * CLOCK_SECOND)
#endif

/**
 * RANDOM_DELAY_MAX: Random startup delay to prevent synchronized elections
 */
#ifndef RANDOM_DELAY_MAX
#define RANDOM_DELAY_MAX    (5 * CLOCK_SECOND)
#endif

#endif /* BULLY_FAST_MODE */

/*---------------------------------------------------------------------------*/
/* MESSAGE TYPES - Bully Algorithm Messages */
/*---------------------------------------------------------------------------*/
#define MSG_ELECTION    1  /* I'm starting an election */
#define MSG_ANSWER      2  /* I have higher priority, back down */
#define MSG_COORDINATOR 3  /* I am the new coordinator */
#define MSG_ALIVE       4  /* I'm still alive (heartbeat) */

/*---------------------------------------------------------------------------*/
/* NODE STATE MACHINE */
/*---------------------------------------------------------------------------*/
typedef enum {
  STATE_NORMAL,                 /* Normal operation */
  STATE_ELECTION,               /* Election in progress */
  STATE_WAITING_COORDINATOR     /* Waiting for coordinator announcement */
} bully_state_t;

/*---------------------------------------------------------------------------*/
/* MESSAGE STRUCTURE */
/*---------------------------------------------------------------------------*/
typedef struct {
  uint8_t type;          /* Message type identifier */
  uint16_t node_id;      /* Sender's node ID (priority) */
  uint16_t target_id;    /* Target node (0=broadcast) */
  uint16_t sequence;     /* Sequence number for duplicate detection */
} bully_msg_t;

#endif /* BULLY_CONFIG_H_ */
