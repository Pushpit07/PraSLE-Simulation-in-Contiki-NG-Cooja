/**
 * \file   prasle-node.c
 * \brief  PraSLE (Practical Self-Stabilizing Leader Election) Algorithm
 * \author Pushpit Bhardwaj
 *
 * Based on "A Practical Self-Stabilizing Leader Election for
 * Networks of Resource-Constrained IoT Devices" (Conard & Ebnenasir, 2021)
 *
 * ALGORITHM:
 * - Round-based algorithm using (min, leader) pairs
 * - Each node starts with (ranking_value, my_id)
 * - Nodes exchange values with neighbors and adopt lexicographically smaller pairs
 * - After K rounds, all nodes converge to the same leader
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
#include "prasle-config.h"

/*---------------------------------------------------------------------------*/
/* LOGGING CONFIGURATION */
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "PraSLE"
#define LOG_LEVEL LOG_LEVEL_INFO

/*---------------------------------------------------------------------------*/
/* GLOBAL STATE VARIABLES */
/*---------------------------------------------------------------------------*/
static uint16_t my_node_id;
static int round_counter;
static neighbor_info_t neighbors[MAX_NEIGHBORS];
static uint8_t num_neighbors = 0;
static uint16_t mini;           /* Current min value */
static uint16_t temp_mini;      /* Temporary min value for current round */
static uint16_t leaderi;        /* Current leader ID */
static uint16_t temp_leaderi;   /* Temporary leader ID for current round */
static bool election_converged = false;
static clock_time_t start_time = 0;

/*---------------------------------------------------------------------------*/
/* TIMER MANAGEMENT */
/*---------------------------------------------------------------------------*/
#if ENABLE_METRICS
static struct etimer metrics_timer;
#endif

/*---------------------------------------------------------------------------*/
/* CONTIKI-NG PROCESS DEFINITION */
/*---------------------------------------------------------------------------*/
PROCESS(prasle_process, "PraSLE Leader Election");
AUTOSTART_PROCESSES(&prasle_process);

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES */
/*---------------------------------------------------------------------------*/
static uint16_t get_ranking_value(void);
static void init_neighbors(void);
static void send_message_to_neighbors(void);
static void handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src);
static bool is_better(uint16_t m1, uint16_t l1, uint16_t m2, uint16_t l2);
static void check_convergence(void);

/*---------------------------------------------------------------------------*/
static uint16_t
get_ranking_value(void)
{
  /* Use node ID as ranking value - can be changed to battery level, etc. */
  return my_node_id;
}

/*---------------------------------------------------------------------------*/
static void
init_neighbors(void)
{
  num_neighbors = 0;
  memset(neighbors, 0, sizeof(neighbors));

#if NETWORK_TOPOLOGY == TOPOLOGY_RING
  /* Ring: connect to (i+1) mod N and (i-1) mod N */
  neighbors[num_neighbors].node_id = (my_node_id % NETWORK_SIZE) + 1;
  neighbors[num_neighbors].min_value = N_MAX + 1;
  neighbors[num_neighbors].leader_id = N_MAX + 1;
  neighbors[num_neighbors].valid = true;
  num_neighbors++;

  neighbors[num_neighbors].node_id = ((my_node_id - 2 + NETWORK_SIZE) % NETWORK_SIZE) + 1;
  neighbors[num_neighbors].min_value = N_MAX + 1;
  neighbors[num_neighbors].leader_id = N_MAX + 1;
  neighbors[num_neighbors].valid = true;
  num_neighbors++;

#elif NETWORK_TOPOLOGY == TOPOLOGY_LINE
  /* Line: connect to i-1 and i+1 if they exist */
  if (my_node_id > 1) {
    neighbors[num_neighbors].node_id = my_node_id - 1;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    num_neighbors++;
  }
  if (my_node_id < NETWORK_SIZE) {
    neighbors[num_neighbors].node_id = my_node_id + 1;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    num_neighbors++;
  }

#elif NETWORK_TOPOLOGY == TOPOLOGY_MESH
  /* 2D Grid mesh */
  int grid_size = 3;
  int row = (my_node_id - 1) / grid_size;
  int col = (my_node_id - 1) % grid_size;

  /* Up */
  if (row > 0) {
    neighbors[num_neighbors].node_id = (row - 1) * grid_size + col + 1;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    num_neighbors++;
  }
  /* Down */
  if (row < grid_size - 1) {
    neighbors[num_neighbors].node_id = (row + 1) * grid_size + col + 1;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    num_neighbors++;
  }
  /* Left */
  if (col > 0) {
    neighbors[num_neighbors].node_id = row * grid_size + col;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    num_neighbors++;
  }
  /* Right */
  if (col < grid_size - 1) {
    neighbors[num_neighbors].node_id = row * grid_size + col + 2;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    num_neighbors++;
  }

#elif NETWORK_TOPOLOGY == TOPOLOGY_CLIQUE
  /* Clique: connect to all other nodes */
  for (uint16_t i = 1; i <= NETWORK_SIZE && num_neighbors < MAX_NEIGHBORS; i++) {
    if (i != my_node_id) {
      neighbors[num_neighbors].node_id = i;
      neighbors[num_neighbors].min_value = N_MAX + 1;
      neighbors[num_neighbors].leader_id = N_MAX + 1;
      neighbors[num_neighbors].valid = true;
      num_neighbors++;
    }
  }
#endif

  LOG_INFO("Initialized %u neighbors: ", num_neighbors);
  for (uint8_t i = 0; i < num_neighbors; i++) {
    LOG_INFO_("%u ", neighbors[i].node_id);
  }
  LOG_INFO_("\n");
}

