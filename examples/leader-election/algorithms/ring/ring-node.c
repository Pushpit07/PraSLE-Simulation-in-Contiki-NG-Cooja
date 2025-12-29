/**
 * \file   ring-node.c
 * \brief  Ring Leader Election Algorithm for Contiki-NG
 * \author Pushpit Bhardwaj
 *
 * Ring-based leader election algorithm where messages circulate around
 * a logical ring. Each node forwards the message to its successor,
 * updating the candidate if it has a higher ID.
 *
 * ALGORITHM:
 * 1. Any node can start an election by sending ELECTION to successor
 * 2. Each node compares its ID with candidate and forwards higher one
 * 3. When ELECTION returns to initiator, the candidate is the winner
 * 4. Winner broadcasts COORDINATOR around the ring
 * 5. Coordinator periodically sends ALIVE around the ring
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
static ring_state_t state = STATE_NORMAL;
static uint16_t my_node_id;
static uint16_t current_leader = 0;
static uint16_t election_sequence = 0;
static uint16_t next_node_id = 0;
static bool election_in_progress = false;

/*---------------------------------------------------------------------------*/
/* TIMER MANAGEMENT */
/*---------------------------------------------------------------------------*/
static struct etimer election_timer;
static struct etimer coordinator_timer;
static struct etimer alive_timer;

#if ENABLE_METRICS
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
static void send_to_next_node(uint8_t msg_type, uint16_t initiator, uint16_t candidate, uint16_t sequence);
static void start_election(void);
static void handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src);
static uint16_t get_next_node(uint16_t node_id);

