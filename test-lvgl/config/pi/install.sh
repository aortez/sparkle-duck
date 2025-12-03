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

# Install desktop icon.
mkdir -p ~/.local/share/icons ~/Desktop ~/.config/libfm
cp "$SCRIPT_DIR/icons/sparkle-duck-256.png" ~/.local/share/icons/sparkle-duck.png
cp "$SCRIPT_DIR/sparkle-duck.desktop" ~/Desktop/
chmod +x ~/Desktop/sparkle-duck.desktop

# Install libfm config (big icons, quick-exec).
cp "$SCRIPT_DIR/libfm/libfm.conf" ~/.config/libfm/

# Restart PCManFM to apply icon settings.
if pgrep -x pcmanfm > /dev/null; then
    pkill pcmanfm
    sleep 1
    pcmanfm --desktop &
    disown
fi

echo "Configuration installed."
echo "Desktop icon added to ~/Desktop/"

# Reload labwc if running.
if pgrep -x labwc > /dev/null; then
    echo "Reloading labwc..."
    pkill -HUP labwc
fi

echo "Done!"