/*---------------------------------------------------------------------------*/
/* Lexicographic comparison: (m1, l1) < (m2, l2) */
static bool
is_better(uint16_t m1, uint16_t l1, uint16_t m2, uint16_t l2)
{
  return (m1 < m2) || ((m1 == m2) && (l1 < l2));
}

/*---------------------------------------------------------------------------*/
static void
send_message_to_neighbors(void)
{
  static prasle_msg_t msg;
  msg.min_value = mini;
  msg.leader_id = leaderi;
  msg.sender_id = my_node_id;

  LOG_INFO("Round %d: Broadcasting (min=%u, leader=%u)\n",
           round_counter, mini, leaderi);

#if ENABLE_METRICS
  metrics_track_message_sent(1, sizeof(prasle_msg_t));
  metrics.algo.prasle.broadcasts++;
#endif

  nullnet_buf = (uint8_t *)&msg;
  nullnet_len = sizeof(msg);
  NETSTACK_NETWORK.output(NULL);
}

/*---------------------------------------------------------------------------*/
static void
handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src)
{
  if (len != sizeof(prasle_msg_t)) {
    LOG_WARN("Received message with wrong size\n");
    return;
  }

  prasle_msg_t *msg = (prasle_msg_t *)data;
  uint16_t sender_id = msg->sender_id;
  uint16_t minj = msg->min_value;
  uint16_t leaderj = msg->leader_id;

#if ENABLE_METRICS
  metrics_track_message_recv(1, sizeof(prasle_msg_t));
#endif

  LOG_INFO("Round %d: Received from node %u: (min=%u, leader=%u)\n",
           round_counter, sender_id, minj, leaderj);

  /* Update neighbor information */
  for (uint8_t i = 0; i < num_neighbors; i++) {
    if (neighbors[i].node_id == sender_id) {
      neighbors[i].min_value = minj;
      neighbors[i].leader_id = leaderj;
      break;
    }
  }

  /* Compare and update temp values (Algorithm 1 Lines 13-15) */
  if (is_better(minj, leaderj, temp_mini, temp_leaderi)) {
    temp_mini = minj;
    temp_leaderi = leaderj;

#if ENABLE_METRICS
    metrics.algo.prasle.value_updates++;
#endif

    LOG_INFO("Round %d: Updated temp values to (min=%u, leader=%u)\n",
             round_counter, temp_mini, temp_leaderi);
  }
}

