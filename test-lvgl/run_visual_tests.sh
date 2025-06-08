#!/bin/bash

# Script to run sparkle-duck tests with visual mode enabled

echo "Running sparkle-duck tests with visual mode enabled..."
echo "Set SPARKLE_DUCK_VISUAL_TESTS=1 to enable visual demonstrations"
echo ""

# Enable visual mode
export SPARKLE_DUCK_VISUAL_TESTS=1

# Check if the tests binary exists
if [ ! -f "bin/sparkle-duck-tests" ]; then
    echo "Error: Test binary not found. Please run 'make' first."
    exit 1
fi

# Check if we're in a graphical environment
if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
    echo "Warning: No display detected. Visual tests may not work."
    echo "Make sure you're running in a graphical environment."
fi

# Run the tests
echo "Starting visual tests..."
echo "You should see a window for each test that uses a World."
echo ""

# Run specific test if provided, otherwise run all tests
if [ $# -eq 0 ]; then
    ./bin/sparkle-duck-tests | ts %.T
else
    ./bin/sparkle-duck-tests --gtest_filter="$1" | ts %.T
fi

echo ""
echo "Visual tests completed." 
