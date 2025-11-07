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

/*---------------------------------------------------------------------------*/
/* LOGGING CONFIGURATION */
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "Bully"
#define LOG_LEVEL LOG_LEVEL_INFO

/*---------------------------------------------------------------------------*/
/* MESSAGE TYPES - Bully Algorithm Messages */
/*---------------------------------------------------------------------------*/
/**
 * MSG_ELECTION: "I'm starting an election, respond if you have higher priority"
 * Sent when a node detects coordinator failure or starts initial election.
 * Higher-priority nodes respond with ANSWER.
 */
#define MSG_ELECTION    1

/**
 * MSG_ANSWER: "I have higher priority than you, back down"
 * Sent in response to ELECTION when responder has higher node ID.
 * Tells the election initiator to back down and wait for coordinator announcement.
 */
#define MSG_ANSWER      2

/**
 * MSG_COORDINATOR: "I am the new coordinator/leader"
 * Broadcast by the election winner to announce itself as the new leader.
 * All nodes accept this and update their current_leader.
 */
#define MSG_COORDINATOR 3

/**
 * MSG_ALIVE: "I'm still alive and functioning as coordinator"
 * Periodic heartbeat from coordinator to prove it's still operational.
 * Prevents unnecessary elections by resetting timeout on follower nodes.
 */
#define MSG_ALIVE       4

/*---------------------------------------------------------------------------*/
/* TIMING CONFIGURATION - Tuned for wireless sensor networks */
/*---------------------------------------------------------------------------*/
/**
 * ELECTION_TIMEOUT: How long to wait for ANSWER responses during election
 * - Set to 5 seconds to handle wireless network delays and packet loss
 * - Must be long enough for all higher-priority nodes to respond
 * - Too short → Valid responses missed, wrong leader elected
 * - Too long → Slow leader election
 */
#define ELECTION_TIMEOUT    (5 * CLOCK_SECOND)

/**
 * COORDINATOR_TIMEOUT: How long to wait before declaring coordinator dead
 * - Set to 20 seconds = 2× ALIVE_INTERVAL + buffer
 * - This allows for one missed ALIVE message plus network delays
 * - CRITICAL: Must be > ALIVE_INTERVAL to avoid false-positive failures
 * - Too short → Unnecessary elections (election storms)
 * - Too long → Slow failure detection
 *
 * FORMULA: COORDINATOR_TIMEOUT ≥ 2× ALIVE_INTERVAL + network_delay_buffer
 */
#define COORDINATOR_TIMEOUT (20 * CLOCK_SECOND)

/**
 * ALIVE_INTERVAL: How often coordinator sends ALIVE heartbeat messages
 * - Set to 8 seconds to balance failure detection with network traffic
 * - Must be < COORDINATOR_TIMEOUT/2 to ensure timely detection
 * - Coordinator sends ALIVE every 8s, followers timeout after 20s
 * - Too short → Excessive network traffic, higher power consumption
 * - Too long → Slow failure detection
 */
#define ALIVE_INTERVAL      (8 * CLOCK_SECOND)

/**
 * RANDOM_DELAY_MAX: Random startup delay to prevent synchronized elections
 * - Set to 5 seconds to adequately spread out initial elections
 * - When all nodes start simultaneously, random delay prevents collision
 * - Each node waits 0-5 seconds before starting initial election
 * - Reduces the likelihood of multiple concurrent elections at startup
 */
#define RANDOM_DELAY_MAX    (5 * CLOCK_SECOND)

/*---------------------------------------------------------------------------*/
/* UDP CONFIGURATION */
/*---------------------------------------------------------------------------*/
/**
 * UDP_PORT: Port number for Bully algorithm messages
 * - All nodes listen on this port for election messages
 * - Messages are sent to IPv6 multicast address for broadcast
 */
#define UDP_PORT 8765

