#!/bin/bash

# This script finds the most recent core dump for the sparkle-duck executable
# and runs gdb on it to generate a stack trace.

set -e

# --- Configuration ---
EXECUTABLE_PATH="/home/oldman/workspace/sparkle-duck/test-lvgl/build/bin/sparkle-duck"
COREDUMP_DIR="/var/lib/apport/coredump"

# --- Script Body ---

# 1. Check if the executable exists
if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "Error: Executable not found at $EXECUTABLE_PATH"
    echo "Please build the project first."
    exit 1
fi

# 2. Construct the core dump file name pattern
# Apport replaces '/' with '_' in the executable path for the core dump name.
EXE_PATH_FOR_CORENAME=$(echo "$EXECUTABLE_PATH" | tr '/' '_')
CORE_PATTERN="*"

echo "Searching for core dumps with pattern: $CORE_PATTERN in $COREDUMP_DIR"

# 3. Find the most recent core dump file
LATEST_CORE=$(find "$COREDUMP_DIR" -name "$CORE_PATTERN" -printf '%T@ %p\n' 2>/dev/null | sort -n | tail -1 | cut -d' ' -f2-)

if [ -z "$LATEST_CORE" ]; then
    echo "Error: No sparkle-duck core dump found in $COREDUMP_DIR"
    echo "You can check the directory manually with: ls -l $COREDUMP_DIR"
    exit 1
fi

echo "Found latest core dump: $LATEST_CORE"
echo "----------------------------------------"

# 4. Make the core dump readable and executable by the user
echo "Updating permissions for $LATEST_CORE"
chmod u+rx "$LATEST_CORE"

# 5. Run gdb to get the backtrace
gdb "$EXECUTABLE_PATH" "$LATEST_CORE" -ex "bt" --batch

echo "----------------------------------------"
echo "Script finished." 
