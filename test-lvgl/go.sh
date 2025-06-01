#!/bin/bash
# Builds and runs it.
set -uxo pipefail

time ./build.sh && ./run.sh
