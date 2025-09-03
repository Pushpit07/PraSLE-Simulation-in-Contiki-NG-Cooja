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
 *         Bully Leader Election Algorithm implementation for Contiki-NG
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
#define LOG_MODULE "Bully"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Bully algorithm message types */
#define MSG_ELECTION    1
#define MSG_ANSWER      2
#define MSG_COORDINATOR 3
#define MSG_ALIVE       4

/* Timing configurations */
#define ELECTION_TIMEOUT    (3 * CLOCK_SECOND)
#define COORDINATOR_TIMEOUT (10 * CLOCK_SECOND)
#define ALIVE_INTERVAL      (5 * CLOCK_SECOND)
#define RANDOM_DELAY_MAX    (2 * CLOCK_SECOND)

/* Node states */
typedef enum {
  STATE_NORMAL,
  STATE_ELECTION,
  STATE_WAITING_COORDINATOR
} bully_state_t;

/* Message structure */
typedef struct {
  uint8_t type;
  uint16_t node_id;
  uint16_t target_id;  /* For directed messages */
  uint16_t sequence;
} bully_msg_t;

/* Global variables */
static bully_state_t state = STATE_NORMAL;
static uint16_t my_node_id;
static uint16_t current_leader = 0;
static uint16_t election_sequence = 0;
static bool election_response_received = false;

/*---------------------------------------------------------------------------*/
PROCESS(bully_process, "Bully Leader Election");
AUTOSTART_PROCESSES(&bully_process);
/*---------------------------------------------------------------------------*/

/* Function prototypes */
static void send_message(uint8_t msg_type, uint16_t target_id);
static void broadcast_message(uint8_t msg_type);
static void start_election(void);
static void handle_message(const uint8_t *data, uint16_t len, const linkaddr_t *src);

/*---------------------------------------------------------------------------*/
static void
send_message(uint8_t msg_type, uint16_t target_id)
{
  static bully_msg_t msg; /* Make it static to persist */
  msg.type = msg_type;
  msg.node_id = my_node_id;
  msg.target_id = target_id;
  msg.sequence = election_sequence;

  LOG_INFO("Sending %s to node %u\n", 
           msg_type == MSG_ELECTION ? "ELECTION" :
           msg_type == MSG_ANSWER ? "ANSWER" :
           msg_type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           target_id);

  nullnet_buf = (uint8_t *)&msg;
  nullnet_len = sizeof(msg);
  NETSTACK_NETWORK.output(NULL); /* Use broadcast for simplicity */
}

/*---------------------------------------------------------------------------*/
static void
broadcast_message(uint8_t msg_type)
{
  static bully_msg_t msg; /* Make it static to persist */
  msg.type = msg_type;
  msg.node_id = my_node_id;
  msg.target_id = 0; /* Broadcast */
  msg.sequence = election_sequence;

  LOG_INFO("Broadcasting %s\n", 
           msg_type == MSG_ELECTION ? "ELECTION" :
           msg_type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE");

  nullnet_buf = (uint8_t *)&msg;
  nullnet_len = sizeof(msg);
  NETSTACK_NETWORK.output(NULL); /* Broadcast */
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
  
  state = STATE_ELECTION;
  election_sequence++;
  election_response_received = false;

  /* Broadcast ELECTION message - higher priority nodes will respond */
  broadcast_message(MSG_ELECTION);
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

  LOG_INFO("Received %s from node %u (seq %u)\n",
           msg->type == MSG_ELECTION ? "ELECTION" :
           msg->type == MSG_ANSWER ? "ANSWER" :
           msg->type == MSG_COORDINATOR ? "COORDINATOR" : "ALIVE",
           sender_id, msg->sequence);

  switch (msg->type) {
    case MSG_ELECTION:
      /* Only respond if message is broadcast or directed to us */
      if (msg->target_id == 0 || msg->target_id == my_node_id) {
        /* Respond with ANSWER if we have higher priority than sender */
        if (my_node_id > sender_id) {
          send_message(MSG_ANSWER, sender_id);
          LOG_INFO("Sent ANSWER to node %u (I have higher priority)\n", sender_id);
          /* Start our own election if not already doing so */
          if (state != STATE_ELECTION) {
            start_election();
          }
        }
      }
      break;

    case MSG_ANSWER:
      /* Only process if this answer is for us */
      if (msg->target_id == my_node_id && state == STATE_ELECTION) {
        election_response_received = true;
        LOG_INFO("Received ANSWER from node %u, backing down from election\n", sender_id);
        state = STATE_WAITING_COORDINATOR;
      }
      break;

    case MSG_COORDINATOR:
      LOG_INFO("New coordinator: node %u\n", sender_id);
      current_leader = sender_id;
      state = STATE_NORMAL;
      break;

    case MSG_ALIVE:
      if (sender_id == current_leader) {
        LOG_INFO("Leader %u is alive\n", sender_id);
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
PROCESS_THREAD(bully_process, ev, data)
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

  LOG_INFO("Bully node %u starting\n", my_node_id);

  /* Initialize nullnet */
  nullnet_buf = NULL;
  nullnet_len = 0;
  nullnet_set_input_callback(input_callback);

  /* Random delay before starting to avoid synchronized starts */
  etimer_set(&random_delay_timer, random_rand() % RANDOM_DELAY_MAX);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&random_delay_timer));

  /* Start initial election after delay */
  start_election();
  etimer_set(&election_timer, ELECTION_TIMEOUT);

  /* Set up coordinator monitoring timer */
  etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);

  /* Set up alive message timer for when we become leader */
  etimer_set(&alive_timer, ALIVE_INTERVAL);

  while (1) {
    PROCESS_WAIT_EVENT();

    if (ev == PROCESS_EVENT_TIMER) {
      if (data == &election_timer) {
        if (state == STATE_ELECTION) {
          if (!election_response_received) {
            /* No higher priority node responded, we become coordinator */
            LOG_INFO("No responses received, becoming coordinator\n");
            current_leader = my_node_id;
            state = STATE_NORMAL;
            broadcast_message(MSG_COORDINATOR);
            etimer_reset(&alive_timer);
          } else {
            /* Wait for coordinator announcement */
            etimer_set(&coordinator_timer, COORDINATOR_TIMEOUT);
          }
        }
      } else if (data == &coordinator_timer) {
        if (state == STATE_WAITING_COORDINATOR || current_leader == 0) {
          LOG_INFO("No coordinator announcement received, starting new election\n");
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        } else if (current_leader != my_node_id) {
          /* Check if current leader is still alive */
          LOG_INFO("Checking if coordinator %u is still alive\n", current_leader);
          start_election();
          etimer_set(&election_timer, ELECTION_TIMEOUT);
        }
        etimer_reset(&coordinator_timer);
      } else if (data == &alive_timer) {
        if (current_leader == my_node_id) {
          /* Send alive message as coordinator */
          broadcast_message(MSG_ALIVE);
        }
        etimer_reset(&alive_timer);
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/