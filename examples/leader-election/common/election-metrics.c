/*
 * Copyright (c) 2024, TU Dresden
 * All rights reserved.
 *
 * Leader Election Algorithm Framework - Metrics Implementation
 */

#include "election-metrics.h"
#include "sys/node-id.h"
#include <stdio.h>
#include <string.h>

#if ENABLE_METRICS

/*---------------------------------------------------------------------------*/
/* Global Metrics Instance */
/*---------------------------------------------------------------------------*/
election_metrics_t metrics;

/*---------------------------------------------------------------------------*/
/* Initialize all metrics */
/*---------------------------------------------------------------------------*/
void
metrics_init(void)
{
  memset(&metrics, 0, sizeof(metrics));
  metrics.process_start_time = clock_time();
  metrics.current_state = ELECTION_STATE_INIT;
  metrics.last_state = ELECTION_STATE_INIT;
  metrics.last_state_change = clock_time();
  metrics.first_convergence_recorded = false;
  metrics.am_leader = false;
}

/*---------------------------------------------------------------------------*/
/* Output CSV header */
/*---------------------------------------------------------------------------*/
void
metrics_output_header(void)
{
  printf("METRICS_HEADER," METRICS_CSV_HEADER_UNIVERSAL);

#if CURRENT_ALGORITHM == ALGORITHM_BULLY
  printf(METRICS_CSV_HEADER_BULLY);
#elif CURRENT_ALGORITHM == ALGORITHM_RING
  printf(METRICS_CSV_HEADER_RING);
#elif CURRENT_ALGORITHM == ALGORITHM_PRASLE || CURRENT_ALGORITHM == ALGORITHM_PRASLE_CUSTOM
  printf(METRICS_CSV_HEADER_PRASLE);
#endif

  printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Output current metrics as CSV */
/*---------------------------------------------------------------------------*/
void
metrics_output(void)
{
  clock_time_t now = clock_time();

  /* Update state timing before output */
  clock_time_t state_duration = now - metrics.last_state_change;
  switch(metrics.current_state) {
    case ELECTION_STATE_NORMAL:
    case ELECTION_STATE_LEADER:
      metrics.time_in_normal += state_duration;
      break;
    case ELECTION_STATE_ELECTION:
      metrics.time_in_election += state_duration;
      break;
    case ELECTION_STATE_WAITING:
      metrics.time_in_waiting += state_duration;
      break;
    default:
      break;
  }
  metrics.last_state_change = now;

  /* Update leader time if we are the leader */
  if(metrics.am_leader && metrics.leader_tenure_start > 0) {
    metrics.total_time_as_leader = now - metrics.leader_tenure_start;
  }

  /* Output universal metrics */
  printf("METRICS,%lu,%u,%s,",
         (unsigned long)CLOCK_TO_MS(now - metrics.process_start_time),
         node_id,
         ALGORITHM_NAME);

  printf("%lu,%lu,%lu,",
         metrics.first_convergence_recorded ?
           (unsigned long)CLOCK_TO_MS(metrics.first_convergence_time) : 0UL,
         (unsigned long)metrics.elections_started,
         (unsigned long)metrics.elections_completed);

  printf("%lu,%lu,%lu,",
         (unsigned long)CLOCK_TO_MS(metrics.total_election_time),
         (unsigned long)metrics.messages_sent,
         (unsigned long)metrics.messages_received);

  printf("%lu,%lu,%lu,%u,",
         (unsigned long)metrics.bytes_sent,
         (unsigned long)metrics.bytes_received,
         (unsigned long)metrics.leader_changes,
         metrics.current_leader);

  printf("%lu,%lu,%lu,",
         (unsigned long)CLOCK_TO_MS(metrics.time_in_normal),
         (unsigned long)CLOCK_TO_MS(metrics.time_in_election),
         (unsigned long)CLOCK_TO_MS(metrics.time_in_waiting));

  printf("%lu,%lu,%lu,%lu,",
         (unsigned long)metrics.state_transitions,
         (unsigned long)metrics.timeouts_detected,
         (unsigned long)metrics.heartbeats_sent,
         (unsigned long)metrics.heartbeats_received);

  printf("%s,%d",
         election_state_name(metrics.current_state),
         metrics.am_leader ? 1 : 0);

  /* Output algorithm-specific metrics */
#if CURRENT_ALGORITHM == ALGORITHM_BULLY
  printf(",%lu,%lu,",
         (unsigned long)metrics.algo.bully.elections_won,
         (unsigned long)metrics.algo.bully.elections_lost);
  printf("%lu,%lu,%lu,%lu,",
         (unsigned long)metrics.algo.bully.msg_election_sent,
         (unsigned long)metrics.algo.bully.msg_answer_sent,
         (unsigned long)metrics.algo.bully.msg_coordinator_sent,
         (unsigned long)metrics.algo.bully.msg_alive_sent);
  printf("%lu,%lu,%lu,%lu,",
         (unsigned long)metrics.algo.bully.msg_election_recv,
         (unsigned long)metrics.algo.bully.msg_answer_recv,
         (unsigned long)metrics.algo.bully.msg_coordinator_recv,
         (unsigned long)metrics.algo.bully.msg_alive_recv);
  printf("%lu,%lu,",
         (unsigned long)metrics.algo.bully.duplicates_filtered,
         (unsigned long)metrics.algo.bully.invalid_coordinators);
  printf("%lu,%lu",
         (unsigned long)metrics.algo.bully.coordinator_reannouncements,
         (unsigned long)metrics.algo.bully.alive_adoptions);

#elif CURRENT_ALGORITHM == ALGORITHM_RING
  printf(",%lu,%lu,",
         (unsigned long)metrics.algo.ring.ring_completions,
         (unsigned long)metrics.algo.ring.forwards);
  printf("%lu,%lu,%lu,",
         (unsigned long)metrics.algo.ring.msg_election_sent,
         (unsigned long)metrics.algo.ring.msg_coordinator_sent,
         (unsigned long)metrics.algo.ring.msg_alive_sent);
  printf("%lu,%lu,%lu,",
         (unsigned long)metrics.algo.ring.msg_election_recv,
         (unsigned long)metrics.algo.ring.msg_coordinator_recv,
         (unsigned long)metrics.algo.ring.msg_alive_recv);
  /* Dynamic ring reconfiguration metrics */
  printf("%lu,%lu,%lu,",
         (unsigned long)metrics.algo.ring.acks_sent,
         (unsigned long)metrics.algo.ring.acks_received,
         (unsigned long)metrics.algo.ring.retries);
  printf("%lu,%lu,%lu",
         (unsigned long)metrics.algo.ring.nodes_marked_unreachable,
         (unsigned long)metrics.algo.ring.nodes_recovered,
         (unsigned long)metrics.algo.ring.ring_reconfigs);

#elif CURRENT_ALGORITHM == ALGORITHM_PRASLE || CURRENT_ALGORITHM == ALGORITHM_PRASLE_CUSTOM
  printf(",%ld,%ld,%u,%lu,%lu",
         (long)metrics.algo.prasle.rounds_completed,
         (long)metrics.algo.prasle.current_round,
         metrics.algo.prasle.min_value,
         (unsigned long)metrics.algo.prasle.value_updates,
         (unsigned long)metrics.algo.prasle.broadcasts);
#endif

  printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Track state transition */
/*---------------------------------------------------------------------------*/
void
metrics_track_state(election_state_t new_state)
{
  clock_time_t now = clock_time();
  clock_time_t state_duration = now - metrics.last_state_change;

  /* Accumulate time in previous state */
  switch(metrics.current_state) {
    case ELECTION_STATE_NORMAL:
    case ELECTION_STATE_LEADER:
      metrics.time_in_normal += state_duration;
      break;
    case ELECTION_STATE_ELECTION:
      metrics.time_in_election += state_duration;
      break;
    case ELECTION_STATE_WAITING:
      metrics.time_in_waiting += state_duration;
      break;
    default:
      break;
  }

  /* Update state tracking */
  metrics.last_state = metrics.current_state;
  metrics.current_state = new_state;
  metrics.last_state_change = now;
  metrics.state_transitions++;

  /* Track if we became leader */
  if(new_state == ELECTION_STATE_LEADER && !metrics.am_leader) {
    metrics.am_leader = true;
    metrics.leader_tenure_start = now;
  } else if(new_state != ELECTION_STATE_LEADER && metrics.am_leader) {
    metrics.total_time_as_leader += now - metrics.leader_tenure_start;
    metrics.am_leader = false;
  }
}

/*---------------------------------------------------------------------------*/
/* Track message sent */
/*---------------------------------------------------------------------------*/
void
metrics_track_message_sent(uint8_t msg_type, uint16_t size)
{
  metrics.messages_sent++;
  metrics.bytes_sent += size;

  /* Algorithm-specific message tracking */
#if CURRENT_ALGORITHM == ALGORITHM_BULLY
  switch(msg_type) {
    case 1: metrics.algo.bully.msg_election_sent++; break;
    case 2: metrics.algo.bully.msg_answer_sent++; break;
    case 3: metrics.algo.bully.msg_coordinator_sent++; break;
    case 4: metrics.algo.bully.msg_alive_sent++; break;
  }
#elif CURRENT_ALGORITHM == ALGORITHM_RING
  switch(msg_type) {
    case 1: metrics.algo.ring.msg_election_sent++; break;
    case 2: metrics.algo.ring.msg_coordinator_sent++; break;
    case 3: metrics.algo.ring.msg_alive_sent++; break;
  }
#elif CURRENT_ALGORITHM == ALGORITHM_PRASLE || CURRENT_ALGORITHM == ALGORITHM_PRASLE_CUSTOM
  metrics.algo.prasle.broadcasts++;
#endif
}

/*---------------------------------------------------------------------------*/
/* Track message received */
/*---------------------------------------------------------------------------*/
void
metrics_track_message_recv(uint8_t msg_type, uint16_t size)
{
  metrics.messages_received++;
  metrics.bytes_received += size;

  /* Algorithm-specific message tracking */
#if CURRENT_ALGORITHM == ALGORITHM_BULLY
  switch(msg_type) {
    case 1: metrics.algo.bully.msg_election_recv++; break;
    case 2: metrics.algo.bully.msg_answer_recv++; break;
    case 3: metrics.algo.bully.msg_coordinator_recv++; break;
    case 4: metrics.algo.bully.msg_alive_recv++; break;
  }
#elif CURRENT_ALGORITHM == ALGORITHM_RING
  switch(msg_type) {
    case 1: metrics.algo.ring.msg_election_recv++; break;
    case 2: metrics.algo.ring.msg_coordinator_recv++; break;
    case 3: metrics.algo.ring.msg_alive_recv++; break;
  }
#endif
}

/*---------------------------------------------------------------------------*/
/* Track leader change */
/*---------------------------------------------------------------------------*/
void
metrics_track_leader_change(uint16_t new_leader)
{
  if(new_leader != metrics.current_leader) {
    metrics.previous_leader = metrics.current_leader;
    metrics.current_leader = new_leader;
    metrics.leader_changes++;

    /* Record first convergence if this is the first leader */
    if(!metrics.first_convergence_recorded && new_leader > 0) {
      metrics_record_convergence();
    }
  }
}

/*---------------------------------------------------------------------------*/
/* Track election start */
/*---------------------------------------------------------------------------*/
void
metrics_track_election_start(void)
{
  metrics.elections_started++;
  metrics.last_election_start = clock_time();
}

/*---------------------------------------------------------------------------*/
/* Track election end */
/*---------------------------------------------------------------------------*/
void
metrics_track_election_end(bool won)
{
  clock_time_t now = clock_time();
  metrics.elections_completed++;

  if(metrics.last_election_start > 0) {
    metrics.total_election_time += now - metrics.last_election_start;
  }

#if CURRENT_ALGORITHM == ALGORITHM_BULLY
  if(won) {
    metrics.algo.bully.elections_won++;
  } else {
    metrics.algo.bully.elections_lost++;
  }
#endif
}

/*---------------------------------------------------------------------------*/
/* Record first convergence */
/*---------------------------------------------------------------------------*/
void
metrics_record_convergence(void)
{
  if(!metrics.first_convergence_recorded) {
    metrics.first_convergence_time = clock_time() - metrics.process_start_time;
    metrics.first_convergence_recorded = true;
  }
}

/*---------------------------------------------------------------------------*/
/* Track timeout event */
/*---------------------------------------------------------------------------*/
void
metrics_track_timeout(void)
{
  metrics.timeouts_detected++;
}

/*---------------------------------------------------------------------------*/
/* Track heartbeat sent */
/*---------------------------------------------------------------------------*/
void
metrics_track_heartbeat_sent(void)
{
  metrics.heartbeats_sent++;
}

/*---------------------------------------------------------------------------*/
/* Track heartbeat received */
/*---------------------------------------------------------------------------*/
void
metrics_track_heartbeat_recv(void)
{
  metrics.heartbeats_received++;
}

#endif /* ENABLE_METRICS */
