/**
 * \file   bully-node.c
 * \brief  Bully Leader Election Algorithm for Contiki-NG
 * \author Pushpit Bhardwaj
 *
 * This is the refactored version that uses the common metrics infrastructure
 * from the leader-election framework.
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
#define UDP_PORT ELECTION_UDP_PORT

/*---------------------------------------------------------------------------*/
/* GLOBAL STATE VARIABLES */
/*---------------------------------------------------------------------------*/
static bully_state_t state = STATE_NORMAL;
static uint16_t my_node_id;
static uint16_t current_leader = 0;
static uint16_t election_sequence = 0;
static bool election_response_received = false;
static struct simple_udp_connection udp_conn;

/*---------------------------------------------------------------------------*/
/* TIMER MANAGEMENT (Global for message handler access) */
/*---------------------------------------------------------------------------*/
static struct etimer election_timer;
static struct etimer coordinator_timer;
static struct etimer alive_timer;

#if ENABLE_METRICS
static struct etimer metrics_timer;
#endif

/*---------------------------------------------------------------------------*/
/* DUPLICATE MESSAGE DETECTION */
/*---------------------------------------------------------------------------*/
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
/* Helper to convert bully state to common election state */
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

/*---------------------------------------------------------------------------*/
static void
send_message(uint8_t msg_type, uint16_t target_id, uint16_t sequence)
{
  static bully_msg_t msg;

  msg.type = msg_type;
  msg.node_id = my_node_id;
  msg.target_id = target_id;
  msg.sequence = sequence;

  LOG_INFO("Sending %s to node %u\n",
           msg_type == MSG_ELECTION ? "ELECTION" :
           msg_type == MSG_ANSWER ? "ANSWER" :
           msg_type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           target_id);

#if ENABLE_METRICS
  metrics_track_message_sent(msg_type, sizeof(bully_msg_t));
#endif

  uip_ipaddr_t dest_addr;
  uip_create_linklocal_allnodes_mcast(&dest_addr);
  simple_udp_sendto(&udp_conn, &msg, sizeof(msg), &dest_addr);
}

/*---------------------------------------------------------------------------*/
static bool
is_duplicate_message(uint16_t sender_id, uint16_t sequence)
{
  if (sender_id == 0 || sender_id > MAX_NODES) {
    return false;
  }

  uint16_t idx = sender_id - 1;

  if (last_seen_sequence[idx] >= sequence) {
    return true;
  }

  last_seen_sequence[idx] = sequence;
  return false;
}

