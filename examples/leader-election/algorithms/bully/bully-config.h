/*
 * Bully Algorithm Configuration
 */

#ifndef BULLY_CONFIG_H_
#define BULLY_CONFIG_H_

/*---------------------------------------------------------------------------*/
/* TIMING CONFIGURATION - Tuned for wireless sensor networks */
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
 * - Set to 20 seconds = 2x ALIVE_INTERVAL + buffer
 */
#ifndef COORDINATOR_TIMEOUT
#define COORDINATOR_TIMEOUT (20 * CLOCK_SECOND)
#endif

/**
 * ALIVE_INTERVAL: How often coordinator sends ALIVE heartbeat messages
 * - Set to 8 seconds to balance failure detection with network traffic
 */
#ifndef ALIVE_INTERVAL
#define ALIVE_INTERVAL      (8 * CLOCK_SECOND)
#endif

/**
 * RANDOM_DELAY_MAX: Random startup delay to prevent synchronized elections
 */
#ifndef RANDOM_DELAY_MAX
#define RANDOM_DELAY_MAX    (5 * CLOCK_SECOND)
#endif

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