/*---------------------------------------------------------------------------*/
/* Helper to convert ring state to common election state */
/*---------------------------------------------------------------------------*/
static election_state_t
ring_to_common_state(ring_state_t rstate)
{
  switch(rstate) {
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

/*---------------------------------------------------------------------------*/
static uint16_t
get_next_node(uint16_t node_id)
{
  /* Ring topology: 1 -> 2 -> 3 -> ... -> RING_SIZE -> 1 */
  if (node_id >= RING_SIZE) {
    return 1;
  } else {
    return node_id + 1;
  }
}

/*---------------------------------------------------------------------------*/
static void
send_to_next_node(uint8_t msg_type, uint16_t initiator, uint16_t candidate, uint16_t sequence)
{
  static ring_msg_t msg;
  msg.type = msg_type;
  msg.initiator_id = initiator;
  msg.candidate_id = candidate;
  msg.sequence = sequence;
  msg.target_node_id = next_node_id;

  LOG_INFO("Sending %s (initiator=%u, candidate=%u, seq=%u) to node %u\n",
           msg_type == MSG_ELECTION ? "ELECTION" :
           msg_type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           initiator, candidate, sequence, next_node_id);

#if ENABLE_METRICS
  metrics_track_message_sent(msg_type, sizeof(ring_msg_t));
  metrics.algo.ring.forwards++;
  if(msg_type == MSG_ALIVE) {
    metrics_track_heartbeat_sent();
  }
#endif

  nullnet_buf = (uint8_t *)&msg;
  nullnet_len = sizeof(msg);
  NETSTACK_NETWORK.output(NULL); /* Broadcast, nodes filter by target_node_id */
}

/*---------------------------------------------------------------------------*/
static void
start_election(void)
{
  if (election_in_progress) {
    LOG_INFO("Election already in progress\n");
    return;
  }

  LOG_INFO("Starting ring election (sequence %u)\n", election_sequence + 1);

#if ENABLE_METRICS
  metrics_track_state(ELECTION_STATE_ELECTION);
  metrics_track_election_start();
#endif

  state = STATE_ELECTION;
  election_sequence++;
  election_in_progress = true;

  /* Send ELECTION message with our own ID as both initiator and candidate */
  send_to_next_node(MSG_ELECTION, my_node_id, my_node_id, election_sequence);
}

/*---------------------------------------------------------------------------*/
static void
handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src)
{
  if (len != sizeof(ring_msg_t)) {
    LOG_WARN("Received message with wrong size\n");
    return;
  }

  ring_msg_t *msg = (ring_msg_t *)data;

  /* Filter messages: only process if targeted to us */
  if (msg->target_node_id != my_node_id) {
    return;
  }

  LOG_INFO("Received %s (initiator=%u, candidate=%u, seq=%u)\n",
           msg->type == MSG_ELECTION ? "ELECTION" :
           msg->type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           msg->initiator_id, msg->candidate_id, msg->sequence);

#if ENABLE_METRICS
  metrics_track_message_recv(msg->type, sizeof(ring_msg_t));
#endif

  switch (msg->type) {
    case MSG_ELECTION:
      if (msg->initiator_id == my_node_id) {
        /* Our election message returned - we determine the leader */
        LOG_INFO("Election completed - Leader is node %u\n", msg->candidate_id);

#if ENABLE_METRICS
        metrics.algo.ring.ring_completions++;
        metrics_track_election_end(msg->candidate_id == my_node_id);
#endif

        current_leader = msg->candidate_id;
        state = STATE_NORMAL;
        election_in_progress = false;

#if ENABLE_METRICS
        metrics_track_leader_change(current_leader);
        metrics_track_state(ring_to_common_state(STATE_NORMAL));
#endif

        /* Announce leadership by sending COORDINATOR message */
        send_to_next_node(MSG_COORDINATOR, my_node_id, current_leader, msg->sequence);

        /* If we became the leader, start alive timer */
        if (current_leader == my_node_id) {
          etimer_set(&alive_timer, ALIVE_INTERVAL);
        }

      } else {
        /* Forward election message, updating candidate if we have higher ID */
        uint16_t new_candidate = msg->candidate_id;
        if (my_node_id > msg->candidate_id) {
          new_candidate = my_node_id;
          LOG_INFO("Updating candidate from %u to %u\n", msg->candidate_id, my_node_id);
        }

#if ENABLE_METRICS
        metrics_track_state(ELECTION_STATE_ELECTION);
#endif

        state = STATE_ELECTION;
        election_in_progress = true;

        /* Forward with original initiator but potentially updated candidate */
        send_to_next_node(MSG_ELECTION, msg->initiator_id, new_candidate, msg->sequence);
      }
      break;

    case MSG_COORDINATOR:
      if (msg->initiator_id == my_node_id && current_leader == msg->candidate_id) {
        /* Our coordinator message returned - announcement complete */
        LOG_INFO("Coordinator announcement completed the ring\n");

#if ENABLE_METRICS
        metrics.algo.ring.ring_completions++;
#endif
        /* DO NOT forward - this terminates the coordinator message */
      } else {
        /* Accept new coordinator and forward message */
        LOG_INFO("New coordinator announced: node %u\n", msg->candidate_id);

#if ENABLE_METRICS
        metrics_track_leader_change(msg->candidate_id);
        metrics_track_state(ring_to_common_state(STATE_NORMAL));
#endif

        current_leader = msg->candidate_id;
        state = STATE_NORMAL;
        election_in_progress = false;

        /* Reset coordinator timer - we now have a valid leader */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

        /* Forward the coordinator message */
        send_to_next_node(MSG_COORDINATOR, msg->initiator_id, msg->candidate_id, msg->sequence);
      }
      break;

    case MSG_ALIVE:
#if ENABLE_METRICS
      metrics_track_heartbeat_recv();
#endif

      if (msg->initiator_id == my_node_id && current_leader == my_node_id) {
        /* Our alive message returned - heartbeat complete */
        LOG_INFO("Alive message completed the ring\n");

#if ENABLE_METRICS
        metrics.algo.ring.ring_completions++;
#endif
        /* DO NOT forward - this terminates the alive message */
      } else if (msg->initiator_id == current_leader) {
        /* Forward alive message from current leader */
        LOG_INFO("Leader %u is alive - forwarding\n", msg->initiator_id);

        /* Reset coordinator timer */
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

        send_to_next_node(MSG_ALIVE, msg->initiator_id, msg->candidate_id, msg->sequence);
      } else if (current_leader == 0) {
        /* We have no leader, adopt the ALIVE sender if valid */
        LOG_INFO("Adopting node %u as coordinator (via ALIVE)\n", msg->initiator_id);

#if ENABLE_METRICS
        metrics_track_leader_change(msg->initiator_id);
        metrics_track_state(ring_to_common_state(STATE_NORMAL));
#endif

        current_leader = msg->initiator_id;
        state = STATE_NORMAL;
        election_in_progress = false;
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

        /* Forward the alive message */
        send_to_next_node(MSG_ALIVE, msg->initiator_id, msg->candidate_id, msg->sequence);
      } else {
        LOG_WARN("Received ALIVE from non-leader node %u (current leader: %u)\n",
                 msg->initiator_id, current_leader);
      }
      break;

    default:
      LOG_WARN("Unknown message type: %u\n", msg->type);
      break;
  }
}

/*---------------------------------------------------------------------------*/
static void
input_callback(const void *data, uint16_t len,
               const linkaddr_t *src, const linkaddr_t *dest)
{
  handle_message((const uint8_t *)data, len, src);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ring_process, ev, data)
{
  static struct etimer random_delay_timer;

  PROCESS_BEGIN();

  /* Initialize node ID from Cooja mote ID */
  my_node_id = simMoteID;
  if (my_node_id == 0) {
    my_node_id = 1;
  }

  /* Determine next node in ring */
  next_node_id = get_next_node(my_node_id);

  LOG_INFO("Ring node %u starting (next node: %u, ring size: %u)\n",
           my_node_id, next_node_id, RING_SIZE);

  /* Initialize nullnet */
  nullnet_buf = NULL;
  nullnet_len = 0;
  nullnet_set_input_callback(input_callback);

#if ENABLE_METRICS
  /* Initialize common metrics */
  metrics_init();
  metrics_output_header();
#endif

  /* Random delay before starting to avoid synchronized starts */
  etimer_set(&random_delay_timer, random_rand() % RANDOM_DELAY_MAX);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&random_delay_timer));

  /* Node with highest ID starts the first election */
  if (my_node_id == RING_SIZE) {
    LOG_INFO("I am the highest ID node, starting initial election\n");
    start_election();
    etimer_set(&election_timer, ELECTION_TIMEOUT);
  }

  /* Set up timers */
  etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
  etimer_set(&alive_timer, ALIVE_INTERVAL);

