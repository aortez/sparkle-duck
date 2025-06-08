#!/bin/bash
# Builds and runs it.
set -exo pipefail

# Pass any arguments to build.sh
./build_debug.sh "$@"
build/bin/sparkle-duck-tests
