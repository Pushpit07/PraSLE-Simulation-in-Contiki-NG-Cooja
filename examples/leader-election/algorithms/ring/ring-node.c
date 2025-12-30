/**
 * \file   ring-node.c
 * \brief  Ring Leader Election Algorithm for Contiki-NG
 * \author Pushpit Bhardwaj
 *
 * ALGORITHM OVERVIEW
 * ==================
 * The Ring Algorithm (Chang-Roberts) is a distributed leader election algorithm
 * where messages circulate around a logical ring. Unlike the Bully algorithm
 * (which uses broadcast), Ring uses point-to-point forwarding through a
 * predetermined ring topology.
 *
 * Key characteristics:
 * - Each node has a unique ID (priority)
 * - The node with the HIGHEST ID becomes the coordinator (leader)
 * - Messages travel unidirectionally around the ring
 * - Only one message type (ELECTION) is used during election
 *
 * RING TOPOLOGY
 * =============
 * Nodes form a logical ring: 1 -> 2 -> 3 -> ... -> N -> 1
 *
 *        Node 1
 *       /      \
 *    Node 5    Node 2
 *      |        |
 *    Node 4 -- Node 3
 *
 * Each node knows only its successor (next node in ring).
 *
 * ELECTION PROCESS
 * ================
 * 1. Node detects coordinator failure (via timeout)
 * 2. Node creates ELECTION message with:
 *    - initiator_id = my_node_id (who started this)
 *    - candidate_id = my_node_id (best candidate so far)
 * 3. Sends ELECTION to successor (next node in ring)
 * 4. Each receiving node:
 *    a. Compares its ID with candidate_id
 *    b. Updates candidate_id if its ID is higher
 *    c. Forwards message to its successor
 * 5. When ELECTION returns to initiator:
 *    - candidate_id contains the highest ID in the ring
 *    - That's the winner!
 * 6. Initiator sends COORDINATOR message with winner ID around ring
 * 7. All nodes update their current_leader
 *
 * MESSAGE FLOW EXAMPLE (5 nodes, Node 3 starts election)
 * =======================================================
 * Initial state: No leader (or leader failed)
 *
 * Step 1: Node 3 starts election
 *   Node 3 creates: ELECTION(initiator=3, candidate=3)
 *   Node 3 -> Node 4: ELECTION(3, 3)
 *
 * Step 2: Node 4 receives and forwards
 *   Node 4 compares: 4 > 3, updates candidate to 4
 *   Node 4 -> Node 5: ELECTION(3, 4)
 *
 * Step 3: Node 5 receives and forwards
 *   Node 5 compares: 5 > 4, updates candidate to 5
 *   Node 5 -> Node 1: ELECTION(3, 5)
 *
 * Step 4: Node 1 receives and forwards
 *   Node 1 compares: 1 < 5, keeps candidate 5
 *   Node 1 -> Node 2: ELECTION(3, 5)
 *
 * Step 5: Node 2 receives and forwards
 *   Node 2 compares: 2 < 5, keeps candidate 5
 *   Node 2 -> Node 3: ELECTION(3, 5)
 *
 * Step 6: Node 3 receives its own message back
 *   initiator_id == my_node_id, election complete!
 *   Winner is candidate_id = 5
 *   Node 3 -> Node 4: COORDINATOR(3, 5)
 *
 * Step 7: COORDINATOR travels the ring
 *   Each node sets current_leader = 5
 *   When returns to Node 3, announcement complete
 *
 * Step 8: Node 5 starts sending ALIVE heartbeats
 *   Node 5 -> Node 1 -> ... -> Node 5: ALIVE(5, 5)
 *
 * FAILURE DETECTION
 * =================
 * - Coordinator sends ALIVE message around ring every ALIVE_INTERVAL (12s)
 * - Each node has a coordinator_timer (20s timeout)
 * - When node receives ALIVE from current leader:
 *   - Reset coordinator_timer
 *   - Forward ALIVE to next node
 * - If coordinator_timer expires (no ALIVE received):
 *   - Coordinator is dead -> Start new election
 *
 * PARTITION HEALING MECHANISMS
 * ============================
 * This implementation includes partition healing for robustness:
 *
 * Mechanism 1: Coordinator Re-announcement
 *   - When: Coordinator receives ELECTION from any node
 *   - Action: Re-broadcasts COORDINATOR message around ring
 *   - Purpose: Nodes that missed original COORDINATOR can quickly learn leader
 *   - Benefit: Fast convergence after partition heals (<12s)
 *
 * Mechanism 2: ALIVE-based Coordinator Adoption
 *   - When: Node receives ALIVE from higher-priority node
 *   - Conditions: No leader OR waiting for coordinator OR sender > current_leader
 *   - Action: Adopt ALIVE sender as coordinator immediately
 *   - Purpose: Discover higher-priority coordinators from other partitions
 *   - Benefit: Automatic convergence without new election
 *
 * DESIGN FEATURES
 * ===============
 * 1. Ring-based sequential message passing (O(n) messages)
 * 2. Heartbeat-based failure detection via ALIVE messages
 * 3. Partition healing via coordinator re-announcement
 * 4. ALIVE-based coordinator adoption for fast partition recovery
 * 5. Proper timer management across state transitions
 * 6. Target-based message filtering (no broadcast echo issues)
 * 7. Sequence numbers to distinguish election rounds
 * 8. Metrics integration for experiment analysis
 *
 * COMPLEXITY ANALYSIS
 * ===================
 * Message Complexity: O(n) per election round
 * Time Complexity: O(n) message hops per election
 * Space Complexity: O(1) per node
 *
 * COMPARISON WITH BULLY ALGORITHM
 * ===============================
 * | Aspect        | Ring              | Bully            |
 * |---------------|-------------------|------------------|
 * | Messages      | O(n) per election | O(n^2) worst case|
 * | Communication | Point-to-point    | Broadcast        |
 * | Topology      | Ring required     | None required    |
 * | Failure       | Ring break fatal  | More resilient   |
 * | Network       | Less traffic      | More traffic     |
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
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
#include "ring-config.h"

/*---------------------------------------------------------------------------*/
/* LOGGING CONFIGURATION */
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "Ring"
#define LOG_LEVEL LOG_LEVEL_INFO