#if ENABLE_METRICS
  etimer_set(&metrics_timer, METRICS_OUTPUT_INTERVAL);
#endif

  /* Main event loop */
  while (1) {
    PROCESS_WAIT_EVENT();

    if (ev == PROCESS_EVENT_TIMER) {
      if (data == &election_timer) {
        if (state == STATE_ELECTION && election_in_progress) {
          LOG_INFO("Election timeout - restarting election\n");

#if ENABLE_METRICS
          metrics_track_timeout();
#endif

          election_in_progress = false;
          start_election();
          etimer_reset(&election_timer);
        }
      } else if (data == &coordinator_timer) {
        if (current_leader == 0 && !election_in_progress) {
          LOG_INFO("Coordinator timeout - starting new election\n");

#if ENABLE_METRICS
          metrics_track_timeout();
#endif

          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        } else if (current_leader != 0 && current_leader != my_node_id && !election_in_progress) {
          /* Leader failed - start new election */
          LOG_INFO("Coordinator %u timeout - starting election\n", current_leader);

#if ENABLE_METRICS
          metrics_track_timeout();
#endif

          current_leader = 0;
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        }
        etimer_reset(&coordinator_timer);
      } else if (data == &alive_timer) {
        if (current_leader == my_node_id) {
          /* We are the coordinator - send alive around the ring */
          send_to_next_node(MSG_ALIVE, my_node_id, my_node_id, election_sequence);
        }
        etimer_reset(&alive_timer);
      }

#if ENABLE_METRICS
      else if (data == &metrics_timer) {
        metrics_output();
        etimer_reset(&metrics_timer);
      }
#endif
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
