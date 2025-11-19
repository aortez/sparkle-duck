#!/bin/bash
# Test script for swap channel logging

echo "=== Testing Swap Channel Logging ==="
echo ""

# Test 1: Run server with swap channel at trace level
echo "Test 1: Server with swap:trace,*:error"
echo "Starting server with swap channel at trace level..."
timeout 5 ./build/bin/sparkle-duck-server -C "swap:trace,*:error" 2>&1 | grep -E "^\[.*\] \[swap\]" &
SERVER_PID=$!
sleep 1

# Connect and run a scenario that will trigger swaps
echo "Running dam break scenario to trigger swap mechanics..."
./build/bin/cli ws://localhost:8080 scenario_config_set '{"scenario": "dam_break"}'
./build/bin/cli ws://localhost:8080 sim_run '{"timestep": 0.016, "max_steps": 50}'
sleep 2
kill $SERVER_PID 2>/dev/null

echo ""
echo "Test 2: Server with all channels off except swap"
echo "Starting server with only swap channel enabled..."
timeout 5 ./build/bin/sparkle-duck-server -C "*:off,swap:info" 2>&1 | grep -E "^\[.*\]" &
SERVER_PID=$!
sleep 1

# Run simulation
./build/bin/cli ws://localhost:8080 scenario_config_set '{"scenario": "falling_dirt"}'
./build/bin/cli ws://localhost:8080 sim_run '{"timestep": 0.016, "max_steps": 30}'
sleep 2
kill $SERVER_PID 2>/dev/null

echo ""
echo "Test 3: UI with swap channel enabled"
echo "You can test the UI manually with:"
echo "./build/bin/sparkle-duck-ui -C 'swap:trace,physics:debug,*:warn'"
echo ""
echo "=== Tests Complete ==="