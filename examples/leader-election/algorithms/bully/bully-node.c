/**
 * \file   bully-node.c
 * \brief  Bully Leader Election Algorithm for Contiki-NG
 * \author Pushpit Bhardwaj
 *
 * ALGORITHM OVERVIEW
 * ==================
 * The Bully Algorithm is a distributed leader election algorithm where:
 * - Each node has a unique ID (priority)
 * - The node with the HIGHEST ID becomes the coordinator (leader)
 * - When a node detects coordinator failure, it initiates an election
 *
 * ELECTION PROCESS
 * ================
 * 1. Node initiates election by broadcasting ELECTION message
 * 2. Nodes with HIGHER IDs respond with ANSWER message
 * 3. If no ANSWER received → Node becomes coordinator
 * 4. If ANSWER received → Node waits for COORDINATOR announcement
 * 5. Winner broadcasts COORDINATOR message to all nodes
 * 6. Coordinator periodically sends ALIVE messages as heartbeat
 *
 * MESSAGE FLOW EXAMPLE (6 nodes)
 * ===============================
 * Node 3 starts election:
 *   Node 3 → [ELECTION broadcast]
 *   Node 4 → [ANSWER to Node 3]  (I have higher ID)
 *   Node 5 → [ANSWER to Node 3]  (I have higher ID)
 *   Node 6 → [ANSWER to Node 3]  (I have higher ID)
 *   Node 3 → [Backs down, waits]
 *
 * Node 4, 5, 6 continue election...
 * Eventually Node 6 wins:
 *   Node 6 → [COORDINATOR broadcast]
 *   All nodes → [Accept Node 6 as leader]
 *   Node 6 → [Periodic ALIVE broadcasts]
 *
 * FAILURE DETECTION
 * =================
 * - Coordinator sends ALIVE every 8 seconds
 * - Nodes expect ALIVE within 20 seconds
 * - If timeout → Coordinator considered dead → New election
 *
 * DESIGN FEATURES
 * ===============
 * 1. Heartbeat-based failure detection with timer reset mechanism
 * 2. Duplicate message detection using sequence numbers
 * 3. Coordinator validation based on priority
 * 4. Carefully tuned timeouts for wireless network reliability
 * 5. Proper timer management across state transitions
 * 6. Self-message filtering to avoid broadcast echo
 * 7. Robust state machine implementation
 * 8. Partition healing via coordinator re-announcement and ALIVE-based discovery
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"
#include "sys/node-id.h"
#include "dev/moteid.h"
#include "random.h"
#include <string.h>
#include <stdio.h>

/* Common framework headers */
#include "election-common.h"
#include "election-metrics.h"

/* Algorithm-specific configuration */
#include "bully-config.h"

/*---------------------------------------------------------------------------*/
/* LOGGING CONFIGURATION */
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "Bully"
#define LOG_LEVEL LOG_LEVEL_INFO

/*---------------------------------------------------------------------------*/
/* UDP CONFIGURATION */
/*---------------------------------------------------------------------------*/
/**
 * UDP_PORT: Port number for Bully algorithm messages
 * - All nodes listen on this port for election messages
 * - Messages are sent to IPv6 multicast address for broadcast
 */
#define UDP_PORT ELECTION_UDP_PORT

/*---------------------------------------------------------------------------*/
/* GLOBAL STATE VARIABLES */
/*---------------------------------------------------------------------------*/
/**
 * Core algorithm state maintained by each node.
 * These variables track the current state of the election process.
 */

/** Current state in the Bully algorithm state machine */
static bully_state_t state = STATE_NORMAL;

/** This node's unique ID - used as priority (higher = higher priority) */
static uint16_t my_node_id;

/**
 * ID of the current coordinator/leader (0 = no known leader)
 * All nodes maintain this to know who the current leader is.
 * Updated when COORDINATOR message is received.
 */
static uint16_t current_leader = 0;

