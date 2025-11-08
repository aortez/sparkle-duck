#!/bin/bash
set -e

# Check if server or UI are already running.
if pgrep -f "sparkle-duck-server" > /dev/null; then
    echo "Error: sparkle-duck-server is already running"
    echo "Kill it with: pkill -f sparkle-duck-server"
    exit 1
fi

if pgrep -f "sparkle-duck-ui" > /dev/null; then
    echo "Error: sparkle-duck-ui is already running"
    echo "Kill it with: pkill -f sparkle-duck-ui"
    exit 1
fi

# Build debug version (server and UI only, skip broken tests).
echo "Building debug version..."
if ! make -C build sparkle-duck-server sparkle-duck-ui -j12; then
    echo "Build failed!"
    exit 1
fi

echo "Build succeeded!"
echo ""

# Start server in background.
echo "Starting DSSM server on port 8080..."
./build/bin/sparkle-duck-server &
SERVER_PID=$!

# Give server time to start.
sleep 1

# Check if server started successfully.
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "Error: Server failed to start"
    exit 1
fi

echo "Server started (PID: $SERVER_PID)"
echo "Starting UI..."
echo ""

# Start UI in foreground (blocks until user quits).
./build/bin/sparkle-duck-ui

# UI has exited - shut down server gracefully.
echo ""
echo "UI exited, shutting down server..."

if ./build/bin/cli ws://localhost:8080 exit 2>/dev/null; then
    echo "Server shutdown command sent"
    # Wait for server to exit gracefully.
    sleep 1
else
    echo "Server already stopped or CLI failed, force killing..."
fi

# Ensure server is dead.
if kill -0 $SERVER_PID 2>/dev/null; then
    echo "Force killing server (PID: $SERVER_PID)"
    kill $SERVER_PID 2>/dev/null || true
fi

echo "Cleanup complete"
