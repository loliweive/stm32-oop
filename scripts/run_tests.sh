#!/bin/bash
# Build and run all host-side unit tests.
# Usage: ./scripts/run_tests.sh

set -e

echo "=== Building tests ==="
cmake -B build/test -DTARGET=host
cmake --build build/test

echo ""
echo "=== Running tests ==="
cd build/test && ctest --output-on-failure
