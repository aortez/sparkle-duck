#!/bin/bash
# Runs it.
set -exo pipefail

./build/bin/sparkle-duck -b wayland $@
