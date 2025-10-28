#!/bin/bash

# PraSLE Leader Election - Build and Test Script
# Copyright (c) 2024, TU Dresden

echo "======================================"
echo "PraSLE Leader Election - Build Script"
echo "======================================"
echo ""

# Check if we're in the right directory
if [ ! -f "prasle-node.c" ]; then
    echo "Error: prasle-node.c not found!"
    echo "Please run this script from the examples/prasle directory"
    exit 1
fi

# Clean previous builds
echo "[1/3] Cleaning previous builds..."
make clean TARGET=cooja
if [ $? -ne 0 ]; then
    echo "Warning: Clean failed, but continuing..."
fi
echo ""

# Build for Cooja
echo "[2/3] Building prasle-node for Cooja..."
make TARGET=cooja
if [ $? -ne 0 ]; then
    echo "Error: Build failed!"
    exit 1
fi
echo ""

# Display success message
echo "[3/3] Build successful!"
echo ""
echo "======================================"
echo "Build Summary"
echo "======================================"
echo "Target: Cooja"
echo "Executable: build/cooja/prasle-node.cooja"
echo ""
echo "Available simulation files:"
echo "  - prasle-cooja-ring.csc  (6 nodes, ring topology)"
echo "  - prasle-cooja-mesh.csc  (9 nodes, 3x3 mesh topology)"
echo ""
echo "To run simulations:"
echo "1. Start Cooja:"
echo "   ../../tools/cooja/gradlew run --project-dir ../../tools/cooja"
echo ""
echo "2. In Cooja, go to: File → Open Simulation"
echo "3. Select one of the .csc files above"
echo "4. Click Start to begin simulation"
echo ""
echo "To change topology:"
echo "1. Edit prasle-node.c"
echo "2. Change #define NETWORK_TOPOLOGY to desired topology:"
echo "   - TOPOLOGY_RING  : Ring topology"
echo "   - TOPOLOGY_LINE  : Line topology"
echo "   - TOPOLOGY_MESH  : 2D Mesh/Grid"
echo "   - TOPOLOGY_CLIQUE: Complete graph"
echo "3. Rebuild with: make clean && make TARGET=cooja"
echo ""
echo "======================================"
