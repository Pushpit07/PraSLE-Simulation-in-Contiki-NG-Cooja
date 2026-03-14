/*
 * Copyright (c) 2024, TU Dresden
 * All rights reserved.
 *
 * Leader Election Algorithm Framework - Standardized Metrics
 *
 * This header defines a common metrics structure that all leader election
 * algorithms must use for fair comparison. The metrics are output in CSV
 * format for analysis.
 */

#ifndef ELECTION_METRICS_H_
#define ELECTION_METRICS_H_

#include "election-common.h"

/*---------------------------------------------------------------------------*/
/* Enable/Disable Metrics Collection */
/*---------------------------------------------------------------------------*/
#ifndef ENABLE_METRICS
#define ENABLE_METRICS 1
#endif

#if ENABLE_METRICS

/*---------------------------------------------------------------------------*/
/* Standardized Metrics Structure
 *
 * All algorithms MUST populate these universal metrics to enable fair
 * comparison. Algorithm-specific metrics are stored in a union to save
 * memory on constrained devices.
 */
/*---------------------------------------------------------------------------*/
typedef struct {
  /* ========== Universal Metrics (All Algorithms MUST Report) ========== */

  /* Election Performance */
  uint32_t elections_started;          /* Total elections initiated by this node */
  uint32_t elections_completed;        /* Elections that reached conclusion */
  clock_time_t first_convergence_time; /* Time from start to first stable leader */
  clock_time_t last_election_start;    /* Timestamp when last election started */
  clock_time_t total_election_time;    /* Cumulative time spent in election states */
  bool first_convergence_recorded;     /* Flag: has first convergence been recorded? */
  clock_time_t process_start_time;     /* When the process started */

  /* Message Metrics */
  uint32_t messages_sent;              /* Total messages transmitted */
  uint32_t messages_received;          /* Total messages received */
  uint32_t bytes_sent;                 /* Total bytes transmitted */
  uint32_t bytes_received;             /* Total bytes received */

  /* Leader Metrics */
  uint32_t leader_changes;             /* Number of leader changes */
  uint16_t current_leader;             /* Current believed leader ID */
  uint16_t previous_leader;            /* Previous leader ID */
  clock_time_t leader_tenure_start;    /* When current leader started */
  clock_time_t total_time_as_leader;   /* Cumulative time as leader */
  bool am_leader;                      /* Am I currently the leader? */

  /* State Metrics */
  uint32_t state_transitions;          /* Total state machine transitions */
  clock_time_t time_in_normal;         /* Time in stable/normal state */
  clock_time_t time_in_election;       /* Time during election states */
  clock_time_t time_in_waiting;        /* Time in waiting states */
  clock_time_t last_state_change;      /* Timestamp of last state change */
  election_state_t current_state;      /* Current state */
  election_state_t last_state;         /* Previous state for tracking */

  /* Fault Detection */
  uint32_t timeouts_detected;          /* Leader/message timeouts detected */
  uint32_t heartbeats_received;        /* Alive/heartbeat messages received */
  uint32_t heartbeats_sent;            /* Alive/heartbeat messages sent */

  /* Energy Metrics (from Energest, cumulative ticks) */
  uint32_t energest_cpu;               /* CPU active ticks */
  uint32_t energest_lpm;               /* Low-power mode ticks */
  uint32_t energest_tx;                /* Radio TX ticks */
  uint32_t energest_rx;                /* Radio RX ticks */

  /* ========== Algorithm-Specific Metrics ========== */
  /* Stored in a union to save memory - access based on CURRENT_ALGORITHM */

  union {
    /* Bully Algorithm Specific */
    struct {
      uint32_t elections_won;              /* Elections won by this node */
      uint32_t elections_lost;             /* Elections lost to higher-priority node */
      uint32_t msg_election_sent;          /* ELECTION messages sent */
      uint32_t msg_answer_sent;            /* ANSWER messages sent */
      uint32_t msg_coordinator_sent;       /* COORDINATOR messages sent */
      uint32_t msg_alive_sent;             /* ALIVE messages sent */
      uint32_t msg_election_recv;          /* ELECTION messages received */
      uint32_t msg_answer_recv;            /* ANSWER messages received */
      uint32_t msg_coordinator_recv;       /* COORDINATOR messages received */
      uint32_t msg_alive_recv;             /* ALIVE messages received */
      uint32_t duplicates_filtered;        /* Duplicate messages filtered */
      uint32_t invalid_coordinators;       /* Rejected low-priority coordinators */
      uint32_t coordinator_reannouncements;/* Partition healing: coordinator re-announcements */
      uint32_t alive_adoptions;            /* Partition healing: alive-based adoptions */
    } bully;

    /* Ring Algorithm Specific */
    struct {
      uint32_t ring_completions;           /* Messages that completed the ring */
      uint32_t forwards;                   /* Messages forwarded to next node */
      uint32_t msg_election_sent;          /* ELECTION messages sent */
      uint32_t msg_coordinator_sent;       /* COORDINATOR messages sent */
      uint32_t msg_alive_sent;             /* ALIVE messages sent */
      uint32_t msg_election_recv;          /* ELECTION messages received */
      uint32_t msg_coordinator_recv;       /* COORDINATOR messages received */
      uint32_t msg_alive_recv;             /* ALIVE messages received */
      /* Dynamic ring reconfiguration metrics */
      uint32_t acks_sent;                  /* ACK messages sent */
      uint32_t acks_received;              /* ACK messages received */
      uint32_t retries;                    /* Message retries due to ACK timeout */
      uint32_t nodes_marked_unreachable;   /* Nodes marked as unreachable */
      uint32_t nodes_recovered;            /* Nodes recovered/rejoined ring */
      uint32_t ring_reconfigs;             /* Ring reconfiguration events */
    } ring;

    /* PraSLE Algorithm Specific */
    struct {
      int32_t rounds_completed;            /* Algorithm rounds completed */
      int32_t current_round;               /* Current round number */
      uint16_t min_value;                  /* Current min ranking value */
      uint32_t value_updates;              /* Times min/leader values updated */
      uint32_t broadcasts;                 /* Broadcast messages sent */
    } prasle;

  } algo;

} election_metrics_t;

