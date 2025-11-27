#!/bin/bash
# Test script for elasticity control

echo "Elasticity Control Test"
echo "======================"
echo "This test drops metal balls and lets you adjust elasticity to see bounce changes."
echo ""
echo "1. Start the server and UI"
echo "2. Use the Elasticity slider in the Physics Controls"
echo "3. Watch how metal bounces differently at different elasticity settings"
echo ""
echo "Starting server..."

# Start server in background
./build/bin/sparkle-duck-server -p 8080 &
SERVER_PID=$!

sleep 1

echo "Starting UI with metal drop scenario..."
echo ""
echo "Try these elasticity settings:"
echo "  - 100% (default): Full bounce"
echo "  - 50%: Half bounce"
echo "  - 0%: No bounce at all"
echo ""

# Start UI connected to server
./build/bin/sparkle-duck-ui -b wayland --connect localhost:8080

# Clean up server when UI exits
kill $SERVER_PID 2>/dev/null

echo "Test complete!"