/**
 * Election sequence number - incremented for each new election.
 * Used to:
 * 1. Track election rounds
 * 2. Detect and filter duplicate messages
 * 3. Distinguish between old and new elections
 */
static uint16_t election_sequence = 0;

/**
 * Flag indicating if we received ANSWER during current election.
 * - false: No higher-priority node responded → We win election
 * - true: Higher-priority node exists → Back down and wait
 * Reset to false at the start of each election.
 */
static bool election_response_received = false;

/**
 * UDP connection for sending/receiving Bully algorithm messages
 * - Uses IPv6 link-local multicast (ff02::1) for broadcast-style communication
 * - Link-local multicast only reaches direct neighbors (single-hop)
 */
static struct simple_udp_connection udp_conn;

/*---------------------------------------------------------------------------*/
/* TIMER MANAGEMENT (Global for message handler access) */
/*---------------------------------------------------------------------------*/
/**
 * Timers are declared globally (not locally in PROCESS_THREAD) so that
 * message handlers can access and reset them. This is essential for the
 * ALIVE message handling mechanism where received heartbeats must reset
 * the coordinator timeout timer.
 */

/**
 * election_timer: Fires after ELECTION_TIMEOUT (5s)
 * - Purpose: Wait for ANSWER responses during election
 * - Started: When node initiates election
 * - Expires: Either we become coordinator (no ANSWER) or wait for COORDINATOR
 */
static struct etimer election_timer;

/**
 * coordinator_timer: Fires after COORDINATOR_TIMEOUT (20s)
 * - Purpose: Detect coordinator failure via missing ALIVE messages
 * - Reset: Every time ALIVE message is received (key design feature!)
 * - Expires: Coordinator is dead → Start new election
 *
 * IMPORTANT DESIGN: This timer is reset both:
 *   1. Periodically (every 20s by default)
 *   2. When ALIVE message received (prevents false failures)
 *
 * This dual-reset mechanism is what enables proper failure detection.
 */
static struct etimer coordinator_timer;

/**
 * alive_timer: Fires after ALIVE_INTERVAL (8s)
 * - Purpose: Coordinator sends periodic heartbeat
 * - Active: Only when this node is the coordinator
 * - Resets: Automatically after each ALIVE broadcast
 */
static struct etimer alive_timer;

#if ENABLE_METRICS
/**
 * metrics_timer: Fires after METRICS_OUTPUT_INTERVAL
 * - Purpose: Periodic output of metrics in CSV format
 * - Active: Always running when metrics are enabled
 */
static struct etimer metrics_timer;
#endif

/*---------------------------------------------------------------------------*/
/* DUPLICATE MESSAGE DETECTION */
/*---------------------------------------------------------------------------*/
/**
 * Tracks the last seen sequence number from each node.
 * Index: node_id - 1 (since node IDs start from 1)
 *
 * Purpose: Prevents processing duplicate messages that occur due to:
 * - Broadcast nature of wireless network
 * - Message retransmissions at MAC layer
 * - Multiple paths in mesh networks
 *
 * A message is considered duplicate if:
 *   last_seen_sequence[sender_id - 1] >= message.sequence
 */
static uint16_t last_seen_sequence[MAX_NODES] = {0};

/*---------------------------------------------------------------------------*/
/* CONTIKI-NG PROCESS DEFINITION */
/*---------------------------------------------------------------------------*/
PROCESS(bully_process, "Bully Leader Election");
AUTOSTART_PROCESSES(&bully_process);

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES */
/*---------------------------------------------------------------------------*/
static void send_message(uint8_t msg_type, uint16_t target_id, uint16_t sequence);
static bool is_duplicate_message(uint16_t sender_id, uint16_t sequence);
static void broadcast_message(uint8_t msg_type, uint16_t sequence);
static void start_election(void);
static void handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src);