/*---------------------------------------------------------------------------*/
/* Global Metrics Instance (defined in election-metrics.c) */
/*---------------------------------------------------------------------------*/
extern election_metrics_t metrics;

/*---------------------------------------------------------------------------*/
/* Metrics Functions */
/*---------------------------------------------------------------------------*/

/**
 * Initialize all metrics to zero/default values
 * Call this at the start of the algorithm
 */
void metrics_init(void);

/**
 * Output the CSV header line
 * Call this once at startup before any data output
 */
void metrics_output_header(void);

/**
 * Output current metrics as a CSV data line
 * Call this periodically (e.g., every METRICS_OUTPUT_INTERVAL)
 */
void metrics_output(void);

/**
 * Track a state transition
 * Updates state timing and transition counts
 *
 * @param new_state The new state being entered
 */
void metrics_track_state(election_state_t new_state);

/**
 * Track a message being sent
 *
 * @param msg_type Algorithm-specific message type
 * @param size Size of the message in bytes
 */
void metrics_track_message_sent(uint8_t msg_type, uint16_t size);

/**
 * Track a message being received
 *
 * @param msg_type Algorithm-specific message type
 * @param size Size of the message in bytes
 */
void metrics_track_message_recv(uint8_t msg_type, uint16_t size);

/**
 * Track a leader change
 *
 * @param new_leader The new leader's node ID
 */
void metrics_track_leader_change(uint16_t new_leader);

/**
 * Track the start of an election
 */
void metrics_track_election_start(void);

/**
 * Track the end of an election
 *
 * @param won True if this node won the election
 */
void metrics_track_election_end(bool won);

/**
 * Record first convergence time
 * Call this when the first stable leader is established
 */
void metrics_record_convergence(void);

/**
 * Track a timeout event (leader failure detection)
 */
void metrics_track_timeout(void);

/**
 * Track heartbeat sent
 */
void metrics_track_heartbeat_sent(void);

/**
 * Track heartbeat received
 */
void metrics_track_heartbeat_recv(void);

/*---------------------------------------------------------------------------*/
/* CSV Output Format Macros */
/*---------------------------------------------------------------------------*/

/* Universal CSV header columns (all algorithms) */
#define METRICS_CSV_HEADER_UNIVERSAL \
  "timestamp,node_id,algorithm," \
  "first_convergence_time_ms,elections_started,elections_completed," \
  "total_election_time_ms,messages_sent,messages_received," \
  "bytes_sent,bytes_received,leader_changes,current_leader," \
  "time_in_normal_ms,time_in_election_ms,time_in_waiting_ms," \
  "state_transitions,timeouts_detected,heartbeats_sent,heartbeats_received," \
  "current_state,am_leader," \
  "energest_cpu,energest_lpm,energest_tx,energest_rx"

/* Bully-specific header columns */
#define METRICS_CSV_HEADER_BULLY \
  ",elections_won,elections_lost," \
  "msg_election_sent,msg_answer_sent,msg_coordinator_sent,msg_alive_sent," \
  "msg_election_recv,msg_answer_recv,msg_coordinator_recv,msg_alive_recv," \
  "duplicates_filtered,invalid_coordinators," \
  "coordinator_reannouncements,alive_adoptions"

/* Ring-specific header columns */
#define METRICS_CSV_HEADER_RING \
  ",ring_completions,forwards," \
  "msg_election_sent,msg_coordinator_sent,msg_alive_sent," \
  "msg_election_recv,msg_coordinator_recv,msg_alive_recv," \
  "acks_sent,acks_received,retries," \
  "nodes_marked_unreachable,nodes_recovered,ring_reconfigs"

/* PraSLE-specific header columns */
#define METRICS_CSV_HEADER_PRASLE \
  ",rounds_completed,current_round,min_value,value_updates,broadcasts"

#endif /* ENABLE_METRICS */

#endif /* ELECTION_METRICS_H_ */
