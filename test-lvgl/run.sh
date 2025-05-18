#!/bin/bash
# Runs it.
set -euxo pipefail

./build/bin/lvglsim $1 $2