/*---------------------------------------------------------------------------*/
/* GLOBAL STATE VARIABLES */
/*---------------------------------------------------------------------------*/
/**
 * Core algorithm state maintained by each node.
 * These variables track the current state of the election process
 * and ring topology information.
 */

/**
 * Current state in the Ring algorithm state machine
 * - STATE_NORMAL: Operating normally with known leader
 * - STATE_ELECTION: Participating in ongoing election
 * - STATE_WAITING_COORDINATOR: Waiting for coordinator announcement
 */
static ring_state_t state = STATE_NORMAL;

/**
 * This node's unique ID - used as priority (higher = higher priority)
 * Obtained from Cooja simulator's simMoteID.
 */
static uint16_t my_node_id;

/**
 * ID of the current coordinator/leader (0 = no known leader)
 * Updated when:
 * - COORDINATOR message is received
 * - ALIVE-based adoption occurs (partition healing)
 */
static uint16_t current_leader = 0;

/**
 * Election sequence number - incremented for each new election.
 * Used to:
 * 1. Track election rounds
 * 2. Distinguish between concurrent elections
 * 3. Include in messages for debugging
 */
static uint16_t election_sequence = 0;

/**
 * ID of the next node in the ring (successor)
 * Ring topology: node_id -> next_node_id -> ...
 * Computed once at startup based on RING_SIZE.
 */
static uint16_t next_node_id = 0;

/**
 * Flag indicating if this node has an election in progress.
 * Used to prevent starting multiple concurrent elections.
 */
static bool election_in_progress = false;

/**
 * Flag indicating if we initiated the current election.
 * When true, we're waiting for our ELECTION message to return.
 */
static bool i_am_initiator = false;

/*---------------------------------------------------------------------------*/
/* TIMER MANAGEMENT (Global for message handler access) */
/*---------------------------------------------------------------------------*/
/**
 * Timers are declared globally (not locally in PROCESS_THREAD) so that
 * message handlers can access and reset them. This is essential for:
 * - Resetting coordinator_timer when ALIVE is received
 * - Managing election_timer during message handling
 */

/**
 * election_timer: Fires after ELECTION_TIMEOUT (12s)
 *
 * PURPOSE:
 * - Detect if election message is stuck (ring may be broken)
 * - Started when initiating election
 * - Reset when election completes successfully
 *
 * ON EXPIRY:
 * - Election message didn't return in time
 * - Ring may be broken -> Restart election
 */
static struct etimer election_timer;

/**
 * coordinator_timer: Fires after COORDINATOR_TIMEOUT (20s)
 *
 * PURPOSE:
 * - Detect coordinator failure via missing ALIVE messages
 * - Reset every time ALIVE message is received from current leader
 *
 * ON EXPIRY:
 * - No ALIVE received within timeout period
 * - Coordinator is dead -> Start new election
 *
 * DESIGN: This timer is reset in handle_message() when ALIVE is received,
 * enabling proper failure detection.
 */
