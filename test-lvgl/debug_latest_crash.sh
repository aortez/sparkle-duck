#!/bin/bash

# This script finds the most recent core dump for the sparkle-duck executable
# and runs gdb on it to generate a stack trace.

set -eu

# --- Configuration ---
EXECUTABLE_PATH="/home/oldman/workspace/sparkle-duck/test-lvgl/build/bin/sparkle-duck"

# --- Script Body ---

# 1. Check if the executable exists
if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "Error: Executable not found at $EXECUTABLE_PATH"
    echo "Please build the project first."
    exit 1
fi

# 2. Check if coredumpctl is available
if ! command -v coredumpctl &> /dev/null; then
    echo "Error: coredumpctl not found. Is systemd-coredump installed?"
    exit 1
fi

# 3. List recent core dumps for sparkle-duck
echo "===== Recent core dumps for sparkle-duck ====="
coredumpctl list "$EXECUTABLE_PATH" 2>/dev/null | tail -10 || {
    echo "No core dumps found for $EXECUTABLE_PATH"
    echo "You may need to enable core dumps with: ulimit -c unlimited"
    exit 1
}

echo ""
echo "===== Debugging most recent core dump ====="

# 4. Debug the most recent core dump with gdb
# Using --debugger-arguments for cleaner syntax
coredumpctl debug "$EXECUTABLE_PATH" --debugger-arguments="--batch -ex 'set pagination off' -ex 'bt full' -ex 'frame 0' -ex 'info registers' -ex 'info locals' -ex 'thread apply all bt'"

echo "----------------------------------------"
echo "Script finished." 