/*---------------------------------------------------------------------------*/
/**
 * \brief Helper to convert bully state to common election state
 *
 * Maps algorithm-specific bully states to the common election states
 * used by the metrics infrastructure.
 */
/*---------------------------------------------------------------------------*/
static election_state_t
bully_to_common_state(bully_state_t bstate)
{
  switch(bstate) {
    case STATE_NORMAL:
      return (current_leader == my_node_id) ? ELECTION_STATE_LEADER : ELECTION_STATE_NORMAL;
    case STATE_ELECTION:
      return ELECTION_STATE_ELECTION;
    case STATE_WAITING_COORDINATOR:
      return ELECTION_STATE_WAITING;
    default:
      return ELECTION_STATE_INIT;
  }
}

/*===========================================================================*/
/*                      MESSAGE HANDLING FUNCTIONS                           */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/**
 * \brief Send a targeted message to a specific node
 *
 * \param msg_type   Type of message to send (MSG_ELECTION, MSG_ANSWER, etc.)
 * \param target_id  Recipient node ID
 * \param sequence   Sequence number for this message
 *
 * PURPOSE:
 * Used for directed messages, primarily MSG_ANSWER responses where we
 * want to notify a specific node that we have higher priority.
 *
 * Although we broadcast at the link layer (for simplicity), the target_id
 * field ensures only the intended recipient processes the message.
 */
