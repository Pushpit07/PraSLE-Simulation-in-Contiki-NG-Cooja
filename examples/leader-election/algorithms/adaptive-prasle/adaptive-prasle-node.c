/**
 * \file   adaptive-prasle-node.c
 * \brief  Adaptive-PraSLE: Enhanced Self-Stabilizing Leader Election
 * \author Pushpit Bhardwaj
 *
 * This implementation extends the PraSLE algorithm with:
 *   1. Energy-aware leader selection (duty-cycle based)
 *   2. Link-quality aware scoring (RSSI/ETX/LQI with freshness confidence)
 *   3. Connectivity-aware scoring (neighbor count)
 *   4. CPU availability scoring
 *   5. Controlled leader rotation with handover protocol
 *   6. Adaptive timeout management (Jacobson-style RTT estimation)
 *   7. Backup-based fast recovery (when ADAPTIVE_RESET_CYCLES=0)
 *   8. Reset-cycle based recovery (default, like PraSLE)
 *
 * Composite Scoring:
 *   Score = Energy(30%) + LinkQuality(30%) + Connectivity(20%) + CPU(20%)
 *   NodeID is used only as a tiebreaker in is_better() when scores are equal.
 *
 * Link Freshness acts as a confidence multiplier on the Link Quality component.
 * Lower scores are better for leader selection.
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"
#include "sys/node-id.h"
#include "sys/energest.h"
#include "dev/radio.h"
#include "dev/moteid.h"
#include "random.h"
#include <string.h>
#include <stdio.h>

/* Common framework headers */
#include "election-common.h"
#include "election-metrics.h"

/* Algorithm-specific configuration */
#include "adaptive-prasle-config.h"

/*---------------------------------------------------------------------------*/
/* LOGGING CONFIGURATION */
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "A-PraSLE"
#define LOG_LEVEL LOG_LEVEL_INFO

/*---------------------------------------------------------------------------*/
/* UDP CONFIGURATION */
/*---------------------------------------------------------------------------*/
/**
 * UDP_PORT: Port number for Adaptive-PraSLE algorithm messages
 * - All nodes listen on this port for election messages
 * - Messages are sent to IPv6 multicast address for broadcast
 * - Uses same port as PraSLE for consistency
 */
#define UDP_PORT ADAPTIVE_PRASLE_UDP_PORT

/**
 * UDP connection for sending/receiving Adaptive-PraSLE messages
 * - Uses IPv6 link-local multicast (ff02::1) for broadcast-style communication
 * - Link-local multicast reaches all nodes in the network
 */
static struct simple_udp_connection udp_conn;

/*---------------------------------------------------------------------------*/
/* GLOBAL STATE VARIABLES */
/*---------------------------------------------------------------------------*/
static uint16_t my_node_id;
static int round_counter;
static adaptive_neighbor_info_t neighbors[MAX_NEIGHBORS];
static uint8_t num_neighbors = 0;
static uint16_t mini;           /* Current min value (composite score) */
static uint16_t temp_mini;      /* Temporary min value for current round */
static uint16_t leaderi;        /* Current leader ID */
static uint16_t temp_leaderi;   /* Temporary leader ID for current round */
static bool election_converged = false;
static clock_time_t start_time = 0;
static algorithm_state_t algorithm_state = STATE_INIT;

/* Reset-cycle tracking for fast recovery */
#if ADAPTIVE_RESET_CYCLES
static uint16_t election_cycle = 0;
#endif

/* Energy tracking */
static uint8_t my_energy_level = INITIAL_BATTERY_LEVEL;
#if ADAPTIVE_ENERGY_AWARE && ENERGEST_CONF_ON
static uint64_t last_energest_tx = 0;
static uint64_t last_energest_rx = 0;
static uint64_t last_energest_total = 0;
#endif

/* Link quality tracking */
static int8_t my_avg_rssi = RSSI_UNKNOWN;

/* RTT estimation for adaptive timeouts */
#if ADAPTIVE_TIMEOUTS
static rtt_state_t rtt_state = {
  .estimate_ms = INITIAL_RTT_ESTIMATE_MS,
  .variance_ms = INITIAL_RTT_VARIANCE_MS,
  .last_send = 0,
  .pending_seq = 0,
  .waiting_response = false
};
#endif

/* Backup list for fast recovery */
#if ADAPTIVE_BACKUP_LIST
static backup_entry_t backup_list[BACKUP_LIST_SIZE];
static uint8_t backup_count = 0;
#endif

/* Leader rotation state */
#if ADAPTIVE_LEADER_ROTATION && !ADAPTIVE_RESET_CYCLES
static rotation_state_t rotation_state = ROTATION_IDLE;
static uint16_t handover_target = 0;
static clock_time_t leadership_start = 0;
static uint8_t handover_retries = 0;
static clock_time_t handover_start_time = 0;
static uint16_t handover_seq = 0;
#endif

/* Message sequence numbers */
static uint16_t msg_seq_num = 0;
#if !ADAPTIVE_RESET_CYCLES
static uint16_t heartbeat_seq = 0;
#endif

/*---------------------------------------------------------------------------*/
/* TIMER MANAGEMENT */
/*---------------------------------------------------------------------------*/
#if ENABLE_METRICS && !ADAPTIVE_RESET_CYCLES
static struct etimer metrics_timer;
#endif

/*---------------------------------------------------------------------------*/
/* CONTIKI-NG PROCESS DEFINITION */
/*---------------------------------------------------------------------------*/
PROCESS(adaptive_prasle_process, "Adaptive-PraSLE Leader Election");
AUTOSTART_PROCESSES(&adaptive_prasle_process);

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES */
/*---------------------------------------------------------------------------*/
static uint8_t get_energy_level(void);
static int8_t get_average_rssi(void);
static uint8_t get_average_link_freshness(void);
static uint8_t get_valid_neighbor_count(void);
static uint8_t get_cpu_usage(void);
static uint16_t get_ranking_value(void);
static void get_list_of_neighbors(void);
static void send_message_to_neighbors(void);
static void handle_message(const uint8_t *data, uint16_t len);
static bool is_better(uint16_t m1, uint16_t l1, uint16_t m2, uint16_t l2);
static void check_convergence(void);
static bool is_neighbor_acceptable(adaptive_neighbor_info_t *nbr);
static void update_link_quality(uint16_t sender_id, int8_t rssi, uint8_t lqi);
static adaptive_neighbor_info_t* find_neighbor(uint16_t node_id);
static clock_time_t get_adaptive_timeout(void);
static bool is_logical_neighbor(uint16_t node_id);

#if ADAPTIVE_BACKUP_LIST
static void init_backup_list(void);
static void update_backup_list(void);
static void handle_heartbeat(adaptive_prasle_msg_t *msg);
#if !ADAPTIVE_RESET_CYCLES
static void send_leader_heartbeat(void);
static void check_neighbor_failures(void);
static void trigger_leader_recovery(void);
#endif
#endif

#if ADAPTIVE_LEADER_ROTATION && !ADAPTIVE_RESET_CYCLES
static bool should_initiate_handover(void);
static uint16_t select_handover_successor(void);
static void initiate_handover(uint16_t successor_id);
static void handle_handover_request(handover_msg_t *msg);
static void handle_handover_ack(handover_msg_t *msg);
#endif

