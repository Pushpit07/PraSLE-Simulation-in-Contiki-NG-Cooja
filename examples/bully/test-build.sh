#!/bin/bash

# Test script to verify the bully algorithm implementation compiles correctly
# Usage: ./test-build.sh

echo "Testing Bully Leader Election Algorithm Build..."
echo "=============================================="

# Navigate to the bully directory
cd "$(dirname "$0")"

echo "Current directory: $(pwd)"
echo "Files in directory:"
ls -la

echo ""
echo "Testing compilation for Cooja target..."
make clean
make bully-node.cooja TARGET=cooja

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ BUILD SUCCESSFUL!"
    echo "The bully algorithm is ready for simulation."
    echo ""
    echo "Next steps:"
    echo "1. Start Cooja: ./tools/cooja/gradlew run (from Contiki-NG root)"
    echo "2. Open simulation: File → Open Simulation → bully-cooja.csc"
    echo "3. Start simulation and observe the election process"
    echo ""
    echo "Generated files:"
    ls -la *.cooja 2>/dev/null || echo "No .cooja files found"
else
    echo ""
    echo "❌ BUILD FAILED!"
    echo "Please check the compilation errors above."
    echo "Common issues:"
    echo "- Ensure you're in the Contiki-NG directory"
    echo "- Check that CONTIKI path is correct in Makefile"
    echo "- Verify all dependencies are installed"
fi