#!/bin/bash
# Builds it.
set -euxo pipefail

cmake -B build -S . "$@"
make -C build -j12

#build/bin/sparkle-duck-tests
