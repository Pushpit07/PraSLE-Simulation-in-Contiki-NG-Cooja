/**
 * \file   prasle-node.c
 * \brief  PraSLE (Practical Self-Stabilizing Leader Election) Algorithm
 * \author Pushpit Bhardwaj
 *
 * Based on "A Practical Self-Stabilizing Leader Election for
 * Networks of Resource-Constrained IoT Devices" (Conard & Ebnenasir, 2021)
 *
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
#include "prasle-config.h"

/*---------------------------------------------------------------------------*/
/* LOGGING CONFIGURATION */
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "PraSLE"
#define LOG_LEVEL LOG_LEVEL_INFO

/*---------------------------------------------------------------------------*/
/* UDP CONFIGURATION */
/*---------------------------------------------------------------------------*/
/**
 * UDP_PORT: Port number for PraSLE algorithm messages
 * - All nodes listen on this port for election messages
 * - Messages are sent to IPv6 multicast address for broadcast
 */
#define UDP_PORT PRASLE_UDP_PORT

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

/**
 * UDP connection for sending/receiving PraSLE messages
 * - Uses IPv6 link-local multicast (ff02::1) for broadcast-style communication
 * - Link-local multicast reaches all nodes in the network
 */
static struct simple_udp_connection udp_conn;

/*---------------------------------------------------------------------------*/
/* TIMER MANAGEMENT */
/*---------------------------------------------------------------------------*/
/* Note: All timers are declared locally in PROCESS_THREAD */

/*---------------------------------------------------------------------------*/
/* CONTIKI-NG PROCESS DEFINITION */
/*---------------------------------------------------------------------------*/
PROCESS(prasle_process, "PraSLE Leader Election");
AUTOSTART_PROCESSES(&prasle_process);

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES */
/*---------------------------------------------------------------------------*/
static uint16_t get_ranking_value(void);
static void get_list_of_neighbors(void);
static void send_message_to_neighbors(void);
static void handle_message(const uint8_t *data, uint16_t len);
static bool is_better(uint16_t m1, uint16_t l1, uint16_t m2, uint16_t l2);
static bool is_logical_neighbor(uint16_t node_id);

/*---------------------------------------------------------------------------*/
static uint16_t
get_ranking_value(void)
{
  /* Use node ID as ranking value - can be changed to battery level, etc. */
  return my_node_id;
}

/*---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------*/
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

  /* Send via UDP to link-local all-nodes multicast address */
  /* Link-local multicast (ff02::1) reaches all nodes in the network */
  uip_ipaddr_t dest_addr;
  uip_create_linklocal_allnodes_mcast(&dest_addr);
  simple_udp_sendto(&udp_conn, &msg, sizeof(msg), &dest_addr);
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Handle received PraSLE message
 *
 * \param data  Pointer to received message data
 * \param len   Length of received data
 *
 * This function handles incoming PraSLE messages. It:
 * 1. Validates message size
 * 2. Filters self-messages (broadcast echo)
 * 3. CRITICAL: Filters non-neighbor messages (topology enforcement)
 * 4. Updates temp values if received pair is lexicographically smaller
 */