#if !ADAPTIVE_RESET_CYCLES
static void restart_election(void);
#endif

/*===========================================================================*/
/* ENERGY MONITORING IMPLEMENTATION                                          */
/*===========================================================================*/

/**
 * Get current energy level as percentage (0-100)
 * Uses radio duty cycle as inverse proxy for remaining energy.
 */
static uint8_t
get_energy_level(void)
{
#if ADAPTIVE_ENERGY_AWARE && ENERGEST_CONF_ON
  energest_flush();

  uint64_t tx_time = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  uint64_t rx_time = energest_type_time(ENERGEST_TYPE_LISTEN);

  /* Calculate delta since last measurement */
  uint64_t delta_tx = tx_time - last_energest_tx;
  uint64_t delta_rx = rx_time - last_energest_rx;

  /* Use CPU + LPM time as total time proxy */
  uint64_t cpu_time = energest_type_time(ENERGEST_TYPE_CPU);
  uint64_t lpm_time = energest_type_time(ENERGEST_TYPE_LPM);
  uint64_t total_time = cpu_time + lpm_time;
  uint64_t delta_total = total_time - last_energest_total;

  /* Update last values */
  last_energest_tx = tx_time;
  last_energest_rx = rx_time;
  last_energest_total = total_time;

  if (delta_total > 0) {
    /* Calculate duty cycle as percentage */
    uint64_t active_time = delta_tx + delta_rx;
    uint8_t duty_cycle = (uint8_t)((active_time * 100) / delta_total);
    duty_cycle = CLAMP(duty_cycle, 0, 100);

    /* Drain battery based on duty cycle */
    uint8_t drain = (duty_cycle * BATTERY_DRAIN_RATE) / 100;
    if (drain > 0 && my_energy_level > drain) {
      my_energy_level -= drain;
    } else if (drain > 0) {
      my_energy_level = 1;  /* Don't go to zero */
    }
  }

  return my_energy_level;
#elif ADAPTIVE_ENERGY_AWARE
  /* Energest not enabled - use simple time-based drain simulation */
  static clock_time_t last_drain_time = 0;
  clock_time_t now = clock_time();

  if (last_drain_time == 0) {
    last_drain_time = now;
  }

  /* Drain 1% every 10 seconds of simulated time */
  clock_time_t elapsed = now - last_drain_time;
  if (elapsed > 10 * CLOCK_SECOND) {
    uint8_t drain = (uint8_t)(elapsed / (10 * CLOCK_SECOND));
    if (my_energy_level > drain) {
      my_energy_level -= drain;
    } else {
      my_energy_level = 1;
    }
    last_drain_time = now;
  }

  return my_energy_level;
#else
  return 100;
#endif
}

/*===========================================================================*/
/* LINK QUALITY IMPLEMENTATION                                               */
/*===========================================================================*/

/**
 * Get average RSSI to all valid neighbors
 */
static int8_t
get_average_rssi(void)
{
#if ADAPTIVE_LINK_QUALITY_AWARE
  int32_t sum = 0;
  uint8_t count = 0;

  for (uint8_t i = 0; i < num_neighbors; i++) {
    if (neighbors[i].valid && neighbors[i].rssi != RSSI_UNKNOWN) {
      sum += neighbors[i].rssi;
      count++;
    }
  }

  if (count > 0) {
    my_avg_rssi = (int8_t)(sum / count);
    return my_avg_rssi;
  }
  return RSSI_POOR_THRESHOLD;
#else
  return RSSI_GOOD_THRESHOLD;
#endif
}

/**
 * Get average link freshness across all valid neighbors.
 * Freshness is a 0-16 scale indicating how recently link data was updated.
 * Higher freshness = more reliable link quality data.
 */
static uint8_t
get_average_link_freshness(void)
{
#if ADAPTIVE_LINK_QUALITY_AWARE
  uint16_t sum = 0;
  uint8_t count = 0;

  for (uint8_t i = 0; i < num_neighbors; i++) {
    if (neighbors[i].valid) {
      /* Calculate freshness based on time since last heard */
      clock_time_t age = clock_time() - neighbors[i].last_heard;
      uint8_t freshness;

      if (age < T_VALUE) {
        freshness = 16;  /* Very fresh */
      } else if (age < 2 * T_VALUE) {
        freshness = 12;
      } else if (age < 4 * T_VALUE) {
        freshness = 8;
      } else if (age < 8 * T_VALUE) {
        freshness = 4;
      } else {
        freshness = 1;  /* Stale */
      }

      sum += freshness;
      count++;
    }
  }

  return (count > 0) ? (uint8_t)(sum / count) : 0;
#else
  return 16;  /* Max freshness if feature disabled */
#endif
}

/**
 * Get count of valid neighbors (connectivity metric).
 * More neighbors = more central node = better leader candidate.
 */
static uint8_t
get_valid_neighbor_count(void)
{
  uint8_t count = 0;

  for (uint8_t i = 0; i < num_neighbors; i++) {
    if (neighbors[i].valid) {
      count++;
    }
  }

  return count;
}

/**
 * Get current CPU usage as percentage (0-100).
 * Uses Energest CPU time to calculate duty cycle.
 * Lower CPU usage = more capacity for leader duties = better candidate.
 */
static uint8_t
get_cpu_usage(void)
{
#if ENERGEST_CONF_ON
  energest_flush();

  static uint64_t last_cpu_time = 0;
  static uint64_t last_total_time = 0;

  uint64_t cpu_time = energest_type_time(ENERGEST_TYPE_CPU);
  uint64_t lpm_time = energest_type_time(ENERGEST_TYPE_LPM);
  uint64_t total_time = cpu_time + lpm_time;

  /* Calculate delta since last call */
  uint64_t delta_cpu = cpu_time - last_cpu_time;
  uint64_t delta_total = total_time - last_total_time;

  /* Update last values */
  last_cpu_time = cpu_time;
  last_total_time = total_time;

  if (delta_total > 0) {
    /* CPU duty cycle = active CPU time / total time */
    return (uint8_t)((delta_cpu * 100) / delta_total);
  }
  return 50;  /* Default 50% if no data */
#else
  return 50;  /* Default: 50% if Energest not available */
#endif
}

/**
 * Update link quality metrics for a neighbor
 */
static void
update_link_quality(uint16_t sender_id, int8_t rssi, uint8_t lqi)
{
#if ADAPTIVE_LINK_QUALITY_AWARE
  adaptive_neighbor_info_t *nbr = find_neighbor(sender_id);
  if (nbr == NULL) return;

  /* EWMA update for RSSI (7/8 old + 1/8 new) */
  if (nbr->rssi == RSSI_UNKNOWN) {
    nbr->rssi = rssi;
  } else {
    nbr->rssi = (int8_t)((nbr->rssi * 7 + rssi) / 8);
  }

  /* EWMA update for LQI */
  if (nbr->lqi == 0) {
    nbr->lqi = lqi;
  } else {
    nbr->lqi = (uint8_t)((nbr->lqi * 7 + lqi) / 8);
  }

  nbr->last_heard = clock_time();
  nbr->missed_msgs = 0;
#endif
}