/*---------------------------------------------------------------------------*/
static void
check_convergence(void)
{
  if (!election_converged && round_counter <= 0) {
    election_converged = true;

#if ENABLE_METRICS
    metrics_record_convergence();
    metrics_track_leader_change(leaderi);
    if (leaderi == my_node_id) {
      metrics_track_state(ELECTION_STATE_LEADER);
    } else {
      metrics_track_state(ELECTION_STATE_NORMAL);
    }
#endif

    LOG_INFO("CONVERGED: Leader = %u (min=%u)\n", leaderi, mini);
    LOG_INFO("Convergence time: %lu ms\n",
             (unsigned long)CLOCK_TO_MS(clock_time() - start_time));
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
PROCESS_THREAD(prasle_process, ev, data)
{
  static struct etimer round_timer;
  static struct etimer recv_timer;
  static struct etimer maintenance_timer;

  PROCESS_BEGIN();

  /* Initialize node ID */
  my_node_id = simMoteID;
  if (my_node_id == 0) {
    my_node_id = 1;
  }

  LOG_INFO("PraSLE node %u starting\n", my_node_id);
  LOG_INFO("Parameters: K=%d rounds, T=%.1f seconds, topology=%d, size=%d\n",
           K_ROUNDS, T_SECONDS, NETWORK_TOPOLOGY, NETWORK_SIZE);

  /* Algorithm 1 Line 2: Initialize round counter */
  round_counter = K_ROUNDS + 1;

  /* Algorithm 1 Line 3: Initialize neighbor list */
  init_neighbors();

  /* Algorithm 1 Line 4: Initialize mini = N + 1 */
  mini = N_MAX + 1;

  /* Algorithm 1 Line 5: Get ranking value for temp_mini */
  temp_mini = get_ranking_value();

  /* Algorithm 1 Lines 6-7: Initialize leader IDs */
  leaderi = my_node_id;
  temp_leaderi = my_node_id;

  LOG_INFO("Initial values: mini=%u, temp_mini=%u, leaderi=%u\n",
           mini, temp_mini, leaderi);

  /* Initialize nullnet */
  nullnet_buf = NULL;
  nullnet_len = 0;
  nullnet_set_input_callback(input_callback);

#if ENABLE_METRICS
  metrics_init();
  metrics.algo.prasle.current_round = round_counter;
  metrics.algo.prasle.min_value = mini;
  metrics_track_state(ELECTION_STATE_ELECTION);
  metrics_track_election_start();
  metrics_output_header();
#endif

  /* Small random delay */
  etimer_set(&round_timer, (random_rand() % CLOCK_SECOND) + CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&round_timer));

  start_time = clock_time();

  /* Main algorithm loop (Algorithm 1 Line 9) */
  while (1) {
    round_counter--;

#if ENABLE_METRICS
    metrics.algo.prasle.current_round = round_counter;
#endif

    LOG_INFO("========== Starting Round %d ==========\n", round_counter);

    /* Algorithm 1 Line 11: Wait and receive for T seconds */
    etimer_set(&recv_timer, T_VALUE);

    LOG_INFO("Round %d: Receiving phase (%lu ms)\n",
             round_counter, (unsigned long)CLOCK_TO_MS(T_VALUE));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&recv_timer));

    LOG_INFO("Round %d: Receive phase complete\n", round_counter);

    /* Algorithm 1 Lines 20-26: Update and disseminate */
    if (is_better(temp_mini, temp_leaderi, mini, leaderi)) {
      mini = temp_mini;
      leaderi = temp_leaderi;

#if ENABLE_METRICS
      metrics.algo.prasle.min_value = mini;
      metrics.algo.prasle.value_updates++;
#endif

      LOG_INFO("Round %d: Updated to (min=%u, leader=%u)\n",
               round_counter, mini, leaderi);

      send_message_to_neighbors();
    } else {
      LOG_INFO("Round %d: No update needed (min=%u, leader=%u)\n",
               round_counter, mini, leaderi);
    }

    /* Algorithm 1 Line 27: Check termination */
    if (round_counter <= 0) {
      LOG_INFO("========== Election Complete ==========\n");
      LOG_INFO("Final Leader: %u (min=%u)\n", leaderi, mini);

#if ENABLE_METRICS
      metrics.algo.prasle.rounds_completed = K_ROUNDS + 1;
      metrics_track_election_end(leaderi == my_node_id);
#endif

      check_convergence();

      /* Enter maintenance mode - periodic metrics output */
#if ENABLE_METRICS
      etimer_set(&metrics_timer, METRICS_OUTPUT_INTERVAL);
#endif
      etimer_set(&maintenance_timer, 10 * CLOCK_SECOND);

      while (1) {
        PROCESS_WAIT_EVENT();

        if (ev == PROCESS_EVENT_TIMER) {
#if ENABLE_METRICS
          if (data == &metrics_timer) {
            metrics_output();
            etimer_reset(&metrics_timer);
          }
#endif
          if (data == &maintenance_timer) {
            /* Periodic heartbeat in maintenance mode */
            LOG_INFO("Maintenance: Leader = %u\n", leaderi);
            etimer_reset(&maintenance_timer);
          }
        }
      }
    } else {
#if ENABLE_METRICS
      metrics.algo.prasle.rounds_completed++;
#endif

      /* Small delay between rounds */
      etimer_set(&round_timer, CLOCK_SECOND / 4);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&round_timer));
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
