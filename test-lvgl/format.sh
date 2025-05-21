#!/bin/bash
# Autoformatter, thanks clang-tidy.
set -euxo pipefail

clang-format --style=file --files=compile_commands.json
