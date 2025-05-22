#!/bin/bash
# Builds and runs it.
set -euxo pipefail

time ./build.sh 
build/bin/sparkle-duck-tests