/*---------------------------------------------------------------------------*/
static void
broadcast_message(uint8_t msg_type, uint16_t sequence)
{
  static bully_msg_t msg;

  msg.type = msg_type;
  msg.node_id = my_node_id;
  msg.target_id = 0;
  msg.sequence = sequence;

  LOG_INFO("Broadcasting %s\n",
           msg_type == MSG_ELECTION ? "ELECTION" :
           msg_type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE");

#if ENABLE_METRICS
  metrics_track_message_sent(msg_type, sizeof(bully_msg_t));
  if(msg_type == MSG_ALIVE) {
    metrics_track_heartbeat_sent();
  }
#endif

  uip_ipaddr_t dest_addr;
  uip_create_linklocal_allnodes_mcast(&dest_addr);
  simple_udp_sendto(&udp_conn, &msg, sizeof(msg), &dest_addr);
}

/*---------------------------------------------------------------------------*/
static void
start_election(void)
{
  if (state == STATE_ELECTION) {
    LOG_INFO("Election already in progress\n");
    return;
  }

  LOG_INFO("Starting election (sequence %u)\n", election_sequence + 1);

#if ENABLE_METRICS
  metrics_track_state(ELECTION_STATE_ELECTION);
  metrics_track_election_start();
#endif

  state = STATE_ELECTION;
  election_sequence++;
  election_response_received = false;

  broadcast_message(MSG_ELECTION, election_sequence);
}

/*---------------------------------------------------------------------------*/
static void
handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src)
{
  if (len != sizeof(bully_msg_t)) {
    LOG_WARN("Received message with wrong size\n");
    return;
  }

  bully_msg_t *msg = (bully_msg_t *)data;
  uint16_t sender_id = msg->node_id;

  /* Filter self-messages */
  if (sender_id == my_node_id) {
    return;
  }

  LOG_INFO("Received %s from node %u (seq %u)\n",
           msg->type == MSG_ELECTION ? "ELECTION" :
           msg->type == MSG_ANSWER ? "ANSWER" :
           msg->type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           sender_id, msg->sequence);

  /* Check duplicates (only for ELECTION) */
  if (msg->type != MSG_ALIVE && msg->type != MSG_ANSWER && msg->type != MSG_COORDINATOR &&
      is_duplicate_message(sender_id, msg->sequence)) {
    LOG_INFO("Ignoring duplicate message from node %u\n", sender_id);
#if ENABLE_METRICS
    metrics.algo.bully.duplicates_filtered++;
#endif
    return;
  }

#if ENABLE_METRICS
  metrics_track_message_recv(msg->type, sizeof(bully_msg_t));
#endif

  switch (msg->type) {

    case MSG_ELECTION:
      if (msg->target_id == 0 || msg->target_id == my_node_id) {
        if (my_node_id > sender_id) {
          send_message(MSG_ANSWER, sender_id, msg->sequence);
          LOG_INFO("Sent ANSWER to node %u (I have higher priority)\n", sender_id);

          /* Partition healing: re-announce if we're coordinator */
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

    case MSG_ANSWER:
      if (msg->target_id == my_node_id && state == STATE_ELECTION) {
        election_response_received = true;
        LOG_INFO("Received ANSWER from node %u, backing down\n", sender_id);

#if ENABLE_METRICS
        metrics_track_election_end(false);
        metrics_track_state(ELECTION_STATE_WAITING);
#endif

        state = STATE_WAITING_COORDINATOR;
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
      }
      break;

    case MSG_COORDINATOR:
      if (sender_id >= my_node_id) {
        LOG_INFO("New coordinator: node %u\n", sender_id);

#if ENABLE_METRICS
        metrics_track_leader_change(sender_id);
        metrics_track_state(bully_to_common_state(STATE_NORMAL));
#endif

        current_leader = sender_id;
        state = STATE_NORMAL;
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

      } else {
        LOG_WARN("Rejecting coordinator %u (lower priority than me)\n", sender_id);
#if ENABLE_METRICS
        metrics.algo.bully.invalid_coordinators++;
#endif

        if (state != STATE_ELECTION) {
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        }
      }
      break;

    case MSG_ALIVE:
#if ENABLE_METRICS
      metrics_track_heartbeat_recv();
#endif

      /* Partition healing: adopt higher-priority ALIVE sender */
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
      else if (sender_id == current_leader) {
        LOG_INFO("Leader %u is alive\n", sender_id);
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
      }
      break;

    default:
      LOG_WARN("Unknown message type: %u\n", msg->type);
      break;
  }
}

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

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(bully_process, ev, data)
{
  static struct etimer random_delay_timer;

  PROCESS_BEGIN();

  /* Initialize node ID */
  my_node_id = simMoteID;
  if (my_node_id == 0) {
    my_node_id = 1;
  }

  LOG_INFO("Bully node %u starting\n", my_node_id);

  /* Initialize UDP */
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);
  LOG_INFO("UDP connection registered on port %d\n", UDP_PORT);

#if ENABLE_METRICS
  /* Initialize common metrics */
  metrics_init();
  metrics_output_header();
#endif

  /* Random startup delay */
  etimer_set(&random_delay_timer, random_rand() % RANDOM_DELAY_MAX);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&random_delay_timer));

  /* Start initial election */
  start_election();
  etimer_set(&election_timer, ELECTION_TIMEOUT);
  etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
  etimer_set(&alive_timer, ALIVE_INTERVAL);

#if ENABLE_METRICS
  etimer_set(&metrics_timer, METRICS_OUTPUT_INTERVAL);
#endif

  /* Main event loop */
  while (1) {
    PROCESS_WAIT_EVENT();

    if (ev == PROCESS_EVENT_TIMER) {

      /* Election timer expired */
      if (data == &election_timer) {
        if (state == STATE_ELECTION || state == STATE_WAITING_COORDINATOR) {
          if (!election_response_received) {
            LOG_INFO("No responses received, becoming coordinator\n");

#if ENABLE_METRICS
            metrics_track_election_end(true);
            metrics_track_leader_change(my_node_id);
            metrics_track_state(ELECTION_STATE_LEADER);
#endif

            current_leader = my_node_id;
            state = STATE_NORMAL;
            broadcast_message(MSG_COORDINATOR, election_sequence);
            etimer_reset(&alive_timer);

          } else {
            LOG_INFO("Election timer expired, waiting for coordinator\n");
          }
        }
      }

      /* Coordinator timer expired */
      else if (data == &coordinator_timer) {
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
        etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
      }

      /* Alive timer expired */
      else if (data == &alive_timer) {
        if (current_leader == my_node_id) {
          broadcast_message(MSG_ALIVE, election_sequence);
        }
        etimer_reset(&alive_timer);
      }

#if ENABLE_METRICS
      /* Metrics timer expired */
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
