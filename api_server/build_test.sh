#!/bin/bash
# Build and run tests for api_server

set -e

echo "Building api_server with tests..."

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure with cmake
cmake ..

# Build everything (including tests)
cmake --build .

# Run tests
echo ""
echo "Running tests..."
./api_server_tests

# Also allow running tests via make target
# make run_tests