/*---------------------------------------------------------------------------*/
static void
send_message(uint8_t msg_type, uint16_t target_id, uint16_t sequence)
{
  /* Static ensures the buffer persists after function returns */
  static bully_msg_t msg;

  /* Populate message fields */
  msg.type = msg_type;
  msg.node_id = my_node_id;         /* Who is sending this message */
  msg.target_id = target_id;        /* Who should process this message */
  msg.sequence = sequence;          /* Sequence number for this message */

  /* Log for debugging */
  LOG_INFO("Sending %s to node %u\n",
           msg_type == MSG_ELECTION ? "ELECTION" :
           msg_type == MSG_ANSWER ? "ANSWER" :
           msg_type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           target_id);

#if ENABLE_METRICS
  /* Track sent messages using common metrics infrastructure */
  metrics_track_message_sent(msg_type, sizeof(bully_msg_t));
#endif

  /* Send via UDP to link-local all-nodes multicast address */
  /* Link-local multicast (ff02::1) only reaches direct neighbors (single-hop) */
  uip_ipaddr_t dest_addr;
  uip_create_linklocal_allnodes_mcast(&dest_addr);
  simple_udp_sendto(&udp_conn, &msg, sizeof(msg), &dest_addr);
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Check if a message is a duplicate based on sequence number
 *
 * \param sender_id  Node ID of the message sender
 * \param sequence   Sequence number from the message
 *
 * \return true if message is duplicate (already processed), false otherwise
 *
 * HOW IT WORKS:
 * - Maintains per-node sequence tracking in last_seen_sequence[] array
 * - If we've seen this or a newer sequence from this sender → Duplicate
 * - Otherwise → New message, update tracking and return false
 *
 * WHY THIS IS NEEDED:
 * Wireless networks using broadcast can deliver the same message multiple
 * times due to network topology, MAC-layer retransmissions, or multi-path
 * routing. Processing duplicates would cause incorrect election behavior.
 */
/*---------------------------------------------------------------------------*/
static bool
is_duplicate_message(uint16_t sender_id, uint16_t sequence)
{
  /* Validate sender_id is in valid range */
  if (sender_id == 0 || sender_id > MAX_NODES) {
    return false; /* Invalid sender ID, treat as new message */
  }

  /* Convert node_id (1-based) to array index (0-based) */
  uint16_t idx = sender_id - 1;

  /* Check if we've already seen this sequence (or a newer one) */
  if (last_seen_sequence[idx] >= sequence) {
    return true; /* Already processed this or newer message from this node */
  }

  /* This is a new message - update our tracking */
  last_seen_sequence[idx] = sequence;
  return false; /* Not a duplicate */
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Broadcast a message to all nodes
 *
 * \param msg_type  Type of message to broadcast
 * \param sequence  Sequence number for this message
 *
 * USAGE:
 * - MSG_ELECTION: Start an election
 * - MSG_COORDINATOR: Announce yourself as the new leader
 * - MSG_ALIVE: Heartbeat from coordinator
 *
 * The target_id is set to 0 to indicate "all nodes".
 */
/*---------------------------------------------------------------------------*/
static void
broadcast_message(uint8_t msg_type, uint16_t sequence)
{
  /* Static ensures the buffer persists after function returns */
  static bully_msg_t msg;

  /* Populate message fields */
  msg.type = msg_type;
  msg.node_id = my_node_id;
  msg.target_id = 0;                /* 0 = broadcast to all */
  msg.sequence = sequence;          /* Sequence number for this message */

  /* Log for debugging */
  LOG_INFO("Broadcasting %s\n",
           msg_type == MSG_ELECTION ? "ELECTION" :
           msg_type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE");

#if ENABLE_METRICS
  /* Track sent messages using common metrics infrastructure */
  metrics_track_message_sent(msg_type, sizeof(bully_msg_t));
  if(msg_type == MSG_ALIVE) {
    metrics_track_heartbeat_sent();
  }
#endif

  /* Send via UDP to link-local all-nodes multicast address */
  /* Link-local multicast (ff02::1) only reaches direct neighbors (single-hop) */
  uip_ipaddr_t dest_addr;
  uip_create_linklocal_allnodes_mcast(&dest_addr);
  simple_udp_sendto(&udp_conn, &msg, sizeof(msg), &dest_addr);
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Initiate a new election
 *
 * PROCESS:
 * 1. Check if election already in progress (prevent concurrent elections)
 * 2. Transition to STATE_ELECTION
 * 3. Increment election sequence number (marks new election round)
 * 4. Broadcast ELECTION message to all higher-priority nodes
 *
 * WHO RESPONDS:
 * - Only nodes with node_id > my_node_id respond with ANSWER
 * - Lower priority nodes ignore the message (they can't win anyway)
 *
 * WHAT HAPPENS NEXT:
 * - Caller sets election_timer for ELECTION_TIMEOUT (5 seconds)
 * - Wait for ANSWER messages
 * - Timer expiry handled in main process loop
 */
/*---------------------------------------------------------------------------*/
static void
start_election(void)
{
  /* Prevent multiple concurrent elections from same node */
  if (state == STATE_ELECTION) {
    LOG_INFO("Election already in progress\n");
    return;
  }

  /* Log election start with sequence number */
  LOG_INFO("Starting election (sequence %u)\n", election_sequence + 1);

#if ENABLE_METRICS
  /* Track state and election start using common metrics */
  metrics_track_state(ELECTION_STATE_ELECTION);
  metrics_track_election_start();
#endif

  /* Update state and election tracking */
  state = STATE_ELECTION;
  election_sequence++;                /* Increment for new election round */
  election_response_received = false; /* Reset ANSWER flag */

  /* Broadcast ELECTION to all nodes */
  /* Higher-priority nodes (node_id > my_node_id) will respond with ANSWER */
  broadcast_message(MSG_ELECTION, election_sequence);

  /* Caller must set election_timer to wait for responses */
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Handle received messages from other nodes
 *
 * \param data  Pointer to received message data
 * \param len   Length of received data
 * \param src   Link-layer address of sender (unused, we use msg->node_id)
 *
 * This is the core message processing function. It handles all four message
 * types and implements the Bully algorithm logic.
 *
 * MESSAGE FLOW:
 * 1. Validate message size
 * 2. Filter out messages from ourselves (broadcast echo)
 * 3. Check for duplicates (except ALIVE - always process heartbeats)
 * 4. Process based on message type
 *
 * DESIGN PRINCIPLES:
 * - ALIVE messages reset coordinator_timer (enables proper failure detection)
 * - Coordinator validation based on priority (algorithm correctness)
 * - Timer-based election triggering (prevents cascade effects)
 * - Proper timer resets on state transitions (synchronization)
 */
/*---------------------------------------------------------------------------*/
static void
handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src)
{
  /* 1. VALIDATE MESSAGE SIZE */
  if (len != sizeof(bully_msg_t)) {
    LOG_WARN("Received message with wrong size\n");
    return;
  }

  /* Cast received data to our message structure */
  bully_msg_t *msg = (bully_msg_t *)data;
  uint16_t sender_id = msg->node_id;

  /* 2. FILTER SELF-MESSAGES */
  /* Due to broadcast nature, we receive our own messages - ignore them */
  if (sender_id == my_node_id) {
    return;
  }

  /* Log message receipt for debugging */
  LOG_INFO("Received %s from node %u (seq %u)\n",
           msg->type == MSG_ELECTION ? "ELECTION" :
           msg->type == MSG_ANSWER ? "ANSWER" :
           msg->type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           sender_id, msg->sequence);

  /* 3. CHECK FOR DUPLICATES (only for ELECTION messages) */
  /*
   * Only ELECTION messages use duplicate detection. Other message types are exempt:
   *
   * ALIVE messages: Always processed because heartbeat must always reset timer
   *
   * ANSWER messages: Always processed because:
   *   - They are targeted (target_id validation ensures correct recipient)
   *   - Multiple concurrent elections can use the same sequence number
   *
   * COORDINATOR messages: Always processed because:
   *   - Within same election, multiple message types share the same sequence
   *   - Preventing duplicates here would cause nodes to reject valid coordinators
   */
  if (msg->type != MSG_ALIVE && msg->type != MSG_ANSWER && msg->type != MSG_COORDINATOR &&
      is_duplicate_message(sender_id, msg->sequence)) {
    LOG_INFO("Ignoring duplicate message from node %u\n", sender_id);
#if ENABLE_METRICS
    metrics.algo.bully.duplicates_filtered++;
#endif
    return;
  }

#if ENABLE_METRICS
  /* Track received messages using common metrics infrastructure */
  metrics_track_message_recv(msg->type, sizeof(bully_msg_t));
#endif

  /* 4. PROCESS MESSAGE BASED ON TYPE */
  switch (msg->type) {

    /* ------------------------------------------------------------------- */
    case MSG_ELECTION:
      /*
       * ELECTION MESSAGE RECEIVED
       *
       * Someone is starting an election. We respond with ANSWER only if
       * we have HIGHER priority (higher node ID).
       *
       * LOGIC:
       * - Check if message is for us (broadcast or targeted)
       * - If my_node_id > sender_id → Send ANSWER (I have higher priority)
       * - If my_node_id < sender_id → Ignore (they have higher priority)
       *
       * DESIGN DECISION:
       * We do NOT automatically start our own election here. This design
       * choice prevents multiple simultaneous elections (cascade effect).
       *
       * PARTITION HEALING ENHANCEMENT (Mechanism 1):
       * If we are the current coordinator, we re-broadcast COORDINATOR
       * to help nodes that may have missed the original announcement.
       */
      if (msg->target_id == 0 || msg->target_id == my_node_id) {
        if (my_node_id > sender_id) {
          /* We have higher priority - tell them to back down */
          send_message(MSG_ANSWER, sender_id, msg->sequence);
          LOG_INFO("Sent ANSWER to node %u (I have higher priority)\n", sender_id);

          /* PARTITION HEALING (Mechanism 1): Coordinator re-announcement */
          if (current_leader == my_node_id) {
            LOG_INFO("Re-announcing coordinator status for partition healing\n");
#if ENABLE_METRICS
            metrics.algo.bully.coordinator_reannouncements++;
#endif
            broadcast_message(MSG_COORDINATOR, election_sequence);
          }
        }
      }
      break;

    /* ------------------------------------------------------------------- */
    case MSG_ANSWER:
      /*
       * ANSWER MESSAGE RECEIVED
       *
       * A higher-priority node has responded to our ELECTION message.
       * This means we cannot win the election and must back down.
       *
       * ACTIONS:
       * 1. Set election_response_received flag (we got a response)
       * 2. Transition to STATE_WAITING_COORDINATOR
       * 3. Reset coordinator_timer to wait for COORDINATOR announcement
       */
      if (msg->target_id == my_node_id && state == STATE_ELECTION) {
        election_response_received = true;
        LOG_INFO("Received ANSWER from node %u, backing down\n", sender_id);

#if ENABLE_METRICS
        /* Track election end (lost) and state transition */
        metrics_track_election_end(false);
        metrics_track_state(ELECTION_STATE_WAITING);
#endif

        /* Transition state */
        state = STATE_WAITING_COORDINATOR;

        /* Reset coordinator timer to wait for COORDINATOR announcement */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
      }
      break;

    /* ------------------------------------------------------------------- */
    case MSG_COORDINATOR:
      /*
       * COORDINATOR MESSAGE RECEIVED
       *
       * Someone is announcing themselves as the new coordinator/leader.
       * We must validate this claim before accepting.
       *
       * VALIDATION RULE:
       * Only accept coordinator if sender_id >= my_node_id
       *
       * RATIONALE:
       * In the Bully algorithm, only the HIGHEST priority node should be
       * leader. If a lower-priority node claims to be coordinator, it
       * indicates an error condition.
       */
      if (sender_id >= my_node_id) {
        /* Valid coordinator - accept */
        LOG_INFO("New coordinator: node %u\n", sender_id);

#if ENABLE_METRICS
        /* Track leader change and state transition */
        metrics_track_leader_change(sender_id);
        metrics_track_state(bully_to_common_state(STATE_NORMAL));
#endif

        current_leader = sender_id;
        state = STATE_NORMAL;

        /* Reset coordinator timer - we now have a valid leader to monitor */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

      } else {
        /* Invalid coordinator - they have lower priority than us! */
        LOG_WARN("Rejecting coordinator %u (lower priority than me)\n", sender_id);
#if ENABLE_METRICS
        metrics.algo.bully.invalid_coordinators++;
#endif

        /* Start election if not already in progress */
        if (state != STATE_ELECTION) {
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        }
      }
      break;

    /* ------------------------------------------------------------------- */
    case MSG_ALIVE:
      /*
       * ALIVE MESSAGE RECEIVED (Heartbeat)
       *
       * The coordinator sends these periodically to prove it's still functioning.
       * This is the key mechanism for failure detection in the Bully algorithm.
       *
       * KEY DESIGN PRINCIPLE:
       * Reset the coordinator_timer when we receive ALIVE from current leader.
       *
       * PARTITION HEALING ENHANCEMENT (Mechanism 2):
       * If we receive ALIVE from a higher-priority node, we may adopt them
       * as coordinator without requiring a COORDINATOR message.
       */
#if ENABLE_METRICS
      metrics_track_heartbeat_recv();
#endif

      /* PARTITION HEALING (Mechanism 2): ALIVE-based coordinator adoption */
      if (sender_id > my_node_id &&
          (current_leader == 0 ||
           state == STATE_WAITING_COORDINATOR ||
           sender_id > current_leader)) {
        LOG_INFO("Adopting node %u as coordinator (via ALIVE)\n", sender_id);

#if ENABLE_METRICS
        metrics.algo.bully.alive_adoptions++;
        metrics_track_leader_change(sender_id);
        metrics_track_state(bully_to_common_state(STATE_NORMAL));
#endif

        current_leader = sender_id;
        state = STATE_NORMAL;
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
      }
      /* STANDARD BEHAVIOR: Reset timer for current leader */
      else if (sender_id == current_leader) {
        LOG_INFO("Leader %u is alive\n", sender_id);
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
      }
      break;

    /* ------------------------------------------------------------------- */
    default:
      /* Unknown message type - log and ignore */
      LOG_WARN("Unknown message type: %u\n", msg->type);
      break;
  }
}

/*---------------------------------------------------------------------------*/
/**
 * \brief UDP receive callback
 *
 * This is registered as the UDP receive callback. It's called whenever
 * a UDP packet is received on our port. We simply forward to our message handler.
 */
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr,
                uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr,
                uint16_t receiver_port,
                const uint8_t *data,
                uint16_t datalen)
{
  handle_message(data, datalen, NULL);
}

/*===========================================================================*/
/*                       MAIN PROCESS IMPLEMENTATION                         */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/**
 * \brief Main Bully algorithm process
 *
 * INITIALIZATION SEQUENCE:
 * 1. Get node ID from Cooja simulator
 * 2. Initialize UDP connection for IPv6 communication
 * 3. Initialize common metrics infrastructure
 * 4. Random startup delay (prevent synchronized elections)
 * 5. Start initial election
 * 6. Enter event loop
 *
 * EVENT LOOP:
 * Process handles three timer events:
 * - election_timer: Election timeout (did we win?)
 * - coordinator_timer: Coordinator failure detection
 * - alive_timer: Send heartbeat (if we're coordinator)
 *
 * TIMER COORDINATION:
 * - All three timers run concurrently
 * - Message handlers can reset timers (especially coordinator_timer)
 * - Timers are global so they're accessible from handle_message()
 */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bully_process, ev, data)
{
  /* Local timer for random startup delay */
  static struct etimer random_delay_timer;

  PROCESS_BEGIN();

  /*=========================================================================*/
  /* INITIALIZATION PHASE */
  /*=========================================================================*/

  /* Get our node ID from Cooja simulator */
  my_node_id = simMoteID;
  if (my_node_id == 0) {
    my_node_id = 1; /* Ensure non-zero ID (0 is reserved for "no leader") */
  }

  LOG_INFO("Bully node %u starting\n", my_node_id);

  /* Initialize UDP connection for IPv6 communication */
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);
  LOG_INFO("UDP connection registered on port %d\n", UDP_PORT);

#if ENABLE_METRICS
  /* Initialize common metrics infrastructure */
  metrics_init();
  metrics_output_header();
#endif

  /*
   * RANDOM STARTUP DELAY
   *
   * Problem: If all nodes start simultaneously, they all start elections
   * at the same time, causing collision and confusion.
   *
   * Solution: Each node waits a random time (0-5 seconds) before starting.
   * This staggers the elections and reduces conflicts.
   */
  etimer_set(&random_delay_timer, random_rand() % RANDOM_DELAY_MAX);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&random_delay_timer));

  /* Start initial election after random delay */
  start_election();
  etimer_set(&election_timer, ELECTION_TIMEOUT);

  /* Start coordinator monitoring timer */
  etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

  /* Start alive timer (will only send when we're coordinator) */
  etimer_set(&alive_timer, ALIVE_INTERVAL);

#if ENABLE_METRICS
  /* Start metrics output timer */
  etimer_set(&metrics_timer, METRICS_OUTPUT_INTERVAL);
#endif

  /*=========================================================================*/
  /* MAIN EVENT LOOP */
  /*=========================================================================*/

  /*
   * Contiki-NG uses a protothread-based event system.
   * PROCESS_WAIT_EVENT() yields and waits for any event.
   * Events can be:
   * - Timer expiry
   * - Message received (handled in udp_rx_callback)
   * - Other system events
   */
  while (1) {
    PROCESS_WAIT_EVENT();

    /* TIMER EVENTS */
    if (ev == PROCESS_EVENT_TIMER) {

      /*---------------------------------------------------------------------*/
      /* ELECTION TIMER EXPIRED */
      /*---------------------------------------------------------------------*/
      if (data == &election_timer) {
        /*
         * We started an election ELECTION_TIMEOUT seconds ago.
         * Now we check: did any higher-priority nodes respond?
         *
         * CASE 1: No ANSWER received (election_response_received == false)
         *   → No higher-priority nodes exist
         *   → WE WIN THE ELECTION
         *   → Become coordinator
         *   → Broadcast COORDINATOR message
         *   → Start sending ALIVE heartbeats
         *
         * CASE 2: ANSWER received (election_response_received == true)
         *   → Higher-priority node exists
         *   → We backed down (already in STATE_WAITING_COORDINATOR)
         *   → Wait for COORDINATOR announcement
         */
        if (state == STATE_ELECTION || state == STATE_WAITING_COORDINATOR) {
          if (!election_response_received) {
            /* CASE 1: WE WON THE ELECTION */
            LOG_INFO("No responses received, becoming coordinator\n");

#if ENABLE_METRICS
            /* Track election won and state changes */
            metrics_track_election_end(true);
            metrics_track_leader_change(my_node_id);
            metrics_track_state(ELECTION_STATE_LEADER);
#endif

            /* Update our role */
            current_leader = my_node_id;
            state = STATE_NORMAL;

            /* Announce ourselves as coordinator */
            broadcast_message(MSG_COORDINATOR, election_sequence);

            /* Start sending periodic ALIVE heartbeats */
            etimer_reset(&alive_timer);

          } else {
            /* CASE 2: WE LOST (received ANSWER) */
            LOG_INFO("Election timer expired, waiting for coordinator\n");
          }
        }
      }

      /*---------------------------------------------------------------------*/
      /* COORDINATOR TIMER EXPIRED */
      /*---------------------------------------------------------------------*/
      else if (data == &coordinator_timer) {
        /*
         * COORDINATOR FAILURE DETECTION
         *
         * SCENARIO 1: We're waiting for coordinator announcement
         * SCENARIO 2: We have no known leader
         * SCENARIO 3: Current leader has died (stopped sending ALIVE)
         */
        if (state == STATE_WAITING_COORDINATOR || current_leader == 0) {
          LOG_INFO("No coordinator announcement, starting election\n");
#if ENABLE_METRICS
          metrics_track_timeout();
#endif
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        }
        else if (current_leader != my_node_id) {
          LOG_INFO("Coordinator %u timeout, starting election\n", current_leader);
#if ENABLE_METRICS
          metrics_track_timeout();
#endif
          current_leader = 0;
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        }
        /* Reset timer for next check period */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
      }

      /*---------------------------------------------------------------------*/
      /* ALIVE TIMER EXPIRED */
      /*---------------------------------------------------------------------*/
      else if (data == &alive_timer) {
        /*
         * TIME TO SEND HEARTBEAT
         *
         * If we are the coordinator, send ALIVE message to all nodes.
         * This proves we're still functioning and prevents unnecessary elections.
         */
        if (current_leader == my_node_id) {
          broadcast_message(MSG_ALIVE, election_sequence);
        }
        etimer_reset(&alive_timer);
      }

#if ENABLE_METRICS
      /*---------------------------------------------------------------------*/
      /* METRICS TIMER EXPIRED */
      /*---------------------------------------------------------------------*/
      else if (data == &metrics_timer) {
        /*
         * TIME TO OUTPUT METRICS
         *
         * Output current metrics snapshot in CSV format to console.
         * This data will be captured by the experiment automation scripts.
         */
        metrics_output();
        etimer_reset(&metrics_timer);
      }
#endif
    }
    /* Messages are handled via udp_rx_callback → handle_message() */
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
