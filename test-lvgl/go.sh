#!/bin/bash
# Builds and runs it.
set -uxo pipefail

time ./build_debug.sh && ./run_main.sh $@
