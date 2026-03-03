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
 * Ring algorithm (CURRENT_ALGORITHM == 2): Uses IPv6 with RPL routing
 * PraSLE (CURRENT_ALGORITHM == 3): Uses IPv6 with RPL routing (paper specifies UDP/IP)
 * Adaptive-PraSLE (CURRENT_ALGORITHM == 4): Uses IPv6 with RPL routing (same as PraSLE for fair comparison)
 *
 * All algorithms now use IPv6/UDP for consistency and fair comparison.
 */
#define NETSTACK_CONF_WITH_IPV6 1
#define UIP_CONF_ROUTER 1

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
/* Network Configuration - All algorithms use IPv6/UDP */
/*---------------------------------------------------------------------------*/

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

/* PraSLE parameters are defined in prasle-config.h */
/* Adaptive-PraSLE parameters are defined in adaptive-prasle-config.h */

#endif /* PROJECT_CONF_H_ */