/*---------------------------------------------------------------------------*/
/* NODE STATE MACHINE */
/*---------------------------------------------------------------------------*/
/**
 * The Bully algorithm operates as a state machine with three distinct states:
 *
 * STATE_NORMAL:
 *   - Normal operation, coordinator is known and alive
 *   - Followers monitor coordinator via ALIVE messages
 *   - Coordinator sends periodic ALIVE messages
 *   - Most time is spent in this state (stable operation)
 *
 * STATE_ELECTION:
 *   - Election in progress, waiting for ANSWER responses
 *   - Node has broadcast ELECTION message
 *   - If no ANSWER received within ELECTION_TIMEOUT → Become coordinator
 *   - If ANSWER received → Transition to STATE_WAITING_COORDINATOR
 *
 * STATE_WAITING_COORDINATOR:
 *   - Received ANSWER, backed down from election
 *   - Waiting for COORDINATOR announcement from election winner
 *   - If no COORDINATOR within COORDINATOR_TIMEOUT → Start new election
 *
 * State Transition Diagram:
 *
 *          [START]
 *             ↓
 *      STATE_NORMAL ←──────────────────┐
 *             ↓                        │
 *       (detect failure)               │
 *             ↓                        │
 *      STATE_ELECTION                  │
 *        ↓          ↓                  │
 *   (no ANSWER) (ANSWER received)      │
 *        ↓          ↓                  │
 *   (become    STATE_WAITING_          │
 *    leader)     COORDINATOR           │
 *        ↓          ↓                  │
 *        └────(COORDINATOR msg)────────┘
 */
typedef enum {
  STATE_NORMAL,                 /* Normal operation */
  STATE_ELECTION,               /* Election in progress */
  STATE_WAITING_COORDINATOR     /* Waiting for coordinator announcement */
} bully_state_t;

/*---------------------------------------------------------------------------*/
/* MESSAGE STRUCTURE */
/*---------------------------------------------------------------------------*/
/**
 * Wire format for all Bully algorithm messages.
 * All fields are in host byte order (Contiki handles endianness internally).
 *
 * Fields:
 *   type: Message type (MSG_ELECTION, MSG_ANSWER, MSG_COORDINATOR, MSG_ALIVE)
 *   node_id: ID of sender node (used for priority comparison)
 *   target_id: Recipient node ID (0 = broadcast to all nodes)
 *   sequence: Election sequence number (for duplicate detection)
 */
typedef struct {
  uint8_t type;          /* Message type identifier */
  uint16_t node_id;      /* Sender's node ID (priority) */
  uint16_t target_id;    /* Target node (0=broadcast) */
  uint16_t sequence;     /* Sequence number for duplicate detection */
} bully_msg_t;

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
/* TIMER MANAGEMENT */
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

/*---------------------------------------------------------------------------*/
/* DUPLICATE MESSAGE DETECTION */
/*---------------------------------------------------------------------------*/
/**
 * Maximum number of nodes supported in the network.
 * Used to size the sequence tracking array.
 * Adjust if your network has more than 10 nodes.
 */