/**
 * Check if a neighbor has acceptable link quality
 */
static bool
is_neighbor_acceptable(adaptive_neighbor_info_t *nbr)
{
  if (nbr == NULL || !nbr->valid) return false;

  /* Check link freshness */
  clock_time_t timeout = get_adaptive_timeout();
  if (clock_time() - nbr->last_heard > 3 * timeout) {
    return false;
  }

#if ADAPTIVE_LINK_QUALITY_AWARE
  /* Check RSSI threshold */
  if (nbr->rssi != RSSI_UNKNOWN && nbr->rssi < RSSI_POOR_THRESHOLD) {
    return false;
  }

  /* Check ETX threshold */
  if (nbr->etx > ETX_MAX_ACCEPTABLE * LINK_STATS_ETX_DIVISOR) {
    return false;
  }
#endif

  return true;
}

/*===========================================================================*/
/* COMPOSITE SCORING IMPLEMENTATION                                          */
/*===========================================================================*/

/**
 * Get ranking value for this node (composite score).
 * This is the Adaptive-PraSLE equivalent of PraSLE's get_ranking_value().
 * Lower scores are better for leader selection.
 *
 * Score = Energy(30%) + LinkQuality(30%) + Connectivity(20%) + CPU(20%)
 *
 * NodeID is used only as a tiebreaker in is_better() when scores are equal.
 * Link Freshness is used as a confidence multiplier for the link quality component.
 */
static uint16_t
get_ranking_value(void)
{
  uint16_t base_score = 0;

#if ADAPTIVE_ENERGY_AWARE
  uint8_t energy = get_energy_level();
  /* Higher energy = lower score (better) */
  /* Scale: 100% energy -> 0 component, 0% energy -> ENERGY_WEIGHT component */
  uint16_t energy_component = ((100 - energy) * ENERGY_WEIGHT) / 100;
  base_score += energy_component;
#endif

#if ADAPTIVE_LINK_QUALITY_AWARE
  int8_t avg_rssi = get_average_rssi();
  uint8_t avg_freshness = get_average_link_freshness();

  /* Better RSSI = lower score (better) */
  /* Scale: -60dBm (good) -> 0, -85dBm (poor) -> 100 */
  int16_t rssi_range = RSSI_GOOD_THRESHOLD - RSSI_POOR_THRESHOLD;
  int16_t rssi_normalized;

  if (avg_rssi >= RSSI_GOOD_THRESHOLD) {
    rssi_normalized = 0;
  } else if (avg_rssi <= RSSI_POOR_THRESHOLD) {
    rssi_normalized = 100;
  } else {
    rssi_normalized = 100 - ((avg_rssi - RSSI_POOR_THRESHOLD) * 100 / rssi_range);
  }

#if FRESHNESS_CONFIDENCE_SCALE
  /* Apply freshness as confidence multiplier:
   * - Freshness 0-3 (stale): 50% confidence, effectively doubles the score penalty
   * - Freshness 4-16: scales from 50% to 100% confidence
   * Low freshness = less confident in link data = penalize by increasing score */
  uint8_t freshness_factor;
  if (avg_freshness < FRESHNESS_MIN_THRESHOLD) {
    freshness_factor = 50;  /* 50% confidence if stale */
  } else {
    freshness_factor = 50 + (avg_freshness * 50 / 16);  /* 50-100% */
  }
  /* Invert: lower confidence should increase the link component penalty */
  uint16_t link_component = (rssi_normalized * LINK_QUALITY_WEIGHT * 100) / (100 * freshness_factor);
#else
  uint16_t link_component = (rssi_normalized * LINK_QUALITY_WEIGHT) / 100;
#endif
  base_score += link_component;
#endif

  /* Connectivity: More neighbors = lower score (better leader candidate) */
  uint8_t neighbor_count = get_valid_neighbor_count();
  /* Scale: 0 neighbors = max penalty, MAX_NEIGHBORS = 0 penalty */
  uint16_t connectivity_normalized;
  if (neighbor_count >= MAX_NEIGHBORS) {
    connectivity_normalized = 0;
  } else {
    connectivity_normalized = 100 - (neighbor_count * 100 / MAX_NEIGHBORS);
  }
  uint16_t connectivity_component = (connectivity_normalized * CONNECTIVITY_WEIGHT) / 100;
  base_score += connectivity_component;

  /* CPU Availability: Lower CPU usage = lower score (better) */
  uint8_t cpu_usage = get_cpu_usage();
  /* Scale: 0% usage = 0 score, 100% usage = CPU_WEIGHT */
  uint16_t cpu_component = (cpu_usage * CPU_WEIGHT) / 100;
  base_score += cpu_component;

  return base_score;
}

/*===========================================================================*/
/* ADAPTIVE TIMEOUT IMPLEMENTATION                                           */
/*===========================================================================*/

/**
 * Get adaptive timeout value based on RTT estimation
 */
static clock_time_t
get_adaptive_timeout(void)
{
#if ADAPTIVE_TIMEOUTS
  /* RTO = RTT + 4 * Variance (Jacobson-style) */
  uint32_t timeout_ms = rtt_state.estimate_ms + 4 * rtt_state.variance_ms;

  /* Apply safety margin */
  timeout_ms = (uint32_t)(timeout_ms * TIMEOUT_SAFETY_MARGIN);

  /* Clamp to bounds */
  uint32_t min_ms = (uint32_t)(TIMEOUT_MIN_SECONDS * 1000);
  uint32_t max_ms = (uint32_t)(TIMEOUT_MAX_SECONDS * 1000);
  timeout_ms = CLAMP(timeout_ms, min_ms, max_ms);

  return MS_TO_CLOCK(timeout_ms);
#else
  return T_VALUE;
#endif
}

#if ADAPTIVE_TIMEOUTS
/**
 * Update RTT estimate from a message round-trip
 */
static void
update_rtt_estimate(uint32_t sample_ms)
{
  /* EWMA update (Jacobson's algorithm) */
  /* rtt_estimate = 7/8 * rtt_estimate + 1/8 * sample */
  int32_t error = (int32_t)sample_ms - (int32_t)rtt_state.estimate_ms;
  rtt_state.estimate_ms = rtt_state.estimate_ms + (error >> 3);

  /* variance = 3/4 * variance + 1/4 * |error| */
  if (error < 0) error = -error;
  rtt_state.variance_ms = rtt_state.variance_ms - (rtt_state.variance_ms >> 2) + (error >> 2);

  LOG_DBG("RTT updated: est=%lu ms, var=%lu ms\n",
          (unsigned long)rtt_state.estimate_ms,
          (unsigned long)rtt_state.variance_ms);
}
#endif

/*===========================================================================*/
/* BACKUP LIST IMPLEMENTATION                                                */
/*===========================================================================*/

#if ADAPTIVE_BACKUP_LIST
/**
 * Initialize backup list
 */
static void
init_backup_list(void)
{
  memset(backup_list, 0, sizeof(backup_list));
  backup_count = 0;
}

/**
 * Update backup list based on current neighbor scores
 */
