CONTIKI_PROJECT = bully-node
all: $(CONTIKI_PROJECT)

CONTIKI = ../..

# Target platform: cooja for simulation
TARGET = cooja

# Enable IPv6 with RPL routing for multi-hop communication
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_LITE
MAKE_MAC = MAKE_MAC_CSMA

include $(CONTIKI)/Makefile.include