#define MAX_NODES 10

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
 *
 * NOTE: In a production system with unicast capability, we might optimize
 * this to use true unicast to reduce network traffic.
 */
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
 * routing. Processing duplicates would cause incorrect election behavior,
 * such as responding to the same ELECTION message multiple times or
 * incorrectly detecting multiple coordinators.
 *
 * EXAMPLE:
 *   Node 3 broadcasts ELECTION (seq=5)
 *   Due to network topology, Node 6 receives it twice
 *   First reception: last_seen_sequence[2]=0, seq=5 → Process (update to 5)
 *   Second reception: last_seen_sequence[2]=5, seq=5 → Ignore (duplicate)
 */
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
   *   - Example: Node 4 receives "ANSWER seq=1" from Node 5 to Node 2,
   *     then receives "ANSWER seq=1" from Node 5 to Node 4 - both are valid
   *
   * COORDINATOR messages: Always processed because:
   *   - Within same election, multiple message types share the same sequence
   *   - Example: Node 3 receives "ELECTION seq=1" from Node 6, then
   *     "COORDINATOR seq=1" from Node 6 - both valid, different message types
   *   - Preventing duplicates here would cause nodes to reject valid coordinators
   */
  if (msg->type != MSG_ALIVE && msg->type != MSG_ANSWER && msg->type != MSG_COORDINATOR &&
      is_duplicate_message(sender_id, msg->sequence)) {
    LOG_INFO("Ignoring duplicate message from node %u (seq %u)\n",
             sender_id, msg->sequence);
    return;
  }

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
       * - If my_node_id == sender_id → Impossible (can't receive from self)
       *
       * DESIGN DECISION:
       * We do NOT automatically start our own election here. This design
       * choice prevents multiple simultaneous elections (cascade effect).
       * Instead, we rely on:
       * - Our own election_timer if we're in an election
       * - coordinator_timer if coordinator fails
       *
       * This timer-based approach ensures controlled, serialized elections
       * rather than having every ANSWER trigger a new election.
       *
       * PARTITION HEALING ENHANCEMENT (Mechanism 1):
       * If we are the current coordinator, we re-broadcast COORDINATOR
       * to help nodes that may have missed the original announcement.
       *
       * This mechanism handles the scenario where:
       * - A node was out of radio range when we became coordinator
       * - The node later moves into our range and starts an election
       * - We respond with both ANSWER and COORDINATOR re-announcement
       * - The node immediately recognizes us as coordinator without waiting
       *
       * Without this, the node would wait COORDINATOR_TIMEOUT (20s) before
       * timing out and potentially electing itself, causing temporary
       * split-brain behavior even though we're reachable.
       */
      if (msg->target_id == 0 || msg->target_id == my_node_id) {
        if (my_node_id > sender_id) {
          /* We have higher priority - tell them to back down */
          send_message(MSG_ANSWER, sender_id, msg->sequence);
          LOG_INFO("Sent ANSWER to node %u (I have higher priority)\n", sender_id);

          /* PARTITION HEALING (Mechanism 1): Coordinator re-announcement
           * If we're the coordinator and receive an ELECTION from a lower-priority
           * node, immediately re-broadcast COORDINATOR so they can adopt us without
           * waiting for coordinator_timer to expire (saves 20 seconds). */
          if (current_leader == my_node_id) {
            LOG_INFO("Re-announcing coordinator status to help partition healing\n");
            broadcast_message(MSG_COORDINATOR, election_sequence);
          }

          /* Note: We do NOT start our own election here.
           * Elections are started by:
           * - Initial startup (after random delay)
           * - Coordinator timeout (failure detection)
           * - Invalid coordinator rejection
           *
           * This prevents election storms where every higher-priority node
           * receiving an ELECTION immediately starts their own election.
           */
        }
        /* If my_node_id < sender_id, we ignore (they have higher priority) */
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
       *
       * VALIDATION:
       * - Only process if message is targeted to us (not broadcast)
       * - Only process if we're currently in STATE_ELECTION
       *
       * NEXT STEP:
       * Wait for COORDINATOR message from the election winner.
       * If no COORDINATOR received within COORDINATOR_TIMEOUT, we'll
       * detect the failure and start a new election.
       */
      if (msg->target_id == my_node_id && state == STATE_ELECTION) {
        election_response_received = true;
        LOG_INFO("Received ANSWER from node %u, backing down from election\n", sender_id);

        /* Transition state */
        state = STATE_WAITING_COORDINATOR;

        /* Reset coordinator timer to wait for COORDINATOR announcement */
        /* If winner doesn't announce within timeout, we'll detect it */
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
       * indicates an error condition (network partition, race condition,
       * or malicious behavior). We reject such claims and start our own
       * election since we have higher priority.
       *
       * ACCEPTANCE CASE (sender_id >= my_node_id):
       * 1. Update current_leader
       * 2. Transition to STATE_NORMAL
       * 3. Reset failed_election_count (successful election)
       * 4. Reset coordinator_timer (start monitoring new leader)
       *
       * REJECTION CASE (sender_id < my_node_id):
       * 1. Log warning
       * 2. Increment failed_election_count (invalid coordinator = failure)
       * 3. Start new election (we have higher priority)
       *
       * EXAMPLE:
       *   My node: 5
       *   Sender: 6 → Accept (6 > 5)
       *   Sender: 5 → Accept (5 == 5, could be us after network partition)
       *   Sender: 4 → Reject and start election (4 < 5, we should be leader)
       */
      if (sender_id >= my_node_id) {
        /* Valid coordinator - accept */
        LOG_INFO("New coordinator: node %u\n", sender_id);
        current_leader = sender_id;
        state = STATE_NORMAL;

        /* Reset coordinator timer - we now have a valid leader to monitor */
        /* This timer will fire if we stop receiving ALIVE messages */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

      } else {
        /* Invalid coordinator - they have lower priority than us! */
        LOG_WARN("Rejecting coordinator %u (lower priority than me)\n", sender_id);

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
       * The coordinator sends these periodically (every ALIVE_INTERVAL)
       * to prove it's still functioning. This is the key mechanism for
       * failure detection in the Bully algorithm.
       *
       * KEY DESIGN PRINCIPLE:
       * Reset the coordinator_timer when we receive ALIVE from current leader.
       *
       * WHY THIS IS ESSENTIAL:
       * The coordinator_timer is our failure detection mechanism. If we
       * didn't reset it when receiving ALIVE messages, the timer would
       * expire even when the coordinator is healthy, causing unnecessary
       * elections (election storms).
       *
       * THE MECHANISM:
       * Every time we receive ALIVE from the current leader, we reset the timer.
       * This means:
       * - If coordinator sends ALIVE every 8s, timer resets every 8s
       * - Timer only expires (20s) if coordinator STOPS sending ALIVE
       * - This provides reliable failure detection without false positives
       *
       * PARTITION HEALING ENHANCEMENT (Mechanism 2):
       * If we receive ALIVE from a higher-priority node, we may adopt them
       * as coordinator without requiring a COORDINATOR message. This provides
       * faster partition healing when nodes move into range.
       *
       * Adoption occurs when ALL of the following are true:
       * 1. sender_id > my_node_id (sender has higher priority than us)
       * 2. At least ONE of these conditions:
       *    a) current_leader == 0 (we have no known leader)
       *    b) state == STATE_WAITING_COORDINATOR (we're waiting for announcement)
       *    c) sender_id > current_leader (sender has higher priority than our leader)
       *
       * Rationale for each condition:
       * - Condition 1: Only higher-priority nodes can be coordinators (algorithm correctness)
       * - Condition 2a: If we have no leader, adopt any reachable higher-priority node
       * - Condition 2b: If waiting for coordinator, adopt higher-priority ALIVE sender
       * - Condition 2c: If we hear from a higher-priority coordinator, adopt them immediately
       *
       * This mechanism complements Mechanism 1 (coordinator re-announcement) but
       * handles cases where the coordinator doesn't receive an ELECTION message,
       * such as when a node passively discovers the coordinator via ALIVE.
       *
       * EXAMPLE TIMELINE (healthy system):
       *   t=0:  Coordinator elected, sends ALIVE
       *   t=8:  ALIVE received → coordinator_timer reset to 20s
       *   t=16: ALIVE received → coordinator_timer reset to 20s
       *   t=24: ALIVE received → coordinator_timer reset to 20s
       *   ...continues indefinitely while coordinator is alive...
       *
       * EXAMPLE TIMELINE (coordinator failure):
       *   t=0:  Last ALIVE received
       *   t=8:  Coordinator crashes (no ALIVE sent)
       *   t=20: coordinator_timer expires (20s after last ALIVE)
       *   t=20: New election triggered by timeout
       *   t=25: New coordinator elected (node with next-highest ID)
       */

      /* PARTITION HEALING (Mechanism 2): ALIVE-based coordinator adoption
       * Check if we should adopt this ALIVE sender as our coordinator.
       * This happens when we receive ALIVE from a higher-priority node AND:
       * - We have no current leader (current_leader == 0), OR
       * - We're waiting for coordinator announcement, OR
       * - This node has higher priority than our current leader
       * This provides passive discovery without requiring explicit COORDINATOR messages. */
      if (sender_id > my_node_id &&
          (current_leader == 0 ||
           state == STATE_WAITING_COORDINATOR ||
           sender_id > current_leader)) {
        LOG_INFO("Adopting node %u as coordinator (discovered via ALIVE)\n", sender_id);
        current_leader = sender_id;
        state = STATE_NORMAL;
        /* Reset coordinator timer to start monitoring the new leader */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
      }
      /* STANDARD BEHAVIOR: Reset timer for current leader */
      else if (sender_id == current_leader) {
        LOG_INFO("Leader %u is alive\n", sender_id);

        /* Reset coordinator timeout timer */
        /* This is the core of the failure detection mechanism */
        /* As long as ALIVE messages keep coming, timer keeps resetting */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
      }
      /* Ignore ALIVE from nodes that aren't our current leader and don't qualify for adoption */
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
 * 3. Random startup delay (prevent synchronized elections)
 * 4. Start initial election
 * 5. Enter event loop
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

  /* Initialize UDP connection for IPv6 communication with RPL routing */
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);
  LOG_INFO("UDP connection registered on port %d\n", UDP_PORT);

  /*
   * RANDOM STARTUP DELAY
   *
   * Problem: If all nodes start simultaneously, they all start elections
   * at the same time, causing collision and confusion.
   *
   * Solution: Each node waits a random time (0-5 seconds) before starting.
   * This staggers the elections and reduces conflicts.
   *
   * Example with 6 nodes:
   *   Node 1: 1.2s delay
   *   Node 2: 4.8s delay
   *   Node 3: 0.3s delay
   *   Node 4: 2.1s delay
   *   Node 5: 3.9s delay
   *   Node 6: 2.7s delay
   *
   * Node 3 starts first, higher-priority nodes respond, eventually Node 6 wins.
   */
  etimer_set(&random_delay_timer, random_rand() % RANDOM_DELAY_MAX);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&random_delay_timer));

  /* Start initial election after random delay */
  start_election();
  etimer_set(&election_timer, ELECTION_TIMEOUT);

  /* Start coordinator monitoring timer */
  /* This will fire periodically to check if coordinator is alive */
  etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

  /* Start alive timer (will only send when we're coordinator) */
  etimer_set(&alive_timer, ALIVE_INTERVAL);

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
         *   → If no announcement, coordinator_timer will trigger new election
         *
         * IMPORTANT: Handle timer expiry regardless of current state.
         * If we received ANSWER before timer expired, we're already in
         * STATE_WAITING_COORDINATOR. We must still handle the timer event
         * to keep the event loop functioning properly.
         */
        if (state == STATE_ELECTION || state == STATE_WAITING_COORDINATOR) {
          if (!election_response_received) {
            /* CASE 1: WE WON THE ELECTION */
            LOG_INFO("No responses received, becoming coordinator\n");

            /* Update our role */
            current_leader = my_node_id;
            state = STATE_NORMAL;

            /* Announce ourselves as coordinator */
            broadcast_message(MSG_COORDINATOR, election_sequence);

            /* Start sending periodic ALIVE heartbeats */
            etimer_reset(&alive_timer);

          } else {
            /* CASE 2: WE LOST (received ANSWER) */
            /* We're already in STATE_WAITING_COORDINATOR */
            /* coordinator_timer is already set, nothing more to do */
            LOG_INFO("Election timer expired, waiting for coordinator announcement\n");
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
         * This timer fires in three scenarios:
         *
         * SCENARIO 1: We're waiting for coordinator announcement
         *   state == STATE_WAITING_COORDINATOR
         *   We backed down from election but winner hasn't announced
         *   → Start new election
         *
         * SCENARIO 2: We have no known leader
         *   current_leader == 0
         *   System just started or previous leader failed
         *   → Start new election
         *
         * SCENARIO 3: Current leader has died
         *   current_leader != my_node_id && current_leader != 0
         *   We haven't received ALIVE within COORDINATOR_TIMEOUT
         *   → Leader is dead
         *   → Clear current_leader
         *   → Start new election
         *
         * NORMAL OPERATION:
         * In a healthy system, SCENARIO 3 should be the ONLY way elections
         * are triggered after initial startup. Each ALIVE resets this timer,
         * so it only expires when the coordinator actually fails.
         */

        /* SCENARIO 1 & 2: Waiting for coordinator or no leader known */
        if (state == STATE_WAITING_COORDINATOR || current_leader == 0) {
          LOG_INFO("No coordinator announcement received, starting new election\n");
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        }
        /* SCENARIO 3: Current leader has failed (stopped sending ALIVE) */
        else if (current_leader != my_node_id) {
          LOG_INFO("Coordinator %u timeout - no ALIVE received, starting election\n",
                   current_leader);

          /* Clear dead leader */
          current_leader = 0;

          /* Start new election */
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        }
        /* If we ARE the coordinator (current_leader == my_node_id), do nothing */

        /* Reset timer for next check period */
        /* Note: This will be reset sooner if we receive ALIVE message */
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
         *
         * Followers receive this ALIVE and reset their coordinator_timer,
         * which is the mechanism that enables proper failure detection.
         *
         * If we're NOT the coordinator, we simply do nothing (we don't send ALIVE).
         */
        if (current_leader == my_node_id) {
          /* We are the coordinator - send heartbeat */
          broadcast_message(MSG_ALIVE, election_sequence);
        }

        /* Reset timer for next heartbeat */
        /* This creates periodic ALIVE messages every ALIVE_INTERVAL */
        etimer_reset(&alive_timer);
      }
    }
    /* Messages are handled via udp_rx_callback → handle_message() */
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

