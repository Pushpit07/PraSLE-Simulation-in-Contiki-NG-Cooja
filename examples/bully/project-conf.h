/*
 * Copyright (c) 2024, TU Dresden
 * All rights reserved.
 *
 * Project configuration for Bully Leader Election Algorithm
 */

#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Enable nullnet for simple message passing */
#define NULLNET_CONF_SIZE 128

/* Completely disable IPv6 stack */
#define UIP_CONF_IPV6 0
#define NETSTACK_CONF_WITH_IPV6 0

/* Disable all routing */
#define UIP_CONF_ROUTER 0
#define NETSTACK_CONF_WITH_ROUTING 0

/* Disable TCP/IP */
#define UIP_CONF_TCP 0

/* Enable logging */
#define LOG_CONF_LEVEL_MAIN LOG_LEVEL_INFO

/* Energy configuration */
#define ENERGEST_CONF_ON 1

#endif /* PROJECT_CONF_H_ */