#!/bin/bash
# Builds and runs it.
set -exo pipefail

# Pass any arguments to build.sh
./build.sh "$@"
build/bin/sparkle-duck-tests
