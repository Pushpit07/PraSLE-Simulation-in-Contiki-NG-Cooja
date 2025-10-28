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
 *         Ring Leader Election Algorithm implementation for Contiki-NG
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

/* Log configuration */
#define LOG_MODULE "Ring"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Ring algorithm message types */
#define MSG_ELECTION    1
#define MSG_COORDINATOR 2
#define MSG_ALIVE       3

/* Timing configurations */
#define ELECTION_TIMEOUT    (8 * CLOCK_SECOND)
#define COORDINATOR_TIMEOUT (15 * CLOCK_SECOND)
#define ALIVE_INTERVAL      (10 * CLOCK_SECOND)
#define RANDOM_DELAY_MAX    (3 * CLOCK_SECOND)

/* Ring topology configuration */
#define RING_SIZE 6

/* Node states */
typedef enum {
  STATE_NORMAL,
  STATE_ELECTION,
  STATE_WAITING_COORDINATOR
} ring_state_t;

/* Message structure */
typedef struct {
  uint8_t type;
  uint16_t initiator_id;
  uint16_t candidate_id;
  uint16_t sequence;
  uint16_t target_node_id;  /* Which node should process this message */
} ring_msg_t;

/* Global variables */
static ring_state_t state = STATE_NORMAL;
static uint16_t my_node_id;
static uint16_t current_leader = 0;
static uint16_t election_sequence = 0;
static uint16_t next_node_id = 0;
static bool election_in_progress = false;

/*---------------------------------------------------------------------------*/
PROCESS(ring_process, "Ring Leader Election");
AUTOSTART_PROCESSES(&ring_process);
/*---------------------------------------------------------------------------*/

/* Function prototypes */
static void send_to_next_node(uint8_t msg_type, uint16_t initiator, uint16_t candidate, uint16_t sequence);
static void start_election(void);
static void handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src);
static uint16_t get_next_node(uint16_t node_id);

/*---------------------------------------------------------------------------*/
static uint16_t
get_next_node(uint16_t node_id)
{
  /* Ring topology: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 1 */
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

  nullnet_buf = (uint8_t *)&msg;
  nullnet_len = sizeof(msg);
  NETSTACK_NETWORK.output(NULL); /* Broadcast, but nodes filter by target_node_id */
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
  
  /* Filter messages: only process if this message is targeted to us */
  if (msg->target_node_id != my_node_id) {
    return; /* Ignore messages not meant for us */
  }
  
  LOG_INFO("Received %s (initiator=%u, candidate=%u, seq=%u)\n",
           msg->type == MSG_ELECTION ? "ELECTION" :
           msg->type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           msg->initiator_id, msg->candidate_id, msg->sequence);

  switch (msg->type) {
    case MSG_ELECTION:
      if (msg->initiator_id == my_node_id) {
        /* Our election message returned - we are the leader */
        LOG_INFO("Election completed - I am the leader (candidate=%u)\n", msg->candidate_id);
        current_leader = msg->candidate_id;
        state = STATE_NORMAL;
        election_in_progress = false;
        
        /* Announce leadership by sending COORDINATOR message */
        send_to_next_node(MSG_COORDINATOR, my_node_id, current_leader, msg->sequence);
      } else {
        /* Forward election message, updating candidate if we have higher ID */
        uint16_t new_candidate = msg->candidate_id;
        if (my_node_id > msg->candidate_id) {
          new_candidate = my_node_id;
          LOG_INFO("Updating candidate from %u to %u\n", msg->candidate_id, my_node_id);
        }
        
        /* Update our state */
        state = STATE_ELECTION;
        election_in_progress = true;
        
        /* Forward the message with original sequence */
        send_to_next_node(MSG_ELECTION, msg->initiator_id, new_candidate, msg->sequence);
      }
      break;

    case MSG_COORDINATOR:
      if (msg->initiator_id == my_node_id && current_leader == my_node_id) {
        /* Our coordinator message returned - announcement complete */
        LOG_INFO("Coordinator announcement completed the ring\n");
        /* DO NOT forward - this terminates the coordinator message */
      } else {
        /* Accept new coordinator and forward message */
        LOG_INFO("New coordinator announced: node %u\n", msg->candidate_id);
        current_leader = msg->candidate_id;
        state = STATE_NORMAL;
        election_in_progress = false;
        
        /* Forward the coordinator message with original sequence */
        send_to_next_node(MSG_COORDINATOR, msg->initiator_id, msg->candidate_id, msg->sequence);
      }
      break;

    case MSG_ALIVE:
      if (msg->initiator_id == my_node_id && current_leader == my_node_id) {
        /* Our alive message returned - heartbeat complete */
        LOG_INFO("Alive message completed the ring\n");
        /* DO NOT forward - this terminates the alive message */
      } else if (msg->initiator_id == current_leader) {
        /* Forward alive message from current leader */
        LOG_INFO("Leader %u is alive - forwarding\n", msg->initiator_id);
        send_to_next_node(MSG_ALIVE, msg->initiator_id, msg->candidate_id, msg->sequence);
      } else {
        LOG_WARN("Received alive from non-leader node %u\n", msg->initiator_id);
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
  static struct etimer election_timer;
  static struct etimer coordinator_timer;
  static struct etimer alive_timer;
  static struct etimer random_delay_timer;

  PROCESS_BEGIN();

  /* Initialize node ID based on Cooja mote ID */
  my_node_id = simMoteID;
  if (my_node_id == 0) {
    my_node_id = 1; /* Ensure non-zero ID */
  }

  /* Determine next node in ring */
  next_node_id = get_next_node(my_node_id);

  LOG_INFO("Ring node %u starting (next node: %u)\n", my_node_id, next_node_id);

  /* Initialize nullnet */
  nullnet_buf = NULL;
  nullnet_len = 0;
  nullnet_set_input_callback(input_callback);

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

  while (1) {
    PROCESS_WAIT_EVENT();

    if (ev == PROCESS_EVENT_TIMER) {
      if (data == &election_timer) {
        if (state == STATE_ELECTION && election_in_progress) {
          LOG_INFO("Election timeout - restarting election\n");
          election_in_progress = false;
          start_election();
          etimer_reset(&election_timer);
        }
      } else if (data == &coordinator_timer) {
        if (current_leader == 0 && !election_in_progress) {
          LOG_INFO("Coordinator timeout - starting new election\n");
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        }
        etimer_reset(&coordinator_timer);
      } else if (data == &alive_timer) {
        if (current_leader == my_node_id) {
          /* Send alive message around the ring */
          send_to_next_node(MSG_ALIVE, my_node_id, my_node_id, election_sequence);
        }
        etimer_reset(&alive_timer);
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/