static void
update_backup_list(void)
{
  /* Collect candidates (valid neighbors except current leader) */
  typedef struct { uint16_t id; uint16_t score; } candidate_t;
  candidate_t candidates[MAX_NEIGHBORS];
  uint8_t num_candidates = 0;

  for (uint8_t i = 0; i < num_neighbors && num_candidates < MAX_NEIGHBORS; i++) {
    if (!is_neighbor_acceptable(&neighbors[i])) continue;
    if (neighbors[i].node_id == leaderi) continue;
    if (neighbors[i].energy_level < ENERGY_LOW_THRESHOLD) continue;

    candidates[num_candidates].id = neighbors[i].node_id;
    candidates[num_candidates].score = neighbors[i].composite_score;
    num_candidates++;
  }

  /* Sort by score (bubble sort - small list) */
  for (uint8_t i = 0; i < num_candidates; i++) {
    for (uint8_t j = 0; j < num_candidates - i - 1; j++) {
      if (candidates[j].score > candidates[j + 1].score) {
        candidate_t tmp = candidates[j];
        candidates[j] = candidates[j + 1];
        candidates[j + 1] = tmp;
      }
    }
  }

  /* Update backup list (top BACKUP_LIST_SIZE) */
  backup_count = (num_candidates < BACKUP_LIST_SIZE) ? num_candidates : BACKUP_LIST_SIZE;
  for (uint8_t i = 0; i < backup_count; i++) {
    backup_list[i].node_id = candidates[i].id;
    backup_list[i].score = candidates[i].score;
    backup_list[i].missed_heartbeats = 0;
  }

  if (backup_count > 0) {
    LOG_DBG("Backup list: ");
    for (uint8_t i = 0; i < backup_count; i++) {
      LOG_DBG_("%u(s=%u) ", backup_list[i].node_id, backup_list[i].score);
    }
    LOG_DBG_("\n");
  }
}

#if !ADAPTIVE_RESET_CYCLES
/**
 * Send leader heartbeat (only if this node is leader)
 * Note: Only used in maintenance mode, not with reset-cycle recovery
 */
static void
send_leader_heartbeat(void)
{
  if (leaderi != my_node_id) return;

  static adaptive_prasle_msg_t msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_type = MSG_HEARTBEAT;
  msg.min_value = mini;
  msg.leader_id = leaderi;
  msg.sender_id = my_node_id;
  msg.energy_level = get_energy_level();
  msg.avg_rssi = get_average_rssi();
  msg.neighbor_count = num_neighbors;
  msg.flags = FLAG_IS_LEADER;
  if (msg.energy_level < ENERGY_LOW_THRESHOLD) {
    msg.flags |= FLAG_LOW_ENERGY;
  }
  msg.seq_num = ++heartbeat_seq;

  /* Send via UDP to link-local all-nodes multicast address */
  uip_ipaddr_t dest_addr;
  uip_create_linklocal_allnodes_mcast(&dest_addr);
  simple_udp_sendto(&udp_conn, &msg, sizeof(msg), &dest_addr);

#if ENABLE_METRICS
  metrics_track_message_sent(1, sizeof(msg));
#endif

  LOG_DBG("Heartbeat sent: seq=%u, energy=%u%%\n", msg.seq_num, msg.energy_level);
}
#endif /* !ADAPTIVE_RESET_CYCLES */

/**
 * Handle received heartbeat
 */
static void
handle_heartbeat(adaptive_prasle_msg_t *msg)
{
  /* Update leader if changed */
  if (msg->leader_id != leaderi && msg->flags & FLAG_IS_LEADER) {
    uint16_t old_leader = leaderi;
    leaderi = msg->leader_id;
    mini = msg->min_value;
    LOG_INFO("Leader changed: %u -> %u\n", old_leader, leaderi);

    /* Reset election state if we were in recovery/election */
    if (algorithm_state == STATE_RECOVERY || algorithm_state == STATE_ELECTION) {
      algorithm_state = STATE_NORMAL;
      election_converged = true;
      LOG_INFO("CONVERGED: Leader=%u, Score=%u (via heartbeat)\n", leaderi, mini);
    }
  }

  /* Update sender's neighbor info */
  adaptive_neighbor_info_t *nbr = find_neighbor(msg->sender_id);
  if (nbr != NULL) {
    nbr->energy_level = msg->energy_level;
    nbr->energy_ts = clock_time();
    nbr->last_heard = clock_time();
    nbr->missed_msgs = 0;
  }
}

#if !ADAPTIVE_RESET_CYCLES
/**
 * Check for neighbor/leader failures
 * Note: Only used in maintenance mode, not with reset-cycle recovery
 */
static void
check_neighbor_failures(void)
{
  clock_time_t now = clock_time();
  clock_time_t timeout = get_adaptive_timeout() * BACKUP_FAILURE_THRESHOLD;

  for (uint8_t i = 0; i < num_neighbors; i++) {
    if (!neighbors[i].valid) continue;

    if (now - neighbors[i].last_heard > timeout) {
      neighbors[i].missed_msgs++;

      if (neighbors[i].missed_msgs >= BACKUP_FAILURE_THRESHOLD) {
        LOG_WARN("Neighbor %u failed (missed %u msgs)\n",
                 neighbors[i].node_id, neighbors[i].missed_msgs);

        /* If leader failed, trigger recovery */
        if (neighbors[i].node_id == leaderi) {
          trigger_leader_recovery();
        }

        neighbors[i].valid = false;
      }
    }
  }
}

/**
 * Trigger leader recovery from backup list
 * Note: Only used in maintenance mode, not with reset-cycle recovery
 */
static void
trigger_leader_recovery(void)
{
  uint16_t old_leader = leaderi;
  LOG_WARN("Leader %u failed, triggering recovery\n", leaderi);
  algorithm_state = STATE_RECOVERY;

#if ENABLE_METRICS
  /* Use ELECTION state during recovery since RECOVERY is not defined */
  metrics_track_state(ELECTION_STATE_ELECTION);
#endif

  /* Try backup list in order */
  for (uint8_t i = 0; i < backup_count; i++) {
    uint16_t backup_id = backup_list[i].node_id;

    /* Check if backup is still valid */
    adaptive_neighbor_info_t *nbr = find_neighbor(backup_id);
    if (nbr != NULL && is_neighbor_acceptable(nbr)) {
      LOG_INFO("Promoting backup %u to leader\n", backup_id);

      /* If this node is the backup, become leader */
      if (backup_id == my_node_id) {
        leaderi = my_node_id;
        mini = get_ranking_value();
#if ADAPTIVE_LEADER_ROTATION && !ADAPTIVE_RESET_CYCLES
        leadership_start = clock_time();
#endif
        algorithm_state = STATE_LEADER;
        send_message_to_neighbors();

#if ENABLE_METRICS
        metrics_track_leader_change(leaderi);
        metrics_track_state(ELECTION_STATE_LEADER);
#endif
      } else {
        /* Expect backup to announce itself */
        leaderi = backup_id;
        algorithm_state = STATE_NORMAL;
      }
      LOG_INFO("Leader changed: %u -> %u\n", old_leader, leaderi);
      return;
    }
  }

  /* No valid backups - restart full election */
  LOG_WARN("No valid backups, restarting election\n");
  restart_election();
}
#endif /* !ADAPTIVE_RESET_CYCLES */
#endif /* ADAPTIVE_BACKUP_LIST */

