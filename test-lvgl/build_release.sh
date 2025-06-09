#!/bin/bash
set -euxo pipefail

# Clean build directory
rm -rf build

# Configure with Release build type
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build with all available cores
make -C build -j$(nproc)

# Run tests to verify the build
build/bin/sparkle-duck-tests 
