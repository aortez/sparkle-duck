#!/bin/bash
# Autoformatter, thanks clang-tidy.
set -euxo pipefail

find . -path ./lvgl -prune -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i
