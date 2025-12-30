/*
 * Leader Election Algorithm Framework - Project Configuration
 */

#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/*---------------------------------------------------------------------------*/
/* Network Stack Configuration - Algorithm-Specific */
/*---------------------------------------------------------------------------*/
/*
 * Bully algorithm (CURRENT_ALGORITHM == 1): Uses IPv6 with RPL routing
 * Ring, PraSLE, PraSLE-Custom (2, 3, 4): Use NullNet (no IPv6)
 */
#if CURRENT_ALGORITHM == 1  /* Bully uses IPv6 */
#define NETSTACK_CONF_WITH_IPV6 1
#define UIP_CONF_ROUTER 1
#else /* Ring, PraSLE, PraSLE-Custom use nullnet - no IPv6 */
#define NETSTACK_CONF_WITH_IPV6 0
#endif

/*---------------------------------------------------------------------------*/
/* Logging Configuration */
/*---------------------------------------------------------------------------*/
#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_WARN
/* Logging */
#define LOG_CONF_LEVEL_MAIN                        LOG_LEVEL_INFO

/*---------------------------------------------------------------------------*/
/* Network Configuration - Algorithm-Specific */
/*---------------------------------------------------------------------------*/

#if CURRENT_ALGORITHM == 1  /* Bully uses IPv6 */

/* UDP Configuration */
#define UIP_CONF_UDP                               1
#define UIP_CONF_UDP_CONNS                         10

/* Disable TCP to save memory */
#define UIP_CONF_TCP                               0

/* RPL Configuration */
#define RPL_CONF_MOP                               RPL_MOP_NON_STORING
#define RPL_CONF_SUPPORTED_OFS                     {&rpl_mrhof}
#define RPL_CONF_OF_OCP                            RPL_OCP_MRHOF

/* Reduce RPL traffic for faster convergence testing */
#define RPL_CONF_DIO_INTERVAL_MIN                  12
#define RPL_CONF_DIO_INTERVAL_DOUBLINGS            8
#define RPL_CONF_DIO_REDUNDANCY                    10

#else /* Ring, PraSLE, PraSLE-Custom use nullnet */

/* No UIP for nullnet-based algorithms */
#define UIP_CONF_UDP                               0
#define UIP_CONF_TCP                               0

#endif

/*---------------------------------------------------------------------------*/
/* Buffer Configuration */
/*---------------------------------------------------------------------------*/
#define QUEUEBUF_CONF_NUM                          8
#define NBR_TABLE_CONF_MAX_NEIGHBORS               20

/*---------------------------------------------------------------------------*/
/* Energy Estimation (optional) */
/*---------------------------------------------------------------------------*/
#define ENERGEST_CONF_ON                           0

/*---------------------------------------------------------------------------*/
/* Metrics Configuration */
/*---------------------------------------------------------------------------*/
#ifndef ENABLE_METRICS
#define ENABLE_METRICS                             1
#endif

#ifndef METRICS_OUTPUT_INTERVAL
#define METRICS_OUTPUT_INTERVAL                    (10 * CLOCK_SECOND)
#endif

/*---------------------------------------------------------------------------*/
/* Algorithm-Specific Configuration */
/*---------------------------------------------------------------------------*/

/* Ring Algorithm Configuration */
#ifndef RING_SIZE
#define RING_SIZE                                  10
#endif

/* PraSLE Algorithm Configuration */
#ifndef PRASLE_K_ROUNDS
#define PRASLE_K_ROUNDS                            10
#endif

#ifndef PRASLE_T_SECONDS
#define PRASLE_T_SECONDS                           1.0
#endif

#ifndef PRASLE_NETWORK_SIZE
#define PRASLE_NETWORK_SIZE                        10
#endif

#endif /* PROJECT_CONF_H_ */
