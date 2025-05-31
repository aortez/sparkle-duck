#!/bin/bash
# Autoformatter, thanks clang-tidy.
set -euxo pipefail

clang-format --style=file -i src/*.cpp src/*.h