/*===========================================================================*/
/* LEADER ROTATION IMPLEMENTATION                                            */
/* Note: Only used in maintenance mode, not with reset-cycle recovery        */
/*===========================================================================*/

#if ADAPTIVE_LEADER_ROTATION && !ADAPTIVE_RESET_CYCLES
/**
 * Check if leader should initiate handover
 */
static bool
should_initiate_handover(void)
{
  if (leaderi != my_node_id) return false;  /* Only leader can initiate */
  if (rotation_state != ROTATION_IDLE) return false;  /* Already in progress */

  /* Prevent oscillation: minimum leadership term */
  if (clock_time() - leadership_start < MIN_LEADERSHIP_TERM) {
    return false;
  }

  /* Check energy threshold */
  uint8_t energy = get_energy_level();
  if (energy < ENERGY_CRITICAL_THRESHOLD) {
    LOG_INFO("Energy critical (%u%%), initiating handover\n", energy);
    return true;
  }

  return false;
}

/**
 * Select best successor for handover
 */
static uint16_t
select_handover_successor(void)
{
  uint16_t best_id = 0;
  uint16_t best_score = UINT16_MAX;

  for (uint8_t i = 0; i < num_neighbors; i++) {
    if (!is_neighbor_acceptable(&neighbors[i])) continue;

    /* Skip nodes with low energy */
    if (neighbors[i].energy_level < ENERGY_LOW_THRESHOLD) continue;

    /* Calculate/use cached score (lower is better) */
    if (neighbors[i].composite_score < best_score) {
      best_score = neighbors[i].composite_score;
      best_id = neighbors[i].node_id;
    }
  }

  return best_id;
}

/**
 * Initiate handover to successor
 */
static void
initiate_handover(uint16_t successor_id)
{
  rotation_state = ROTATION_INITIATING;
  handover_target = successor_id;
  handover_retries = 0;
  handover_start_time = clock_time();

  static handover_msg_t msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_type = MSG_HANDOVER_REQ;
  msg.old_leader = my_node_id;
  msg.new_leader = successor_id;
  msg.seq_num = ++handover_seq;

  /* Send via UDP to link-local all-nodes multicast address */
  uip_ipaddr_t dest_addr;
  uip_create_linklocal_allnodes_mcast(&dest_addr);
  simple_udp_sendto(&udp_conn, &msg, sizeof(msg), &dest_addr);

  rotation_state = ROTATION_WAITING_ACK;

#if ENABLE_METRICS
  metrics_track_message_sent(1, sizeof(msg));
#endif

  LOG_INFO("Handover initiated to node %u (seq=%u)\n", successor_id, msg.seq_num);
}

/**
 * Handle handover request (successor side)
 */
static void
handle_handover_request(handover_msg_t *msg)
{
  if (msg->new_leader != my_node_id) return;

  LOG_INFO("Received handover request from %u\n", msg->old_leader);

  /* Send acknowledgement */
  static handover_msg_t ack;
  memset(&ack, 0, sizeof(ack));
  ack.msg_type = MSG_HANDOVER_ACK;
  ack.old_leader = msg->old_leader;
  ack.new_leader = my_node_id;
  ack.seq_num = msg->seq_num;

  /* Send via UDP to link-local all-nodes multicast address */
  uip_ipaddr_t dest_addr;
  uip_create_linklocal_allnodes_mcast(&dest_addr);
  simple_udp_sendto(&udp_conn, &ack, sizeof(ack), &dest_addr);

#if ENABLE_METRICS
  metrics_track_message_sent(1, sizeof(ack));
#endif

  /* Become leader */
  leaderi = my_node_id;
  mini = get_ranking_value();
  leadership_start = clock_time();
  algorithm_state = STATE_LEADER;

#if ENABLE_METRICS
  metrics_track_leader_change(leaderi);
  metrics_track_state(ELECTION_STATE_LEADER);
#endif

  /* Announce new leadership */
  send_message_to_neighbors();

  LOG_INFO("Accepted leadership from %u\n", msg->old_leader);
}

/**
 * Handle handover acknowledgement (old leader side)
 */
static void
handle_handover_ack(handover_msg_t *msg)
{
  if (rotation_state != ROTATION_WAITING_ACK) return;
  if (msg->old_leader != my_node_id) return;

  rotation_state = ROTATION_COMPLETING;

  /* Update leader */
  leaderi = msg->new_leader;
  algorithm_state = STATE_NORMAL;

  /* Broadcast new leader announcement */
  send_message_to_neighbors();

  rotation_state = ROTATION_IDLE;

#if ENABLE_METRICS
  metrics_track_leader_change(leaderi);
  metrics_track_state(ELECTION_STATE_NORMAL);
#endif

  LOG_INFO("Handover complete, new leader: %u\n", leaderi);
}
#endif /* ADAPTIVE_LEADER_ROTATION && !ADAPTIVE_RESET_CYCLES */

/*===========================================================================*/
/* NEIGHBOR MANAGEMENT                                                       */
/*===========================================================================*/

/**
 * Find neighbor by node ID
 */
