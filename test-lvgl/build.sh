#!/bin/bash
# Builds it.
set -euxo pipefail

cmake -B build -S . "$@"
make -C build -j4

build/bin/sparkle-duck-tests