static struct etimer coordinator_timer;

/**
 * alive_timer: Fires after ALIVE_INTERVAL (12s)
 *
 * PURPOSE:
 * - Coordinator sends periodic heartbeat around ring
 * - Only active when this node is the coordinator
 *
 * ON EXPIRY:
 * - If we're coordinator: Send ALIVE message to next node
 * - Resets automatically for next heartbeat
 */
static struct etimer alive_timer;

#if ENABLE_METRICS
/**
 * metrics_timer: Fires after METRICS_OUTPUT_INTERVAL
 *
 * PURPOSE:
 * - Periodic output of metrics in CSV format
 * - Active always when metrics are enabled
 */
static struct etimer metrics_timer;
#endif

/*---------------------------------------------------------------------------*/
/* CONTIKI-NG PROCESS DEFINITION */
/*---------------------------------------------------------------------------*/
PROCESS(ring_process, "Ring Leader Election");
AUTOSTART_PROCESSES(&ring_process);

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES */
/*---------------------------------------------------------------------------*/
static uint16_t get_next_node(uint16_t node_id);
static void send_to_next_node(uint8_t msg_type, uint16_t initiator,
                              uint16_t candidate, uint16_t sequence);
static void start_election(void);
static void handle_message(const uint8_t *data, uint16_t len,
                          const linkaddr_t *src);

/*---------------------------------------------------------------------------*/
/**
 * \brief Helper to convert ring state to common election state
 *
 * Maps algorithm-specific ring states to the common election states
 * used by the metrics infrastructure.
 *
 * \param rstate Algorithm-specific ring state
 * \return Corresponding common election state
 */
/*---------------------------------------------------------------------------*/
static election_state_t
ring_to_common_state(ring_state_t rstate)
{
  switch(rstate) {
    case STATE_NORMAL:
      /* Distinguish between leader and follower in NORMAL state */
      return (current_leader == my_node_id) ?
             ELECTION_STATE_LEADER : ELECTION_STATE_NORMAL;
    case STATE_ELECTION:
      return ELECTION_STATE_ELECTION;
    case STATE_WAITING_COORDINATOR:
      return ELECTION_STATE_WAITING;
    default:
      return ELECTION_STATE_INIT;
  }
}

/*===========================================================================*/
/*                        RING TOPOLOGY FUNCTIONS                            */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/**
 * \brief Get the next node in the ring (successor)
 *
 * \param node_id Current node's ID
 * \return ID of the next node in the ring
 *
 * RING TOPOLOGY:
 *   1 -> 2 -> 3 -> ... -> RING_SIZE -> 1 (wraps around)
 *
 * FORMULA:
 *   next_node = (node_id >= RING_SIZE) ? 1 : node_id + 1
 *
 * EXAMPLES (RING_SIZE = 5):
 *   get_next_node(1) = 2
 *   get_next_node(4) = 5
 *   get_next_node(5) = 1 (wraps around)
 */
/*---------------------------------------------------------------------------*/
static uint16_t
get_next_node(uint16_t node_id)
{
  /* Wrap around at RING_SIZE */
  if(node_id >= RING_SIZE) {
    return 1;  /* Wrap to first node */
  } else {
    return node_id + 1;  /* Next node in sequence */
  }
}

/*===========================================================================*/
/*                      MESSAGE HANDLING FUNCTIONS                           */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/**
 * \brief Send a message to the next node in the ring
 *
 * \param msg_type   Type of message (MSG_ELECTION, MSG_COORDINATOR, MSG_ALIVE)
 * \param initiator  Node that originally started this message chain
 * \param candidate  Current best candidate (highest ID seen)
 * \param sequence   Sequence number for this election
 *
 * PURPOSE:
 * Sends a ring message to our successor (next_node_id). The message is
 * broadcast at the link layer, but includes target_node_id so only the
 * intended recipient processes it.
 *
 * HOW IT WORKS:
 * 1. Construct message with all fields
 * 2. Set target_node_id to next_node_id
 * 3. Broadcast via NullNet
 * 4. Only target node processes the message
 *
 * WHY BROADCAST + FILTERING:
 * - NullNet doesn't support unicast
 * - All nodes receive the message
 * - target_node_id ensures only intended recipient processes it
 * - This simulates point-to-point ring communication
 */