/*===========================================================================*/
/*                         IMPLEMENTATION NOTES                              */
/*===========================================================================*/

/*
 * DEPLOYMENT CHECKLIST
 * ================================
 *
 * 1. TIMING CONFIGURATION
 *    - Ensure COORDINATOR_TIMEOUT > 2× ALIVE_INTERVAL
 *    - Adjust timeouts based on network characteristics
 *    - Consider network congestion and packet loss rates
 *
 * 2. SCALABILITY
 *    - MAX_NODES is currently 10, increase if needed
 *    - Consider memory constraints on sensor nodes
 *    - Test with maximum expected network size
 *
 * 3. NETWORK RELIABILITY
 *    - Test under packet loss conditions
 *    - Test with network partitions (split-brain scenarios)
 *    - Verify recovery when partition heals
 *
 * 4. POWER CONSUMPTION
 *    - ALIVE messages consume power (radio transmission)
 *    - Increase ALIVE_INTERVAL if battery life is critical
 *    - Balance against failure detection speed
 *
 * 5. MONITORING
 *    - Track election frequency (should be rare after startup)
 *    - Log coordinator changes for analysis
 *
 * 6. SECURITY (Not currently implemented)
 *    - Add message authentication (prevent spoofing)
 *    - Validate node IDs (prevent priority manipulation)
 *    - Consider encryption for sensitive applications
 *
 * 7. TESTING SCENARIOS
 *    - Normal operation (stable coordinator)
 *    - Coordinator failure (planned shutdown)
 *    - Coordinator crash (abrupt failure)
 *    - Multiple simultaneous failures
 *    - Network partition and recovery
 *    - Worst-case: all nodes start simultaneously
 *
 * KNOWN LIMITATIONS
 * =================
 *
 * 1. BYZANTINE FAULTS
 *    - Assumes nodes are not malicious
 *    - Malicious node can claim false priority
 *    - Solution: Add authentication and Byzantine fault tolerance
 *
 * 2. NETWORK PARTITIONS
 *    - Can result in multiple coordinators (split-brain)
 *    - Each partition elects its own coordinator
 *    - When partition heals, algorithm will reconcile (may cause brief election)
 *
 * 3. CLOCK SKEW
 *    - Timers on different nodes may drift
 *    - Extreme drift can cause timing issues
 *    - Consider periodic clock synchronization
 *
 * PERFORMANCE CHARACTERISTICS
 * ===========================
 *
 * Time Complexity:
 *   - Message overhead: O(n²) worst case (every node broadcasts)
 *   - Election completion: O(n × ELECTION_TIMEOUT) worst case
 *
 * Message Complexity:
 *   - Startup: O(n²) messages (cascading elections)
 *   - Steady state: 1 ALIVE per ALIVE_INTERVAL
 *   - Election: O(n²) messages in worst case
 *
 * Memory Usage:
 *   - Per node: ~100 bytes static + message buffers
 *   - Sequence tracking: 2 bytes × MAX_NODES
 *   - Scales well for small to medium networks (< 100 nodes)
 *
 * Network Bandwidth:
 *   - Steady state: sizeof(bully_msg_t) bytes every ALIVE_INTERVAL
 *   - Election: O(n) × sizeof(bully_msg_t) bytes
 *   - Low overhead for typical WSN applications
 */