static adaptive_neighbor_info_t*
find_neighbor(uint16_t node_id)
{
  for (uint8_t i = 0; i < num_neighbors; i++) {
    if (neighbors[i].node_id == node_id) {
      return &neighbors[i];
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Check if a node is a logical neighbor based on the configured topology
 *
 * \param node_id  The node ID to check
 * \return true if node_id is a logical neighbor, false otherwise
 *
 * This function enforces topology constraints at the application layer.
 * Since we use IPv6 multicast (ff02::1), ALL nodes receive every message.
 * This filter ensures we only process messages from nodes that are our
 * logical neighbors in the configured topology (ring, line, mesh, clique).
 *
 * This receiver-side filtering is more energy-efficient than sender-side
 * filtering (unicast), which would require K radio transmissions instead of 1.
 * The O(K) array lookup cost here is negligible compared to radio TX overhead.
 */
static bool
is_logical_neighbor(uint16_t node_id)
{
  for (uint8_t i = 0; i < num_neighbors; i++) {
    if (neighbors[i].node_id == node_id && neighbors[i].valid) {
      return true;
    }
  }
  return false;
}

/**
 * Initialize neighbor list based on topology
 */
static void
get_list_of_neighbors(void)
{
  num_neighbors = 0;
  memset(neighbors, 0, sizeof(neighbors));

#if NETWORK_TOPOLOGY == TOPOLOGY_RING
  /* Ring: connect to (i+1) mod N and (i-1) mod N */
  neighbors[num_neighbors].node_id = (my_node_id % NETWORK_SIZE) + 1;
  neighbors[num_neighbors].min_value = N_MAX + 1;
  neighbors[num_neighbors].leader_id = N_MAX + 1;
  neighbors[num_neighbors].valid = true;
  neighbors[num_neighbors].rssi = RSSI_UNKNOWN;
  neighbors[num_neighbors].last_heard = clock_time();
  num_neighbors++;

  neighbors[num_neighbors].node_id = ((my_node_id - 2 + NETWORK_SIZE) % NETWORK_SIZE) + 1;
  neighbors[num_neighbors].min_value = N_MAX + 1;
  neighbors[num_neighbors].leader_id = N_MAX + 1;
  neighbors[num_neighbors].valid = true;
  neighbors[num_neighbors].rssi = RSSI_UNKNOWN;
  neighbors[num_neighbors].last_heard = clock_time();
  num_neighbors++;

#elif NETWORK_TOPOLOGY == TOPOLOGY_LINE
  /* Line: connect to i-1 and i+1 if they exist */
  if (my_node_id > 1) {
    neighbors[num_neighbors].node_id = my_node_id - 1;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    neighbors[num_neighbors].rssi = RSSI_UNKNOWN;
    neighbors[num_neighbors].last_heard = clock_time();
    num_neighbors++;
  }
  if (my_node_id < NETWORK_SIZE) {
    neighbors[num_neighbors].node_id = my_node_id + 1;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    neighbors[num_neighbors].rssi = RSSI_UNKNOWN;
    neighbors[num_neighbors].last_heard = clock_time();
    num_neighbors++;
  }

#elif NETWORK_TOPOLOGY == TOPOLOGY_MESH
  /* 2D Grid mesh */
  int grid_size = 3;
  int row = (my_node_id - 1) / grid_size;
  int col = (my_node_id - 1) % grid_size;

  /* Up */
  if (row > 0 && num_neighbors < MAX_NEIGHBORS) {
    neighbors[num_neighbors].node_id = (row - 1) * grid_size + col + 1;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    neighbors[num_neighbors].rssi = RSSI_UNKNOWN;
    neighbors[num_neighbors].last_heard = clock_time();
    num_neighbors++;
  }
  /* Down */
  if (row < grid_size - 1 && num_neighbors < MAX_NEIGHBORS) {
    neighbors[num_neighbors].node_id = (row + 1) * grid_size + col + 1;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    neighbors[num_neighbors].rssi = RSSI_UNKNOWN;
    neighbors[num_neighbors].last_heard = clock_time();
    num_neighbors++;
  }
  /* Left */
  if (col > 0 && num_neighbors < MAX_NEIGHBORS) {
    neighbors[num_neighbors].node_id = row * grid_size + col;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    neighbors[num_neighbors].rssi = RSSI_UNKNOWN;
    neighbors[num_neighbors].last_heard = clock_time();
    num_neighbors++;
  }
  /* Right */
  if (col < grid_size - 1 && num_neighbors < MAX_NEIGHBORS) {
    neighbors[num_neighbors].node_id = row * grid_size + col + 2;
    neighbors[num_neighbors].min_value = N_MAX + 1;
    neighbors[num_neighbors].leader_id = N_MAX + 1;
    neighbors[num_neighbors].valid = true;
    neighbors[num_neighbors].rssi = RSSI_UNKNOWN;
    neighbors[num_neighbors].last_heard = clock_time();
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
      neighbors[num_neighbors].rssi = RSSI_UNKNOWN;
      neighbors[num_neighbors].last_heard = clock_time();
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

/*===========================================================================*/
/* ELECTION CORE                                                             */
/*===========================================================================*/

/**
 * Lexicographic comparison: (m1, l1) < (m2, l2)
 */
static bool
is_better(uint16_t m1, uint16_t l1, uint16_t m2, uint16_t l2)
{
  return (m1 < m2) || ((m1 == m2) && (l1 < l2));
}

/**
 * \brief Broadcast (mini, leaderi) to all neighbors via UDP multicast
 *
 * Uses IPv6 link-local all-nodes multicast (ff02::1) for broadcast.
 * All nodes receive the message; filtering is done at the receiver side
 * via is_logical_neighbor().
 *
 * DESIGN NOTE: Why broadcast instead of unicast to each logical neighbor?
 *
 * 1. ENERGY EFFICIENCY: In wireless IoT, radio transmission is the most
 *    expensive operation. One broadcast costs the same as one unicast,
 *    but reaching K neighbors via unicast requires K transmissions.
 *    Example: 10-node clique -> 1 broadcast vs 9 unicasts = 9x energy savings.
 *
 * 2. PHYSICAL LAYER: Wireless is inherently broadcast - all nodes in radio
 *    range receive every transmission regardless of addressing. Unicast
 *    only filters at MAC layer, not physical layer.
 *
 * 3. SIMPLICITY: Broadcast requires no routing knowledge or address resolution.
 *    Topology enforcement via is_logical_neighbor() at receiver is O(K) lookup.
 *
 * The receiver-side filtering cost (array search) is negligible compared to
 * the cost of additional radio transmissions that sender-side filtering would require.
 */
static void
send_message_to_neighbors(void)
{
  static adaptive_prasle_msg_t msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_type = MSG_ELECTION;
  msg.min_value = mini;
  msg.leader_id = leaderi;
  msg.sender_id = my_node_id;
  msg.energy_level = get_energy_level();
  msg.avg_rssi = get_average_rssi();
  msg.neighbor_count = num_neighbors;
  msg.flags = 0;
  if (leaderi == my_node_id) {
    msg.flags |= FLAG_IS_LEADER;
  }
  if (msg.energy_level < ENERGY_LOW_THRESHOLD) {
    msg.flags |= FLAG_LOW_ENERGY;
  }
  msg.seq_num = ++msg_seq_num;

  LOG_INFO("Round %d: Broadcasting (min=%u, leader=%u, energy=%u%%)\n",
           round_counter, mini, leaderi, msg.energy_level);

#if ENABLE_METRICS
  metrics_track_message_sent(1, sizeof(msg));
#endif

#if ADAPTIVE_TIMEOUTS
  rtt_state.last_send = clock_time();
  rtt_state.pending_seq = msg.seq_num;
  rtt_state.waiting_response = true;
#endif

  /* Send via UDP to link-local all-nodes multicast address */
  uip_ipaddr_t dest_addr;
  uip_create_linklocal_allnodes_mcast(&dest_addr);
  simple_udp_sendto(&udp_conn, &msg, sizeof(msg), &dest_addr);
}

/**
 * \brief Handle received Adaptive-PraSLE message
 *
 * \param data  Pointer to received message data
 * \param len   Length of received data
 *
 * This function handles incoming messages. It:
 * 1. Validates message size
 * 2. Filters self-messages (broadcast echo)
 * 3. CRITICAL: Filters non-neighbor messages for election (topology enforcement)
 * 4. Updates temp values if received pair is lexicographically smaller
 */
static void
handle_message(const uint8_t *data, uint16_t len)
{
  if (len < sizeof(uint8_t)) {
    return;
  }

  uint8_t msg_type = data[0];

  /* Get radio metrics for this reception */
  radio_value_t rssi_val = RSSI_UNKNOWN;
  radio_value_t lqi_val = 0;
  NETSTACK_RADIO.get_value(RADIO_PARAM_LAST_RSSI, &rssi_val);
  NETSTACK_RADIO.get_value(RADIO_PARAM_LAST_LINK_QUALITY, &lqi_val);

  switch (msg_type) {
    case MSG_ELECTION:
    case MSG_HEARTBEAT:
      if (len != sizeof(adaptive_prasle_msg_t)) {
        LOG_WARN("Invalid election/heartbeat message size\n");
        return;
      }
      break;

    case MSG_HANDOVER_REQ:
    case MSG_HANDOVER_ACK:
      if (len != sizeof(handover_msg_t)) {
        LOG_WARN("Invalid handover message size\n");
        return;
      }
      break;

    default:
      LOG_WARN("Unknown message type: %u\n", msg_type);
      return;
  }

  /* Handle based on message type */
  switch (msg_type) {
    case MSG_ELECTION: {
      adaptive_prasle_msg_t *msg = (adaptive_prasle_msg_t *)data;
      uint16_t sender_id = msg->sender_id;

      /* Filter self-messages (broadcast echo) */
      if (sender_id == my_node_id) {
        return;
      }

      /*
       * CRITICAL: Filter messages from non-neighbors (topology enforcement)
       *
       * With IPv6/UDP multicast, ALL nodes in the network receive the message.
       * Adaptive-PraSLE operates on logical topologies (ring, line, mesh, clique).
       * Each node has specific logical neighbors based on the topology.
       * We must filter to only process messages from logical neighbors.
       */
      if (!is_logical_neighbor(sender_id)) {
        return;  /* Ignore messages from non-neighbors */
      }

#if ENABLE_METRICS
      metrics_track_message_recv(1, len);
#endif

      /* Update link quality */
      update_link_quality(sender_id, (int8_t)rssi_val, (uint8_t)lqi_val);

#if ADAPTIVE_TIMEOUTS
      /* Update RTT if this is a response to our message */
      if (rtt_state.waiting_response) {
        uint32_t rtt_ms = CLOCK_TO_MS(clock_time() - rtt_state.last_send);
        update_rtt_estimate(rtt_ms);
        rtt_state.waiting_response = false;
      }
#endif

      LOG_INFO("Round %d: Recv from neighbor %u: (min=%u, leader=%u, energy=%u%%)\n",
               round_counter, sender_id, msg->min_value, msg->leader_id,
               msg->energy_level);

      /* Update neighbor info */
      adaptive_neighbor_info_t *nbr = find_neighbor(sender_id);
      if (nbr != NULL) {
        nbr->min_value = msg->min_value;
        nbr->leader_id = msg->leader_id;
        nbr->energy_level = msg->energy_level;
        nbr->energy_ts = clock_time();
        nbr->composite_score = msg->min_value;  /* Use sender's score */
      }

      /* Compare and update temp values */
      if (is_better(msg->min_value, msg->leader_id, temp_mini, temp_leaderi)) {
        temp_mini = msg->min_value;
        temp_leaderi = msg->leader_id;

        LOG_INFO("Round %d: Updated temp to (min=%u, leader=%u)\n",
                 round_counter, temp_mini, temp_leaderi);
      }
      break;
    }

#if ADAPTIVE_BACKUP_LIST
    case MSG_HEARTBEAT: {
      adaptive_prasle_msg_t *msg = (adaptive_prasle_msg_t *)data;
      if (msg->sender_id == my_node_id) return;  /* Filter self */
#if ENABLE_METRICS
      metrics_track_message_recv(1, len);
#endif
      update_link_quality(msg->sender_id, (int8_t)rssi_val, (uint8_t)lqi_val);
      handle_heartbeat(msg);
      break;
    }
#endif

#if ADAPTIVE_LEADER_ROTATION && !ADAPTIVE_RESET_CYCLES
    case MSG_HANDOVER_REQ: {
      handover_msg_t *msg = (handover_msg_t *)data;
#if ENABLE_METRICS
      metrics_track_message_recv(1, len);
#endif
      handle_handover_request(msg);
      break;
    }

    case MSG_HANDOVER_ACK: {
      handover_msg_t *msg = (handover_msg_t *)data;
#if ENABLE_METRICS
      metrics_track_message_recv(1, len);
#endif
      handle_handover_ack(msg);
      break;
    }
#endif

    default:
      break;
  }
}

/**
 * Check and handle convergence
 */
static void
check_convergence(void)
{
  if (!election_converged && round_counter <= 0) {
    election_converged = true;
    clock_time_t convergence_time = clock_time() - start_time;

#if ENABLE_METRICS
    metrics_record_convergence();
    metrics_track_leader_change(leaderi);
#endif

    if (leaderi == my_node_id) {
      algorithm_state = STATE_LEADER;
#if ADAPTIVE_LEADER_ROTATION && !ADAPTIVE_RESET_CYCLES
      leadership_start = clock_time();
#endif
#if ENABLE_METRICS
      metrics_track_state(ELECTION_STATE_LEADER);
#endif
    } else {
      algorithm_state = STATE_NORMAL;
#if ENABLE_METRICS
      metrics_track_state(ELECTION_STATE_NORMAL);
#endif
    }

    LOG_INFO("CONVERGED: Leader=%u, Score=%u, Time=%lu ms\n",
             leaderi, mini, (unsigned long)CLOCK_TO_MS(convergence_time));
  }
}

#if !ADAPTIVE_RESET_CYCLES
/**
 * Restart full election
 * Note: Only used in maintenance mode, not with reset-cycle recovery
 */
static void
restart_election(void)
{
  round_counter = K_ROUNDS + 1;
  mini = N_MAX + 1;
  temp_mini = get_ranking_value();
  leaderi = my_node_id;
  temp_leaderi = my_node_id;
  election_converged = false;
  algorithm_state = STATE_ELECTION;
  start_time = clock_time();

#if ENABLE_METRICS
  metrics_track_state(ELECTION_STATE_ELECTION);
  metrics_track_election_start();
#endif

  LOG_INFO("Election restarted\n");
}
#endif /* !ADAPTIVE_RESET_CYCLES */

/*===========================================================================*/
/* UDP RECEIVE CALLBACK                                                      */
/*===========================================================================*/

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
  handle_message(data, datalen);
}

/*===========================================================================*/
/* MAIN PROCESS                                                              */
/*===========================================================================*/

PROCESS_THREAD(adaptive_prasle_process, ev, data)
{
  static struct etimer round_timer;
  static struct etimer recv_timer;
#if !ADAPTIVE_RESET_CYCLES
  static struct etimer heartbeat_timer;
  static struct etimer energy_timer;
  static struct etimer handover_timer;
#endif
#if ENABLE_METRICS && !ADAPTIVE_RESET_CYCLES
  static struct etimer metrics_timer;
#endif

  PROCESS_BEGIN();

  /* Initialize node ID */
  my_node_id = simMoteID;
  if (my_node_id == 0) {
    my_node_id = 1;
  }

  LOG_INFO("Adaptive-PraSLE node %u starting\n", my_node_id);
  LOG_INFO("Config: K=%d, T=%.1fs, topology=%d, size=%d\n",
           K_ROUNDS, T_SECONDS, NETWORK_TOPOLOGY, NETWORK_SIZE);
  LOG_INFO("Weights: energy=%d%%, link=%d%%, conn=%d%%, cpu=%d%%\n",
           ENERGY_WEIGHT, LINK_QUALITY_WEIGHT, CONNECTIVITY_WEIGHT, CPU_WEIGHT);
  LOG_INFO("Features: energy=%d, link=%d, rotation=%d, adaptive_to=%d, backup=%d\n",
           ADAPTIVE_ENERGY_AWARE, ADAPTIVE_LINK_QUALITY_AWARE,
           ADAPTIVE_LEADER_ROTATION, ADAPTIVE_TIMEOUTS, ADAPTIVE_BACKUP_LIST);

  /* Initialize algorithm state */
  round_counter = K_ROUNDS + 1;
  algorithm_state = STATE_ELECTION;

  /* Initialize neighbors */
  get_list_of_neighbors();

#if ADAPTIVE_BACKUP_LIST
  init_backup_list();
#endif

  /* Initialize election values */
  mini = N_MAX + 1;
  temp_mini = get_ranking_value();
  leaderi = my_node_id;
  temp_leaderi = my_node_id;

  LOG_INFO("Initial: score=%u, mini=%u, leaderi=%u\n",
           temp_mini, mini, leaderi);

  /* Initialize UDP connection for IPv6 communication */
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);
  LOG_INFO("UDP connection registered on port %d\n", UDP_PORT);

#if ENABLE_METRICS
  metrics_init();
  metrics_track_state(ELECTION_STATE_ELECTION);
  metrics_track_election_start();
  metrics_output_header();
#endif

  /* Random startup delay - uses STARTUP_DELAY_MAX from config (0.5s in FAST_MODE) */
  etimer_set(&round_timer, (random_rand() % STARTUP_DELAY_MAX) + (STARTUP_DELAY_MAX / 2));
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&round_timer));

  start_time = clock_time();

  /*=========================================================================*/
  /* ELECTION PHASE (with Reset-Cycle for Fast Recovery)                     */
  /*=========================================================================*/

#if ADAPTIVE_RESET_CYCLES
  /* Continuous election mode for fast recovery (like PraSLE unreliable mode) */
  while (1) {
#endif

  while (round_counter > 0) {
    round_counter--;

    LOG_INFO("========== Round %d ==========\n", round_counter);

    /* Receive phase with adaptive timeout */
    clock_time_t timeout = get_adaptive_timeout();
    etimer_set(&recv_timer, timeout);

    LOG_DBG("Receive phase: %lu ms\n", (unsigned long)CLOCK_TO_MS(timeout));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&recv_timer));

    /* Update phase */
    if (is_better(temp_mini, temp_leaderi, mini, leaderi)) {
      mini = temp_mini;
      leaderi = temp_leaderi;

      LOG_INFO("Updated: min=%u, leader=%u\n", mini, leaderi);

      send_message_to_neighbors();
    }

#if ADAPTIVE_BACKUP_LIST
    /* Update backup list periodically */
    update_backup_list();
#endif

    /* Small inter-round delay - reduced for faster convergence */
    if (round_counter > 0) {
      etimer_set(&round_timer, CLOCK_SECOND / 10);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&round_timer));
    }
  }

  /* Check convergence after each election cycle */
  check_convergence();