static void
handle_message(const uint8_t *data, uint16_t len)
{
  if (len != sizeof(prasle_msg_t)) {
    LOG_WARN("Received message with wrong size\n");
    return;
  }

  prasle_msg_t *msg = (prasle_msg_t *)data;
  uint16_t sender_id = msg->sender_id;

  /* Filter self-messages (broadcast echo) */
  if (sender_id == my_node_id) {
    return;
  }

  /*
   * CRITICAL: Filter messages from non-neighbors (topology enforcement)
   *
   * With NullNet, broadcast only reaches physical neighbors (radio range).
   * With IPv6/UDP multicast, ALL nodes in the network receive the message.
   *
   * PraSLE operates on logical topologies (ring, line, mesh, clique).
   * Each node has specific logical neighbors based on the topology.
   * We must filter to only process messages from logical neighbors.
   *
   * Paper requirement: "recv (minj, leaderj) from pj in neighborsi"
   */
  if (!is_logical_neighbor(sender_id)) {
    return;  /* Ignore messages from non-neighbors */
  }

  uint16_t minj = msg->min_value;
  uint16_t leaderj = msg->leader_id;

  #if ENABLE_METRICS
    metrics_track_message_recv(1, sizeof(prasle_msg_t));
  #endif

  LOG_INFO("Round %d: Received from neighbor %u: (min=%u, leader=%u)\n",
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

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(prasle_process, ev, data)
{
  static struct etimer round_timer;
  static struct etimer recv_timer;
  #if !PRASLE_UNRELIABLE_MODE
    static struct etimer maintenance_timer;
  #endif
  static uint16_t election_cycle = 0;  /* Track election cycles for unreliable mode */

  PROCESS_BEGIN();

  /*=========================================================================*/
  /* INITIALIZATION PHASE (Paper Algorithm 1, Lines 1-8) */
  /*=========================================================================*/

  /* Initialize node ID */
  my_node_id = simMoteID;
  if (my_node_id == 0) {
    my_node_id = 1;
  }

  LOG_INFO("PraSLE node %u starting\n", my_node_id);
  LOG_INFO("Parameters: K=%d rounds, T=%.1f seconds, topology=%d, size=%d\n",
           PRASLE_K_ROUNDS, PRASLE_T_SECONDS, NETWORK_TOPOLOGY, NETWORK_SIZE);
  LOG_INFO("Mode: %s network\n", PRASLE_UNRELIABLE_MODE ? "UNRELIABLE (continuous)" : "RELIABLE (terminates)");

  /* Paper Line 2: round := K + 1 */
  round_counter = PRASLE_K_ROUNDS + 1;

  /* Paper Line 3: neighborsi := getListOfNeighbors() */
  get_list_of_neighbors();

  /* Paper Line 4: mini := N + 1 (null value indicating no leader yet) */
  mini = N_MAX + 1;

  /* Paper Line 5: temp_mini := getRankingValue()
   * Using node ID as ranking value - lowest ID wins */
  temp_mini = get_ranking_value();

  /* Paper Lines 6-7: leaderi := IDi; temp_leaderi := IDi */
  leaderi = my_node_id;
  temp_leaderi = my_node_id;

  LOG_INFO("Initial values: mini=%u, temp_mini=%u, leaderi=%u, temp_leaderi=%u\n",
           mini, temp_mini, leaderi, temp_leaderi);

  /* Initialize UDP connection for IPv6 communication */
  simple_udp_register(&udp_conn, UDP_PORT, NULL, UDP_PORT, udp_rx_callback);
  LOG_INFO("UDP connection registered on port %d\n", UDP_PORT);

  #if ENABLE_METRICS
    metrics_init();
    metrics.algo.prasle.current_round = round_counter;
    metrics.algo.prasle.min_value = mini;
    metrics_track_state(ELECTION_STATE_ELECTION);
    metrics_track_election_start();
    metrics_output_header();
  #endif

  /* Random startup delay to desynchronize nodes */
#ifdef STANDARDIZED_PARAMS
  /* Use same formula as Bully/Ring for fair cross-algorithm comparison */
  etimer_set(&round_timer, random_rand() % PRASLE_STARTUP_DELAY_MAX);
#else
  /* Offset formula: guarantees minimum delay for RPL establishment */
  etimer_set(&round_timer, (random_rand() % PRASLE_STARTUP_DELAY_MAX) + (PRASLE_STARTUP_DELAY_MAX / 2));
#endif
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&round_timer));

  start_time = clock_time();

  /*=========================================================================*/
  /* MAIN ALGORITHM LOOP (Paper Algorithm 1, Lines 9-28) */
  /*=========================================================================*/
  /*
   * Paper: "begin until False" - loop forever (Line 9)
   *
   * IMPORTANT - First Round Behavior (Paper page 3):
   * "In the first round, the condition of the while loop is false
   * (because round is equal to K + 1) and each process sends its
   * mini and leaderi values to its immediate neighbors"
   */
  while (1) {
    LOG_INFO("========== Round %d (cycle %u) ==========\n",
             round_counter, election_cycle);

    #if ENABLE_METRICS
        metrics.algo.prasle.current_round = round_counter;
    #endif

    /*-----------------------------------------------------------------------*/
    /* Paper Lines 10-18: Receive phase (conditional on round < K+1) */
    /*-----------------------------------------------------------------------*/
    /*
     * Paper Line 11: while recvTimer > 0 and round < K + 1
     *
     * In the FIRST round (round == K+1), this condition is FALSE,
     * so we skip the receive phase and go directly to broadcasting.
     * This is how nodes bootstrap - they send their initial values first.
     */
    if (round_counter < PRASLE_K_ROUNDS + 1) {
      /* Set receive timer for T seconds (Paper Line 10) */
      etimer_set(&recv_timer, PRASLE_T_VALUE);

      LOG_INFO("Round %d: Receiving for %lu ms (round < K+1, so receive)\n",
               round_counter, (unsigned long)CLOCK_TO_MS(PRASLE_T_VALUE));

      /*
       * Wait T seconds for messages from neighbors.
       * Messages are handled asynchronously via input_callback -> handle_message.
       *
       * Paper: "Even if process pi receives a message from all of its
       * neighbors it will still wait the full T seconds of the round
       * duration before moving on." (Page 4)
       */
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&recv_timer));

      LOG_INFO("Round %d: Receive phase complete\n", round_counter);
    } else {
      /*
       * First round (round == K+1): Skip receive phase entirely.
       * This is critical for algorithm correctness.
       */
      LOG_INFO("Round %d: FIRST ROUND - skipping receive (round == K+1)\n",
               round_counter);
    }

    /*-----------------------------------------------------------------------*/
    /* Paper Line 19: round-- (decrement round counter) */
    /*-----------------------------------------------------------------------*/
    round_counter--;

    /*-----------------------------------------------------------------------*/
    /* Paper Lines 20-26: Update local values and send if better */
    /*-----------------------------------------------------------------------*/
    /*
     * Paper: "if (temp_mini, temp_leaderi) < (mini, leaderi)"
     *
     * This compares the best values received during this round (temp)
     * with our persistent values (mini, leaderi).
     * If temp is lexicographically smaller, adopt it.
     *
     * For RELIABLE mode: send is INSIDE this if (Paper Lines 23-25)
     * For UNRELIABLE mode: send is OUTSIDE this if (moved below)
     */
    if (is_better(temp_mini, temp_leaderi, mini, leaderi)) {
      uint16_t old_leader = leaderi;
      mini = temp_mini;
      leaderi = temp_leaderi;

      #if ENABLE_METRICS
            metrics.algo.prasle.min_value = mini;
            metrics.algo.prasle.value_updates++;
      #endif

      LOG_INFO("Round %d: UPDATED local values to (min=%u, leader=%u)\n",
               round_counter, mini, leaderi);

      /* Log leader change for experiment detection (fault tolerance recovery) */
      if (old_leader != leaderi && old_leader != my_node_id) {
        LOG_INFO("Leader changed: %u -> %u\n", old_leader, leaderi);
      }

      #if !PRASLE_UNRELIABLE_MODE
            /* Reliable mode: Send only when values changed (Paper Lines 23-25 inside if) */
            send_message_to_neighbors();
      #endif
    } else {
      LOG_INFO("Round %d: No update needed (min=%u, leader=%u)\n",
               round_counter, mini, leaderi);
    }

    /*-----------------------------------------------------------------------*/
    /* Paper Lines 23-25: Send to neighbors */
    /*-----------------------------------------------------------------------*/
    /*
     * UNRELIABLE NETWORK MODE (Paper Section III):
     * "In case of unreliable networks, Lines 23 to 25 are moved outside
     * the 'if statement' of Line 20"
     *
     * This means we ALWAYS broadcast every round, not just when values change.
     * This provides robustness against message loss.
     *
     * RELIABLE NETWORK MODE:
     * Send only happens inside the if-block when values actually changed.
     * This is the original paper algorithm (Lines 23-25 inside if).
     */
    #if PRASLE_UNRELIABLE_MODE
        /* Unreliable mode: Always broadcast every round */
        send_message_to_neighbors();
    #endif
    /* Note: For reliable mode, send_message_to_neighbors() is called
     * inside the value update block above when values change */

    /*-----------------------------------------------------------------------*/
    /* Paper Line 27: Check convergence (reliable mode only terminates) */
    /*-----------------------------------------------------------------------*/
    if (round_counter <= 0) {
      /* First time reaching convergence in this cycle */
      if (!election_converged) {
        election_converged = true;

        LOG_INFO("========================================\n");
        LOG_INFO("CONVERGED: Leader = %u (min=%u)\n", leaderi, mini);
        LOG_INFO("Convergence time: %lu ms\n",
                 (unsigned long)CLOCK_TO_MS(clock_time() - start_time));
        LOG_INFO("========================================\n");

        #if ENABLE_METRICS
                metrics.algo.prasle.rounds_completed = PRASLE_K_ROUNDS + 1;
                metrics_record_convergence();
                metrics_track_leader_change(leaderi);
                metrics_track_election_end(leaderi == my_node_id);
                if (leaderi == my_node_id) {
                  metrics_track_state(ELECTION_STATE_LEADER);
                } else {
                  metrics_track_state(ELECTION_STATE_NORMAL);
                }
        #endif
      }

      #if PRASLE_UNRELIABLE_MODE
      /*
       * UNRELIABLE MODE: Don't terminate! (Paper: "Line 27 is removed")
       *
       * Reset round_counter for continuous self-stabilizing operation.
       * This allows the algorithm to:
       * 1. Recover from transient faults (bad initialization, bit flips)
       * 2. Adapt to network changes (nodes joining/leaving)
       * 3. Maintain correctness despite message loss
       */
      election_cycle++;
      round_counter = PRASLE_K_ROUNDS + 1;

      /*
       * PERIODIC RESET FOR CRASH RECOVERY:
       * Every PRASLE_RESET_CYCLES cycles, reset mini/leaderi to initial values.
       * This allows recovery when the leader crashes:
       * - Without reset, stale (min,leader) values persist forever
       * - With reset, nodes re-converge to the new minimum (live leader)
       */
      #if PRASLE_RESET_CYCLES > 0
      if (election_cycle % PRASLE_RESET_CYCLES == 0) {
        uint16_t old_leader = leaderi;
        mini = N_MAX + 1;
        leaderi = my_node_id;
        temp_mini = get_ranking_value();
        temp_leaderi = my_node_id;
        election_converged = false;

        LOG_INFO("Periodic reset: Clearing values for re-election (cycle %u)\n",
                 election_cycle);

        /* Log leader change if applicable */
        if (old_leader != leaderi && old_leader != my_node_id) {
          LOG_INFO("Leader changed: %u -> %u (reset)\n", old_leader, leaderi);
        }
      }
      #endif

      LOG_INFO("Unreliable mode: Starting new election cycle %u\n", election_cycle);

      #if ENABLE_METRICS
      /* Output metrics at the end of each election cycle */
      metrics_output();
      #endif

      /* Continue to next iteration of the main loop */
      #else
      /*
       * RELIABLE MODE: Terminate and enter maintenance mode (Paper Line 27)
       *
       * The election is complete. Enter a maintenance loop that just
       * outputs metrics periodically.
       */
      LOG_INFO("Reliable mode: Election complete, entering maintenance\n");

      #if ENABLE_METRICS
            etimer_set(&metrics_timer, METRICS_OUTPUT_INTERVAL);
      #endif
      etimer_set(&maintenance_timer, 10 * CLOCK_SECOND);

      /* Maintenance loop - just output status periodically */
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
            LOG_INFO("Maintenance: Leader = %u (min=%u)\n", leaderi, mini);
            etimer_reset(&maintenance_timer);
          }
        }
      }
      #endif /* PRASLE_UNRELIABLE_MODE */
    }

    #if ENABLE_METRICS
        if (!election_converged) {
          metrics.algo.prasle.rounds_completed++;
        }
    #endif

    /*-----------------------------------------------------------------------*/
    /* Inter-round delay for message processing (removed in FAST_MODE) */
    /*-----------------------------------------------------------------------*/
    #ifndef PRASLE_FAST_MODE
    /* Normal mode: minimal delay for Cooja message processing */
    etimer_set(&round_timer, CLOCK_SECOND / 10);  /* 100ms, reduced from 250ms */
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&round_timer));
    #endif
    /* In FAST_MODE: no inter-round delay for maximum speed */
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
