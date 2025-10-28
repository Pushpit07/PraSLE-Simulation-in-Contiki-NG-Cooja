/*
 * Copyright (c) 2024, TU Dresden
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * \file
 *         PraSLE (Practical Self-Stabilizing Leader Election) Algorithm
 *         Based on "A Practical Self-Stabilizing Leader Election for
 *         Networks of Resource-Constrained IoT Devices" (Conard & Ebnenasir, 2021)
 * \author
 *         TU Dresden Thesis Project
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
#include <stdbool.h>

/* Log configuration */
#define LOG_MODULE "PraSLE"
#define LOG_LEVEL LOG_LEVEL_INFO

/* PraSLE Algorithm Parameters (based on Algorithm 1 from paper) */
#define MAX_NEIGHBORS 8           /* Maximum number of neighbors */
#define N_MAX 20                  /* Maximum number of nodes in network */
#define K_ROUNDS 10               /* K can be network diameter (tunable) */
#define T_SECONDS 1.0             /* T: max network latency in seconds (tunable) */
#define CLOCK_SECOND_FLOAT ((float)CLOCK_SECOND)

/* Convert T_SECONDS to clock ticks */
#define T_VALUE ((clock_time_t)(T_SECONDS * CLOCK_SECOND_FLOAT))

/* Network topology configuration */
#define TOPOLOGY_RING  1
#define TOPOLOGY_LINE  2
#define TOPOLOGY_MESH  3
#define TOPOLOGY_CLIQUE 4

/* Select topology - change this to test different topologies */
#define NETWORK_TOPOLOGY TOPOLOGY_RING
#define NETWORK_SIZE 6            /* Number of nodes in network */

/* Message structure: (min, leader) pair */
typedef struct {
  uint16_t min_value;    /* Ranking value (mini) */
  uint16_t leader_id;    /* Leader ID (leaderi) */
  uint16_t sender_id;    /* ID of sending node */
} prasle_msg_t;

/* Neighbor information */
typedef struct {
  uint16_t node_id;
  uint16_t min_value;
  uint16_t leader_id;
  bool valid;
} neighbor_info_t;

/* Global variables following Algorithm 1 */
static uint16_t my_node_id;                        /* Process pi identifier */
static int round_counter;                          /* Current round number */
static neighbor_info_t neighbors[MAX_NEIGHBORS];   /* Neighbor list */
static uint8_t num_neighbors = 0;                  /* Number of neighbors */
static uint16_t mini;                              /* Current min value */
static uint16_t temp_mini;                         /* Temporary min value for current round */
static uint16_t leaderi;                           /* Current leader ID */
static uint16_t temp_leaderi;                      /* Temporary leader ID for current round */
static bool election_converged = false;            /* Convergence flag */
static clock_time_t convergence_time = 0;          /* Time when converged */
static clock_time_t start_time = 0;                /* Start time of election */
static uint32_t messages_sent = 0;                 /* Message counter */
static uint32_t messages_received = 0;             /* Received message counter */

/*---------------------------------------------------------------------------*/
PROCESS(prasle_process, "PraSLE Leader Election");
AUTOSTART_PROCESSES(&prasle_process);
/*---------------------------------------------------------------------------*/

/* Function prototypes */
static uint16_t get_ranking_value(void);
static void init_neighbors(void);
static void send_message_to_neighbors(void);
static void handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src);
static bool is_better(uint16_t m1, uint16_t l1, uint16_t m2, uint16_t l2);
static void check_convergence(void);

/*---------------------------------------------------------------------------*/
/* Get ranking value for this node (can be random or based on node properties) */
static uint16_t
get_ranking_value(void)
{
  /* For simulation, we use node ID as ranking value */
  /* In practice, this could be battery level, compute power, etc. */
  return my_node_id;
}