#if ADAPTIVE_RESET_CYCLES
  /*
   * RESET-CYCLE RECOVERY (from PraSLE):
   * Every RESET_CYCLE_COUNT cycles, clear stale values to allow
   * re-convergence to a live leader if the current one crashed.
   */
  election_cycle++;

  if (election_cycle % RESET_CYCLE_COUNT == 0) {
    uint16_t old_leader = leaderi;

    /* Reset election state */
    mini = N_MAX + 1;
    leaderi = my_node_id;
    temp_mini = get_ranking_value();
    temp_leaderi = my_node_id;
    election_converged = false;

    LOG_INFO("Reset-cycle %u: Clearing values for re-election\n", election_cycle);

    if (old_leader != my_node_id) {
      LOG_INFO("Leader changed: %u -> %u (reset)\n", old_leader, leaderi);
    }
  }

  /* Restart election rounds */
  round_counter = K_ROUNDS + 1;
  LOG_INFO("Starting election cycle %u\n", election_cycle);

#if ENABLE_METRICS
  /* Output metrics at the end of each election cycle */
  metrics_output();
#endif

  } /* End of continuous election loop */
#endif

  /*=========================================================================*/
  /* POST-CONVERGENCE: MAINTENANCE MODE                                      */
  /* (Only reached when ADAPTIVE_RESET_CYCLES is disabled)                   */
  /*=========================================================================*/

