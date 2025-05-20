#!/bin/bash
# Runs it.
set -exo pipefail

./build/bin/lvglsim $1 $2