/*---------------------------------------------------------------------------*/
/* Initialize neighbor list based on network topology */
static void
init_neighbors(void)
{
  num_neighbors = 0;
  memset(neighbors, 0, sizeof(neighbors));

#if NETWORK_TOPOLOGY == TOPOLOGY_RING
  /* Ring: node i connects to node (i+1) mod N and node (i-1) mod N */
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
  /* Line: node i connects to node i-1 and i+1 (if they exist) */
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
  /* Mesh (2D grid): assumes nodes arranged in sqrt(N) x sqrt(N) grid */
  /* For 9 nodes: 3x3 grid, for 16 nodes: 4x4 grid */
  int grid_size = 3; /* Assuming 3x3 = 9 nodes for mesh */
  int row = (my_node_id - 1) / grid_size;
  int col = (my_node_id - 1) % grid_size;

  /* Up neighbor */
  if (row > 0) {
    neighbors[num_neighbors].node_id = (row - 1) * grid_size + col + 1;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    num_neighbors++;
  }
  /* Down neighbor */
  if (row < grid_size - 1) {
    neighbors[num_neighbors].node_id = (row + 1) * grid_size + col + 1;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    num_neighbors++;
  }
  /* Left neighbor */
  if (col > 0) {
    neighbors[num_neighbors].node_id = row * grid_size + col;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    num_neighbors++;
  }
  /* Right neighbor */
  if (col < grid_size - 1) {
    neighbors[num_neighbors].node_id = row * grid_size + col + 2;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    num_neighbors++;
  }

#elif NETWORK_TOPOLOGY == TOPOLOGY_CLIQUE
  /* Clique: every node connects to every other node */
  for (uint16_t i = 1; i <= NETWORK_SIZE; i++) {
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
/* Lexicographic comparison: (m1, l1) < (m2, l2) iff (m1<m2) OR (m1==m2 AND l1<l2) */
static bool
is_better(uint16_t m1, uint16_t l1, uint16_t m2, uint16_t l2)
{
  return (m1 < m2) || ((m1 == m2) && (l1 < l2));
}

/*---------------------------------------------------------------------------*/
/* Send (mini, leaderi) to all neighbors - Algorithm 1 Lines 23-25 */
static void
send_message_to_neighbors(void)
{
  static prasle_msg_t msg;
  msg.min_value = mini;
  msg.leader_id = leaderi;
  msg.sender_id = my_node_id;

  LOG_INFO("Round %d: Broadcasting (min=%u, leader=%u)\n",
           round_counter, mini, leaderi);

  nullnet_buf = (uint8_t *)&msg;
  nullnet_len = sizeof(msg);
  NETSTACK_NETWORK.output(NULL); /* Broadcast to all neighbors */

  messages_sent++;
}

/*---------------------------------------------------------------------------*/
/* Handle incoming message - Algorithm 1 Lines 12-17 */
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

  messages_received++;

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

  /* Algorithm 1 Lines 13-15: Compare and update temp values */
  if (is_better(minj, leaderj, temp_mini, temp_leaderi)) {
    temp_mini = minj;
    temp_leaderi = leaderj;
    LOG_INFO("Round %d: Updated temp values to (min=%u, leader=%u)\n",
             round_counter, temp_mini, temp_leaderi);
  }
}

/*---------------------------------------------------------------------------*/
/* Check if election has converged */
static void
check_convergence(void)
{
  if (!election_converged && round_counter > K_ROUNDS) {
    /* Check if our values haven't changed */
    if (mini == temp_mini && leaderi == temp_leaderi) {
      election_converged = true;
      convergence_time = clock_time() - start_time;

      LOG_INFO("CONVERGED at round %d: Leader = %u (min=%u)\n",
               round_counter, leaderi, mini);
      LOG_INFO("Convergence time: %lu ms\n",
               (unsigned long)(convergence_time * 1000 / CLOCK_SECOND));
      LOG_INFO("Messages sent: %lu, received: %lu\n",
               (unsigned long)messages_sent, (unsigned long)messages_received);
    }
  }
}

/*---------------------------------------------------------------------------*/
/* Input callback for nullnet */
static void
input_callback(const void *data, uint16_t len,
               const linkaddr_t *src, const linkaddr_t *dest)
{
  handle_message((const uint8_t *)data, len, src);
}

/*---------------------------------------------------------------------------*/
/* Main PraSLE process - implements Algorithm 1 */
PROCESS_THREAD(prasle_process, ev, data)
{
  static struct etimer round_timer;
  static struct etimer recv_timer;

  PROCESS_BEGIN();

  /* Initialize node ID */
  my_node_id = simMoteID;
  if (my_node_id == 0) {
    my_node_id = 1;
  }

  LOG_INFO("PraSLE node %u starting\n", my_node_id);
  LOG_INFO("Parameters: K=%d rounds, T=%u.%u seconds\n",
           K_ROUNDS, (unsigned int)T_SECONDS,
           (unsigned int)((T_SECONDS - (int)T_SECONDS) * 10));

  /* Algorithm 1 Line 2: Initialize round counter */
  round_counter = K_ROUNDS + 1;

  /* Algorithm 1 Line 3: Initialize neighbor list */
  init_neighbors();

  /* Algorithm 1 Line 4: Initialize mini = N + 1 */
  mini = N_MAX + 1;

  /* Algorithm 1 Line 5: Get ranking value for temp_mini */
  temp_mini = get_ranking_value();

  /* Algorithm 1 Line 6-7: Initialize leader IDs */
  leaderi = my_node_id;
  temp_leaderi = my_node_id;

  LOG_INFO("Initial values: mini=%u, temp_mini=%u, leaderi=%u\n",
           mini, temp_mini, leaderi);

  /* Initialize nullnet */
  nullnet_buf = NULL;
  nullnet_len = 0;
  nullnet_set_input_callback(input_callback);

  /* Small random delay to avoid synchronized starts */
  etimer_set(&round_timer, (random_rand() % CLOCK_SECOND) + CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&round_timer));

  start_time = clock_time();

  /* Algorithm 1 Line 9: Main loop (until False = infinite loop) */
  while (1) {
    /* Start a new round */
    round_counter--;
    LOG_INFO("========== Starting Round %d ==========\n", round_counter);

    /* Algorithm 1 Line 11: Wait and receive for T seconds */
    etimer_set(&recv_timer, T_VALUE);

    LOG_INFO("Round %d: Receiving phase (%u ms)\n",
             round_counter, (unsigned int)(T_VALUE * 1000 / CLOCK_SECOND));

    /* Wait for T seconds, processing incoming messages */
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&recv_timer));

    LOG_INFO("Round %d: Receive phase complete\n", round_counter);

    /* Algorithm 1 Line 19: Decrement round counter (already done at start) */

    /* Algorithm 1 Line 20-26: Update local knowledge and disseminate */
    if (is_better(temp_mini, temp_leaderi, mini, leaderi)) {
      /* Algorithm 1 Lines 21-22: Update mini and leaderi */
      mini = temp_mini;
      leaderi = temp_leaderi;

      LOG_INFO("Round %d: Updated to (min=%u, leader=%u)\n",
               round_counter, mini, leaderi);

      /* Algorithm 1 Lines 23-25: Send to all neighbors */
      send_message_to_neighbors();
    } else {
      LOG_INFO("Round %d: No update needed (min=%u, leader=%u)\n",
               round_counter, mini, leaderi);
    }

    /* Algorithm 1 Line 27: Check termination condition */
    if (round_counter <= 0) {
      LOG_INFO("========== Election Complete ==========\n");
      LOG_INFO("Final Leader: %u (min=%u)\n", leaderi, mini);
      LOG_INFO("Total messages sent: %lu, received: %lu\n",
               (unsigned long)messages_sent, (unsigned long)messages_received);

      if (!election_converged) {
        convergence_time = clock_time() - start_time;
        LOG_INFO("Total time: %lu ms\n",
                 (unsigned long)(convergence_time * 1000 / CLOCK_SECOND));
        election_converged = true;
      }

      /* Continue running to maintain leader info (optional) */
      etimer_set(&round_timer, 10 * CLOCK_SECOND);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&round_timer));
    } else {
      /* Check for early convergence */
      check_convergence();

      /* Small delay between rounds */
      etimer_set(&round_timer, CLOCK_SECOND / 4);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&round_timer));
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
