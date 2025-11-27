#!/bin/bash
# Local deploy - build and restart on this machine

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

exec node src/scripts/deploy/deploy.mjs --local "$@"
