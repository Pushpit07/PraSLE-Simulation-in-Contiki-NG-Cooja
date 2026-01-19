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
 * BULLY ALGORITHM REFERENCE:
 * H. Garcia-Molina, "Elections in a Distributed Computing System,"
 * IEEE Transactions on Computers, vol. C-31, no. 1, pp. 48-59, Jan. 1982.
 * DOI: 10.1109/TC.1982.1675885
 *
 * ORIGINAL PAPER TIMING MODEL:
 * Garcia-Molina defines T as "the maximum time for a message to reach its
 * destination." The algorithm uses the following timeouts:
 *   - Election timeout: 2T (time for ELECTION msg + ANSWER response)
 *   - Coordinator wait: T (time to receive COORDINATOR announcement)
 *
 * The paper assumes synchronous systems where T is a known upper bound.
 * For asynchronous/lossy networks, T must account for:
 *   - Wireless propagation and MAC layer delays
 *   - Potential retransmissions due to packet loss
 *   - Multi-hop routing delays (if applicable)
 *
 * TIMING ADAPTATION FOR IoT/WSN:
 * For 802.15.4-based wireless sensor networks:
 *   - T (max message delay) = 2 seconds (conservative for lossy links)
 *   - Election timeout = 2T = 4 seconds
 *   - Coordinator timeout = 2T = 4 seconds (detect missed announcement)
 *   - Heartbeat interval = T = 2 seconds
 */

/**
 * ELECTION_TIMEOUT: How long to wait for ANSWER responses during election
 * - Garcia-Molina: 2T (round-trip for ELECTION + ANSWER)
 * - With T = 2s for IoT: 2T = 4 seconds
 */
#ifndef ELECTION_TIMEOUT
#define ELECTION_TIMEOUT    (4 * CLOCK_SECOND)
#endif

/**
 * COORDINATOR_TIMEOUT: How long to wait before declaring coordinator dead
 * - Garcia-Molina: Uses T for coordinator announcement wait
 * - Extended to 2T to tolerate one missed heartbeat in lossy networks
 * - With T = 2s: 2T = 4 seconds
 */
#ifndef COORDINATOR_TIMEOUT
#define COORDINATOR_TIMEOUT (4 * CLOCK_SECOND)
#endif

/**
 * ALIVE_INTERVAL: How often coordinator sends ALIVE heartbeat messages
 * - Set to T = 2 seconds (matches Garcia-Molina's message delay bound)
 * - Coordinator sends heartbeat every T to prove liveness
 */
#ifndef ALIVE_INTERVAL
#define ALIVE_INTERVAL      (2 * CLOCK_SECOND)
#endif

/**
 * RANDOM_DELAY_MAX: Random startup delay to prevent synchronized elections
 * - Garcia-Molina notes importance of avoiding simultaneous elections
 * - Set to T = 2 seconds
 */
#ifndef RANDOM_DELAY_MAX
#define RANDOM_DELAY_MAX    (2 * CLOCK_SECOND)
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
