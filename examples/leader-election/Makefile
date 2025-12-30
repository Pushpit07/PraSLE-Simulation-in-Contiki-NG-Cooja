# ============================================================
# Leader Election Algorithm Framework - Master Makefile
# ============================================================
#
# Usage:
#   make ALGORITHM=bully         # Build Bully algorithm
#   make ALGORITHM=ring          # Build Ring algorithm
#   make ALGORITHM=prasle        # Build PraSLE algorithm
#   make ALGORITHM=adaptive-prasle # Build Customized PraSLE
#   make clean                   # Clean current algorithm build
#   make clean-all               # Clean all algorithm builds
#   make all-algorithms          # Build all algorithms
#
# Options:
#   TARGET=cooja                 # Target platform (default: cooja)
#   ENABLE_METRICS=1             # Enable metrics collection (default: 1)
#   MAX_NODES=100                # Maximum number of nodes (default: 100)
#   FAST_MODE=1                  # Enable fast timing mode (reduced timeouts)
#
# ============================================================

# Default algorithm
ALGORITHM ?= bully

# Valid algorithms
VALID_ALGORITHMS := bully ring prasle adaptive-prasle

# Validate algorithm selection
ifeq ($(filter $(ALGORITHM),$(VALID_ALGORITHMS)),)
  $(error Invalid ALGORITHM='$(ALGORITHM)'. Must be one of: $(VALID_ALGORITHMS))
endif

# ============================================================
# Project Configuration
# ============================================================

# Project name based on algorithm
CONTIKI_PROJECT = $(ALGORITHM)-node

# Contiki-NG root (relative from examples/leader-election/)
CONTIKI = ../..

# Target platform (default: Cooja simulator)
TARGET ?= cooja

# ============================================================
# Algorithm-Specific Configuration
# ============================================================

# Algorithm identifier for conditional compilation
ifeq ($(ALGORITHM),bully)
  ALGORITHM_ID = 1
  MAKE_NET = MAKE_NET_IPV6
  MAKE_ROUTING = MAKE_ROUTING_RPL_LITE
else ifeq ($(ALGORITHM),ring)
  ALGORITHM_ID = 2
  MAKE_NET = MAKE_NET_NULLNET
  MAKE_ROUTING = MAKE_ROUTING_NULLROUTING
else ifeq ($(ALGORITHM),prasle)
  ALGORITHM_ID = 3
  MAKE_NET = MAKE_NET_NULLNET
  MAKE_ROUTING = MAKE_ROUTING_NULLROUTING
else ifeq ($(ALGORITHM),adaptive-prasle)
  ALGORITHM_ID = 4
  MAKE_NET = MAKE_NET_NULLNET
  MAKE_ROUTING = MAKE_ROUTING_NULLROUTING
endif

# MAC layer (CSMA for all)
MAKE_MAC = MAKE_MAC_CSMA

# ============================================================
# Compiler Flags
# ============================================================

# Algorithm identifier
CFLAGS += -DCURRENT_ALGORITHM=$(ALGORITHM_ID)

# Metrics configuration
ENABLE_METRICS ?= 1
CFLAGS += -DENABLE_METRICS=$(ENABLE_METRICS)

# Maximum nodes
MAX_NODES ?= 100
CFLAGS += -DMAX_NODES=$(MAX_NODES)

# Fast timing mode (reduced timeouts for testing/simulation)
FAST_MODE ?= 0
ifeq ($(FAST_MODE),1)
  CFLAGS += -DBULLY_FAST_MODE=1
endif

# Include paths
CFLAGS += -I$(CURDIR)/common
CFLAGS += -I$(CURDIR)/algorithms/$(ALGORITHM)

# ============================================================
# Source Files
# ============================================================

# Common source files
PROJECT_SOURCEFILES += election-metrics.c

# ============================================================
# VPATH for finding sources
# ============================================================
VPATH += algorithms/$(ALGORITHM)
VPATH += common

# Source directories for Contiki-NG
SOURCEDIRS += algorithms/$(ALGORITHM)
SOURCEDIRS += common

# ============================================================
# Build Targets
# ============================================================

all: $(CONTIKI_PROJECT)

# Build all algorithms
.PHONY: all-algorithms
all-algorithms:
	@echo "Building all algorithms..."
	$(MAKE) ALGORITHM=bully clean
	$(MAKE) ALGORITHM=bully
	$(MAKE) ALGORITHM=ring clean
	$(MAKE) ALGORITHM=ring
	$(MAKE) ALGORITHM=prasle clean
	$(MAKE) ALGORITHM=prasle
	$(MAKE) ALGORITHM=adaptive-prasle clean
	$(MAKE) ALGORITHM=adaptive-prasle
	@echo "All algorithms built successfully!"

# Clean all algorithm builds
.PHONY: clean-all
clean-all:
	@echo "Cleaning all algorithm builds..."
	-$(MAKE) ALGORITHM=bully TARGET=cooja clean 2>/dev/null || true
	-$(MAKE) ALGORITHM=ring TARGET=cooja clean 2>/dev/null || true
	-$(MAKE) ALGORITHM=prasle TARGET=cooja clean 2>/dev/null || true
	-$(MAKE) ALGORITHM=adaptive-prasle TARGET=cooja clean 2>/dev/null || true
	@echo "All builds cleaned!"

# Per-algorithm clean targets
.PHONY: clean-bully clean-ring clean-prasle clean-adaptive-prasle
clean-bully:
	$(MAKE) ALGORITHM=bully TARGET=cooja clean
clean-ring:
	$(MAKE) ALGORITHM=ring TARGET=cooja clean
clean-prasle:
	$(MAKE) ALGORITHM=prasle TARGET=cooja clean
clean-adaptive-prasle:
	$(MAKE) ALGORITHM=adaptive-prasle TARGET=cooja clean

# Help target
.PHONY: help
help:
	@echo "Leader Election Algorithm Framework"
	@echo "===================================="
	@echo ""
	@echo "Build a specific algorithm:"
	@echo "  make ALGORITHM=bully"
	@echo "  make ALGORITHM=ring"
	@echo "  make ALGORITHM=prasle"
	@echo "  make ALGORITHM=adaptive-prasle"
	@echo ""
	@echo "Build all algorithms:"
	@echo "  make all-algorithms"
	@echo ""
	@echo "Clean builds:"
	@echo "  make clean              # Clean current algorithm"
	@echo "  make clean-all          # Clean all algorithms"
	@echo ""
	@echo "Options:"
	@echo "  TARGET=cooja            # Target platform"
	@echo "  ENABLE_METRICS=1        # Enable metrics (0 or 1)"
	@echo "  MAX_NODES=100           # Maximum number of nodes"
	@echo "  FAST_MODE=1             # Fast timing mode (reduced timeouts)"
	@echo ""
	@echo "Current configuration:"
	@echo "  ALGORITHM=$(ALGORITHM)"
	@echo "  TARGET=$(TARGET)"
	@echo "  ENABLE_METRICS=$(ENABLE_METRICS)"
	@echo "  MAX_NODES=$(MAX_NODES)"
	@echo "  FAST_MODE=$(FAST_MODE)"

# ============================================================
# Include Contiki-NG Build System
# ============================================================
include $(CONTIKI)/Makefile.include