/*---------------------------------------------------------------------------*/
static void
send_to_next_node(uint8_t msg_type, uint16_t initiator, uint16_t candidate,
                  uint16_t sequence)
{
  /* Static ensures the buffer persists after function returns */
  static ring_msg_t msg;

  /* Populate message fields */
  msg.type = msg_type;
  msg.initiator_id = initiator;
  msg.candidate_id = candidate;
  msg.sequence = sequence;
  msg.target_node_id = next_node_id;  /* Only next node should process */

  /* Log message sending for debugging */
  LOG_INFO("Sending %s to node %u (initiator=%u, candidate=%u, seq=%u)\n",
           msg_type == MSG_ELECTION ? "ELECTION" :
           msg_type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           next_node_id, initiator, candidate, sequence);

#if ENABLE_METRICS
  /* Track message sending using common metrics infrastructure */
  metrics_track_message_sent(msg_type, sizeof(ring_msg_t));
  metrics.algo.ring.forwards++;

  /* Track algorithm-specific message counts */
  switch(msg_type) {
    case MSG_ELECTION:
      metrics.algo.ring.msg_election_sent++;
      break;
    case MSG_COORDINATOR:
      metrics.algo.ring.msg_coordinator_sent++;
      break;
    case MSG_ALIVE:
      metrics.algo.ring.msg_alive_sent++;
      metrics_track_heartbeat_sent();
      break;
  }
#endif

  /* Send via NullNet (broadcast, filtered by target_node_id) */
  nullnet_buf = (uint8_t *)&msg;
  nullnet_len = sizeof(msg);
  NETSTACK_NETWORK.output(NULL);  /* NULL = broadcast to all neighbors */
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Initiate a new election
 *
 * PROCESS:
 * 1. Check if election already in progress (prevent concurrent elections)
 * 2. Update state to STATE_ELECTION
 * 3. Increment election sequence number
 * 4. Mark ourselves as initiator
 * 5. Send ELECTION message to next node with our ID as both initiator and candidate
 *
 * INITIATOR BEHAVIOR:
 * When we start an election, we set:
 *   initiator_id = my_node_id
 *   candidate_id = my_node_id
 *
 * When this message returns (initiator_id == my_node_id), we know:
 *   - Election has completed one full ring circuit
 *   - candidate_id contains the highest ID in the ring
 *   - That node is the winner
 *
 * WHAT HAPPENS NEXT:
 * - Caller sets election_timer for ELECTION_TIMEOUT
 * - Wait for ELECTION message to return
 * - On return: Determine winner, broadcast COORDINATOR
 */
/*---------------------------------------------------------------------------*/
static void
start_election(void)
{
  /* Prevent multiple concurrent elections from same node */
  if(election_in_progress) {
    LOG_INFO("Election already in progress, not starting new one\n");
    return;
  }

  /* Log election start */
  LOG_INFO("Starting ring election (sequence %u)\n", election_sequence + 1);

#if ENABLE_METRICS
  /* Track state transition and election start */
  metrics_track_state(ELECTION_STATE_ELECTION);
  metrics_track_election_start();
#endif

  /* Update state machine */
  state = STATE_ELECTION;
  election_sequence++;           /* New election round */
  election_in_progress = true;   /* Mark election active */
  i_am_initiator = true;         /* We started this election */

  /*
   * Send ELECTION message to next node
   * - initiator_id = my_node_id (we started this)
   * - candidate_id = my_node_id (we're the first candidate)
   *
   * As message travels the ring, each node updates candidate_id
   * if their ID is higher. When message returns to us, candidate_id
   * will contain the highest ID in the ring.
   */
  send_to_next_node(MSG_ELECTION, my_node_id, my_node_id, election_sequence);

  /* Caller must set election_timer to detect stuck elections */
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Handle received messages from other nodes
 *
 * \param data  Pointer to received message data
 * \param len   Length of received data
 * \param src   Link-layer address of sender (unused)
 *
 * This is the core message processing function. It handles all three message
 * types and implements the Ring algorithm logic plus partition healing.
 *
 * MESSAGE FILTERING:
 * - Messages are broadcast at link layer
 * - We only process if target_node_id == my_node_id
 * - This simulates point-to-point ring communication
 *
 * MESSAGE TYPES:
 * - MSG_ELECTION: Election message traveling the ring
 * - MSG_COORDINATOR: Leader announcement traveling the ring
 * - MSG_ALIVE: Heartbeat from coordinator traveling the ring
 *
 * PARTITION HEALING:
 * - Mechanism 1: Coordinator re-announces when receiving ELECTION
 * - Mechanism 2: Adopt higher-priority coordinator from ALIVE
 */
/*---------------------------------------------------------------------------*/
static void
handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src)
{
  /* 1. VALIDATE MESSAGE SIZE */
  if(len != sizeof(ring_msg_t)) {
    LOG_WARN("Received message with wrong size: %u (expected %u)\n",
             len, (unsigned)sizeof(ring_msg_t));
    return;
  }

  /* Cast received data to message structure */
  ring_msg_t *msg = (ring_msg_t *)data;

  /* 2. FILTER BY TARGET NODE */
  /*
   * Ring algorithm uses point-to-point communication.
   * We simulate this by broadcasting but filtering on target_node_id.
   * Only process messages intended for us.
   */
  if(msg->target_node_id != my_node_id) {
    return;  /* Not for us, ignore */
  }

  /* Log message receipt for debugging */
  LOG_INFO("Received %s (initiator=%u, candidate=%u, seq=%u)\n",
           msg->type == MSG_ELECTION ? "ELECTION" :
           msg->type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           msg->initiator_id, msg->candidate_id, msg->sequence);

#if ENABLE_METRICS
  /* Track message receipt using common metrics infrastructure */
  metrics_track_message_recv(msg->type, sizeof(ring_msg_t));

  /* Track algorithm-specific message counts */
  switch(msg->type) {
    case MSG_ELECTION:
      metrics.algo.ring.msg_election_recv++;
      break;
    case MSG_COORDINATOR:
      metrics.algo.ring.msg_coordinator_recv++;
      break;
    case MSG_ALIVE:
      metrics.algo.ring.msg_alive_recv++;
      break;
  }
#endif

  /* 3. PROCESS MESSAGE BASED ON TYPE */
  switch(msg->type) {

    /* ------------------------------------------------------------------- */
    case MSG_ELECTION:
      /*
       * ELECTION MESSAGE RECEIVED
       *
       * Two cases:
       * A) initiator_id == my_node_id: Our election returned!
       *    - Election complete, candidate_id is the winner
       *    - Announce winner via COORDINATOR message
       *
       * B) initiator_id != my_node_id: Forwarding someone else's election
       *    - Compare our ID with candidate
       *    - Update candidate if we have higher ID
       *    - Forward to next node
       */
      if(msg->initiator_id == my_node_id) {
        /*
         * CASE A: OUR ELECTION MESSAGE RETURNED
         *
         * The election message has traveled the full ring.
         * candidate_id now contains the highest ID in the ring.
         * That node is the winner!
         */
        LOG_INFO("Election completed! Winner is node %u\n", msg->candidate_id);

        /* Update state */
        election_in_progress = false;
        i_am_initiator = false;
        current_leader = msg->candidate_id;
        state = STATE_NORMAL;

#if ENABLE_METRICS
        /* Track ring completion and election result */
        metrics.algo.ring.ring_completions++;
        metrics_track_election_end(msg->candidate_id == my_node_id);
        metrics_track_leader_change(current_leader);
        metrics_track_state(ring_to_common_state(STATE_NORMAL));
#endif

        /*
         * ANNOUNCE NEW COORDINATOR
         *
         * Send COORDINATOR message around the ring to inform all nodes.
         * initiator_id = my_node_id (we're sending the announcement)
         * candidate_id = current_leader (the winner)
         */
        send_to_next_node(MSG_COORDINATOR, my_node_id, current_leader,
                          msg->sequence);

        /* If we're the new coordinator, start heartbeat timer */
        if(current_leader == my_node_id) {
          LOG_INFO("I am the new coordinator! Starting heartbeats.\n");
          etimer_set(&alive_timer, ALIVE_INTERVAL);
        }

        /* Reset coordinator timer - we have a valid leader now */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

      } else {
        /*
         * CASE B: FORWARDING SOMEONE ELSE'S ELECTION
         *
         * Compare our ID with current candidate:
         * - If our ID > candidate: We become the new candidate
         * - Otherwise: Keep current candidate
         * Then forward to next node.
         */
        uint16_t new_candidate = msg->candidate_id;

        if(my_node_id > msg->candidate_id) {
          /* We have higher priority - become the new candidate */
          new_candidate = my_node_id;
          LOG_INFO("Updating candidate from %u to %u (my ID is higher)\n",
                   msg->candidate_id, my_node_id);
        }

        /* Update our state - we're participating in election */
        state = STATE_ELECTION;
        election_in_progress = true;

#if ENABLE_METRICS
        metrics_track_state(ELECTION_STATE_ELECTION);
#endif

        /* Forward with original initiator but potentially updated candidate */
        send_to_next_node(MSG_ELECTION, msg->initiator_id, new_candidate,
                          msg->sequence);

        /*
         * PARTITION HEALING (Mechanism 1): Coordinator Re-announcement
         *
         * If we are currently the coordinator and receive an ELECTION,
         * it means some node doesn't know about us (possibly due to
         * partition healing). Re-announce ourselves.
         */
        if(current_leader == my_node_id) {
          LOG_INFO("Re-announcing coordinator status (partition healing)\n");
          send_to_next_node(MSG_COORDINATOR, my_node_id, my_node_id,
                            election_sequence);
        }
      }
      break;

    /* ------------------------------------------------------------------- */
    case MSG_COORDINATOR:
      /*
       * COORDINATOR MESSAGE RECEIVED
       *
       * Someone is announcing the new coordinator around the ring.
       *
       * Two cases:
       * A) initiator_id == my_node_id && current_leader == candidate_id:
       *    Our announcement returned, ring notification complete
       *
       * B) Otherwise: Accept coordinator and forward message
       */
      if(msg->initiator_id == my_node_id &&
         current_leader == msg->candidate_id) {
        /*
         * CASE A: OUR COORDINATOR ANNOUNCEMENT RETURNED
         *
         * The announcement has completed the full ring.
         * All nodes now know about the new coordinator.
         * Do NOT forward - this terminates the message.
         */
        LOG_INFO("Coordinator announcement completed the ring\n");

#if ENABLE_METRICS
        metrics.algo.ring.ring_completions++;
#endif
        /* Message terminates here */

      } else {
        /*
         * CASE B: ACCEPT AND FORWARD COORDINATOR ANNOUNCEMENT
         *
         * Update our local state with the new coordinator,
         * then forward the message to continue the ring circuit.
         */
        LOG_INFO("New coordinator announced: node %u\n", msg->candidate_id);

#if ENABLE_METRICS
        metrics_track_leader_change(msg->candidate_id);
        metrics_track_state(ring_to_common_state(STATE_NORMAL));
#endif

        /* Accept new coordinator */
        current_leader = msg->candidate_id;
        state = STATE_NORMAL;
        election_in_progress = false;
        i_am_initiator = false;

        /* Reset coordinator timer - we have a valid leader now */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

        /* Forward coordinator message to continue ring circuit */
        send_to_next_node(MSG_COORDINATOR, msg->initiator_id,
                          msg->candidate_id, msg->sequence);
      }
      break;

    /* ------------------------------------------------------------------- */
    case MSG_ALIVE:
      /*
       * ALIVE MESSAGE RECEIVED (Heartbeat)
       *
       * The coordinator sends these periodically around the ring
       * to prove it's still functioning.
       *
       * Three cases:
       * A) Our ALIVE returned (we're coordinator): Ring circuit complete
       * B) ALIVE from current leader: Reset timer and forward
       * C) ALIVE from unknown/higher-priority node: Partition healing
       */
#if ENABLE_METRICS
      metrics_track_heartbeat_recv();
#endif

      if(msg->initiator_id == my_node_id && current_leader == my_node_id) {
        /*
         * CASE A: OUR ALIVE MESSAGE RETURNED (WE ARE COORDINATOR)
         *
         * The heartbeat has completed the full ring.
         * All nodes have received proof we're alive.
         * Do NOT forward - this terminates the message.
         */
        LOG_INFO("ALIVE message completed the ring (I am coordinator)\n");

#if ENABLE_METRICS
        metrics.algo.ring.ring_completions++;
#endif
        /* Message terminates here */

      } else if(msg->initiator_id == current_leader) {
        /*
         * CASE B: ALIVE FROM CURRENT LEADER
         *
         * Standard heartbeat processing:
         * 1. Reset coordinator timer (leader is alive)
         * 2. Forward message to continue ring circuit
         */
        LOG_INFO("Leader %u is alive - forwarding heartbeat\n",
                 current_leader);

        /* Reset coordinator timer - leader is alive */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

        /* Forward ALIVE to next node */
        send_to_next_node(MSG_ALIVE, msg->initiator_id, msg->candidate_id,
                          msg->sequence);

      } else if(msg->initiator_id > my_node_id &&
                (current_leader == 0 ||
                 state == STATE_WAITING_COORDINATOR ||
                 msg->initiator_id > current_leader)) {
        /*
         * CASE C: PARTITION HEALING (Mechanism 2) - ALIVE-based Adoption
         *
         * We received ALIVE from a higher-priority node that:
         * - We don't know as leader (current_leader == 0)
         * - OR we're waiting for coordinator
         * - OR has higher priority than our current leader
         *
         * This happens when:
         * - Partitions heal and we discover a better leader
         * - We missed the COORDINATOR announcement
         *
         * Action: Adopt this node as our coordinator immediately
         * without requiring a new election.
         */
        LOG_INFO("Adopting node %u as coordinator via ALIVE (partition healing)\n",
                 msg->initiator_id);

#if ENABLE_METRICS
        metrics_track_leader_change(msg->initiator_id);
        metrics_track_state(ring_to_common_state(STATE_NORMAL));
#endif

        /* Adopt sender as coordinator */
        current_leader = msg->initiator_id;
        state = STATE_NORMAL;
        election_in_progress = false;
        i_am_initiator = false;

        /* Reset coordinator timer */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

        /* Forward ALIVE to continue ring circuit */
        send_to_next_node(MSG_ALIVE, msg->initiator_id, msg->candidate_id,
                          msg->sequence);

      } else if(current_leader != 0 && msg->initiator_id != current_leader) {
        /*
         * ALIVE from non-leader node (and we already have a leader)
         *
         * This could happen if:
         * - Multiple nodes think they're coordinator (shouldn't happen normally)
         * - Old ALIVE message from previous coordinator
         *
         * Don't forward - let it die. The correct leader's ALIVE will continue.
         */
        LOG_WARN("Received ALIVE from node %u but current leader is %u - ignoring\n",
                 msg->initiator_id, current_leader);
      } else {
        /*
         * No known leader and sender doesn't have higher priority
         * Forward anyway to help the message complete its circuit
         */
        LOG_INFO("No leader known, forwarding ALIVE from %u\n",
                 msg->initiator_id);
        send_to_next_node(MSG_ALIVE, msg->initiator_id, msg->candidate_id,
                          msg->sequence);
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
 * \brief NullNet input callback
 *
 * This is registered as the NullNet receive callback. It's called whenever
 * a packet is received. We simply forward to our message handler.
 *
 * \param data Received data buffer
 * \param len  Length of received data
 * \param src  Source link-layer address
 * \param dest Destination link-layer address (usually broadcast)
 */
/*---------------------------------------------------------------------------*/
static void
input_callback(const void *data, uint16_t len,
               const linkaddr_t *src, const linkaddr_t *dest)
{
  handle_message((const uint8_t *)data, len, src);
}

/*===========================================================================*/
/*                       MAIN PROCESS IMPLEMENTATION                         */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
/**
 * \brief Main Ring algorithm process
 *
 * INITIALIZATION SEQUENCE:
 * 1. Get node ID from Cooja simulator
 * 2. Compute successor (next node in ring)
 * 3. Initialize NullNet for communication
 * 4. Initialize metrics infrastructure
 * 5. Random startup delay (prevent synchronized elections)
 * 6. Highest ID node starts initial election
 * 7. Enter main event loop
 *
 * EVENT LOOP:
 * Process handles three timer events:
 * - election_timer: Election stuck detection
 * - coordinator_timer: Coordinator failure detection
 * - alive_timer: Send heartbeat (if we're coordinator)
 *
 * DESIGN DECISION - WHO STARTS INITIAL ELECTION:
 * The node with the highest ID (node RING_SIZE) starts the initial election.
 * This ensures only one election starts at boot time, reducing message
 * overhead. Other nodes wait for coordinator announcement.
 */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ring_process, ev, data)
{
  /* Local timer for random startup delay */
  static struct etimer random_delay_timer;

  PROCESS_BEGIN();

  /*=========================================================================*/
  /* INITIALIZATION PHASE */
  /*=========================================================================*/

  /* Get our node ID from Cooja simulator */
  my_node_id = simMoteID;
  if(my_node_id == 0) {
    my_node_id = 1;  /* Ensure non-zero ID (0 is reserved for "no leader") */
  }

  /* Compute our successor in the ring */
  next_node_id = get_next_node(my_node_id);

  LOG_INFO("Ring node %u starting (successor: %u, ring size: %u)\n",
           my_node_id, next_node_id, RING_SIZE);

  /*
   * INITIALIZE NULLNET
   *
   * NullNet is a minimal network layer for Contiki-NG.
   * - No routing overhead
   * - Direct broadcast communication
   * - We filter by target_node_id to simulate ring topology
   */
  nullnet_buf = NULL;
  nullnet_len = 0;
  nullnet_set_input_callback(input_callback);

  LOG_INFO("NullNet initialized\n");

#if ENABLE_METRICS
  /* Initialize common metrics infrastructure */
  metrics_init();
  metrics_output_header();
#endif

  /*
   * RANDOM STARTUP DELAY
   *
   * Problem: If all nodes start simultaneously, they might all try to
   * start elections at the same time, causing confusion.
   *
   * Solution: Each node waits a random time (0 to RANDOM_DELAY_MAX seconds)
   * before checking if it should start an election.
   */
  etimer_set(&random_delay_timer, random_rand() % RANDOM_DELAY_MAX);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&random_delay_timer));

  /*
   * INITIAL ELECTION STRATEGY
   *
   * Only the node with the highest ID (RING_SIZE) starts the initial
   * election. This ensures:
   * - Exactly one election starts at boot time
   * - Minimizes message overhead
   * - Other nodes learn the leader via COORDINATOR message
   *
   * WHY HIGHEST ID?
   * - That node will win anyway (highest priority)
   * - Reduces unnecessary message forwarding
   * - Faster convergence to stable state
   */
  if(my_node_id == RING_SIZE) {
    LOG_INFO("I am the highest-ID node (%u), starting initial election\n",
             my_node_id);
    start_election();
    etimer_set(&election_timer, ELECTION_TIMEOUT);
  } else {
    LOG_INFO("Waiting for election from highest-ID node\n");
  }

  /* Start coordinator monitoring timer */
  etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

  /* Start alive timer (only sends when we're coordinator) */
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
   * - Timer expiry (handled below)
   * - Message received (handled in input_callback -> handle_message)
   */
  while(1) {
    PROCESS_WAIT_EVENT();

    /* TIMER EVENTS */
    if(ev == PROCESS_EVENT_TIMER) {

      /*---------------------------------------------------------------------*/
      /* ELECTION TIMER EXPIRED */
      /*---------------------------------------------------------------------*/
      if(data == &election_timer) {
        /*
         * ELECTION TIMEOUT
         *
         * If we initiated an election and it hasn't completed, the message
         * might be stuck (ring broken, node failed, etc.).
         *
         * Action: Restart election to try again
         */
        if(election_in_progress && i_am_initiator) {
          LOG_INFO("Election timeout - message may be stuck, restarting\n");

#if ENABLE_METRICS
          metrics_track_timeout();
#endif

          /* Reset state and start new election */
          election_in_progress = false;
          i_am_initiator = false;
          start_election();
          etimer_reset(&election_timer);
        }
      }

      /*---------------------------------------------------------------------*/
      /* COORDINATOR TIMER EXPIRED */
      /*---------------------------------------------------------------------*/
      else if(data == &coordinator_timer) {
        /*
         * COORDINATOR FAILURE DETECTION
         *
         * We haven't received ALIVE from coordinator within timeout period.
         * This indicates:
         * - Coordinator has failed
         * - OR network partition (we can't reach coordinator)
         * - OR we never had a coordinator
         *
         * Action: Start new election
         *
         * CASES:
         * 1. No known leader (current_leader == 0): Start election
         * 2. Known leader but not us: Leader failed, start election
         * 3. We are the leader: We're fine, just reset timer
         */
        if(current_leader == 0) {
          /* No leader known - start election */
          LOG_INFO("No coordinator known, starting election\n");

#if ENABLE_METRICS
          metrics_track_timeout();
#endif

          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);

        } else if(current_leader != my_node_id && !election_in_progress) {
          /* We have a leader but haven't heard from them - they may be dead */
          LOG_INFO("Coordinator %u timeout, starting new election\n",
                   current_leader);

#if ENABLE_METRICS
          metrics_track_timeout();
#endif

          current_leader = 0;  /* Clear old leader */
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        }
        /* else: We are coordinator or election in progress - do nothing */

        /* Reset timer for next check period */
        etimer_reset(&coordinator_timer);
      }

      /*---------------------------------------------------------------------*/
      /* ALIVE TIMER EXPIRED */
      /*---------------------------------------------------------------------*/
      else if(data == &alive_timer) {
        /*
         * TIME TO SEND HEARTBEAT
         *
         * If we are the coordinator, send ALIVE message around the ring.
         * This proves to all nodes that we're still functioning.
         *
         * The ALIVE message travels the full ring:
         *   Coordinator -> Node1 -> Node2 -> ... -> Coordinator
         *
         * Each node resets its coordinator_timer upon receipt.
         */
        if(current_leader == my_node_id) {
          LOG_INFO("Sending ALIVE heartbeat around ring\n");
          send_to_next_node(MSG_ALIVE, my_node_id, my_node_id, election_sequence);
        }
        etimer_reset(&alive_timer);
      }

#if ENABLE_METRICS
      /*---------------------------------------------------------------------*/
      /* METRICS TIMER EXPIRED */
      /*---------------------------------------------------------------------*/
      else if(data == &metrics_timer) {
        /*
         * TIME TO OUTPUT METRICS
         *
         * Output current metrics snapshot in CSV format to console.
         * This data is captured by experiment automation scripts.
         */
        metrics_output();
        etimer_reset(&metrics_timer);
      }
#endif
    }
    /* Messages are handled via input_callback -> handle_message() */
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
