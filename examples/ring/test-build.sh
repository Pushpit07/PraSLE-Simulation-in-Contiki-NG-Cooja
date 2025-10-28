#!/bin/bash

# Test build script for Ring Leader Election Algorithm
# This script tests the compilation of the ring leader election example

echo "Testing Ring Leader Election Algorithm build..."
echo "============================================="

# Clean previous builds
echo "Cleaning previous builds..."
make clean

# Test compilation for cooja target
echo "Compiling for Cooja target..."
if make TARGET=cooja; then
    echo "✓ Cooja compilation successful"
else
    echo "✗ Cooja compilation failed"
    exit 1
fi

# Clean up
make clean

echo "============================================="
echo "Build test completed successfully!"
echo ""
echo "To run the simulation:"
echo "1. cd examples/ring"
echo "2. make TARGET=cooja"
echo "3. ../../tools/cooja/gradlew run --project-dir ../../tools/cooja"
echo "4. Load ring-cooja.csc in Cooja"
echo "5. Start the simulation"