#if !ADAPTIVE_RESET_CYCLES
  LOG_INFO("========== Election Complete ==========\n");

#if ADAPTIVE_LEADER_ROTATION
  leadership_start = clock_time();
#endif

  /* Set up maintenance timers */
#if ADAPTIVE_BACKUP_LIST
  etimer_set(&heartbeat_timer, BACKUP_HEARTBEAT_INTERVAL);
#endif
  etimer_set(&energy_timer, ENERGY_UPDATE_INTERVAL);

#if ENABLE_METRICS
  etimer_set(&metrics_timer, METRICS_OUTPUT_INTERVAL);
#endif

  /* Main maintenance loop */
  while (1) {
    PROCESS_WAIT_EVENT();

    if (ev != PROCESS_EVENT_TIMER) continue;

#if ADAPTIVE_BACKUP_LIST
    /* Leader heartbeat */
    if (data == &heartbeat_timer) {
      send_leader_heartbeat();
      check_neighbor_failures();
      etimer_reset(&heartbeat_timer);
    }
#endif

    /* Energy monitoring and rotation check */
    if (data == &energy_timer) {
      uint8_t energy = get_energy_level();
      LOG_DBG("Energy: %u%%\n", energy);

#if ADAPTIVE_LEADER_ROTATION
      /* Check if handover needed */
      if (should_initiate_handover()) {
        uint16_t successor = select_handover_successor();
        if (successor != 0) {
          initiate_handover(successor);
          etimer_set(&handover_timer, HANDOVER_ACK_TIMEOUT);
        } else {
          LOG_WARN("No suitable successor for handover\n");
        }
      }
#endif

      etimer_reset(&energy_timer);
    }

#if ADAPTIVE_LEADER_ROTATION
    /* Handover timeout handling */
    if (data == &handover_timer && rotation_state == ROTATION_WAITING_ACK) {
      handover_retries++;
      if (handover_retries >= MAX_HANDOVER_RETRIES) {
        LOG_WARN("Handover failed after %d retries\n", handover_retries);
        rotation_state = ROTATION_IDLE;

        /* Try next candidate from backup list */
#if ADAPTIVE_BACKUP_LIST
        for (uint8_t i = 0; i < backup_count; i++) {
          if (backup_list[i].node_id != handover_target) {
            adaptive_neighbor_info_t *nbr = find_neighbor(backup_list[i].node_id);
            if (nbr != NULL && is_neighbor_acceptable(nbr)) {
              initiate_handover(backup_list[i].node_id);
              etimer_set(&handover_timer, HANDOVER_ACK_TIMEOUT);
              break;
            }
          }
        }
#endif
      } else {
        /* Retry */
        LOG_INFO("Handover retry %d/%d\n", handover_retries, MAX_HANDOVER_RETRIES);
        initiate_handover(handover_target);
        etimer_set(&handover_timer, HANDOVER_ACK_TIMEOUT);
      }
    }
#endif

#if ENABLE_METRICS
    /* Metrics output */
    if (data == &metrics_timer) {
      metrics_output();
      etimer_reset(&metrics_timer);
    }
#endif
  }
#endif /* !ADAPTIVE_RESET_CYCLES */

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
