#!/bin/bash
set -euxo pipefail

# Configure with Debug build type and enable debug logging
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS_DEBUG="-g -O0 -DLOG_DEBUG" \
    -DCMAKE_C_FLAGS_DEBUG="-g -O0 -DLOG_DEBUG"

make -C build -j$(nproc)
