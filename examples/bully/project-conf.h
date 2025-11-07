/*
 * Project configuration for Bully Leader Election Algorithm
 */

#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Enable IPv6 with RPL routing for multi-hop communication */
#define NETSTACK_CONF_WITH_IPV6 1
#define UIP_CONF_ROUTER 1

/* TCP not needed for our UDP-based protocol */
#define UIP_CONF_TCP 0

/* Optional: cap app payload size */
#define PACKETBUF_CONF_SIZE 128

/* Logging */
#define LOG_CONF_LEVEL_MAIN LOG_LEVEL_INFO

#endif /* PROJECT_CONF_H_ */