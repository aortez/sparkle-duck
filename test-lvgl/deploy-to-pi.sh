#!/bin/bash
# Deploy to Pi - customize these defaults for your setup

# === CONFIGURE THESE ===
PI_HOST="oldman@dirtsim.local"                                 # SSH host
PI_PATH="/home/oldman/workspace/sparkle-duck/test-lvgl"        # Remote path
SSH_KEY="$HOME/.ssh/id_ed25519_sparkle_duck"                   # SSH key (optional)
# ========================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Build args
ARGS=(
  --host "$PI_HOST"
  --path "$PI_PATH"
)

# Add SSH key if it exists
if [[ -f "$SSH_KEY" ]]; then
  ARGS+=(--ssh-key "$SSH_KEY")
fi

# Pass through any additional args
ARGS+=("$@")

# Run deploy script
exec node "$SCRIPT_DIR/src/scripts/deploy/deploy.mjs" "${ARGS[@]}"
