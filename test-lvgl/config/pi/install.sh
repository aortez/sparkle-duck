#!/bin/bash
# Install Pi configuration files for Sparkle Duck.
# Run this on the Pi after cloning the repo.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Installing Pi configuration files..."

# Create config directories.
mkdir -p ~/.config/labwc ~/.config/kanshi

# Check touch device name and warn if it differs.
TOUCH_DEVICE=$(libinput list-devices 2>/dev/null | grep -A1 "Goodix Capacitive TouchScreen" | head -1 | sed 's/.*: *//')
if [[ -n "$TOUCH_DEVICE" && "$TOUCH_DEVICE" != *"13-005d"* ]]; then
    echo "WARNING: Your touch device is '$TOUCH_DEVICE'"
    echo "         The config expects '13-005d Goodix Capacitive TouchScreen'"
    echo "         You may need to edit ~/.config/labwc/rc.xml after installation."
fi

# Copy labwc config.
cp "$SCRIPT_DIR/labwc/rc.xml" ~/.config/labwc/
cp "$SCRIPT_DIR/labwc/autostart" ~/.config/labwc/
chmod +x ~/.config/labwc/autostart

# Copy kanshi config.
cp "$SCRIPT_DIR/kanshi/config" ~/.config/kanshi/

echo "Configuration installed."

# Reload labwc if running.
if pgrep -x labwc > /dev/null; then
    echo "Reloading labwc..."
    pkill -HUP labwc
fi

echo "Done!"
