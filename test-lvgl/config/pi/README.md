# Pi Configuration Files

Configuration files for running Sparkle Duck on a Raspberry Pi with HyperPixel display.

## Files

- `labwc/rc.xml` - Window manager config with touch-to-display mapping
- `labwc/autostart` - Starts sparkle-duck service after compositor is ready
- `kanshi/config` - Display configuration (rotation, resolution)
- `sparkle-duck.desktop` - Desktop launcher
- `icons/` - Application icons (128x128, 256x256)
- `libfm/libfm.conf` - File manager settings (big icons, quick-exec)

## Installation

Run the install script on the Pi:

```bash
./install.sh
```

Or manually copy the files:

```bash
mkdir -p ~/.config/labwc ~/.config/kanshi
cp labwc/* ~/.config/labwc/
cp kanshi/* ~/.config/kanshi/
pkill -HUP labwc  # Reload config
```

## Customization

**Touch device name varies between Pis.** Check yours with:

```bash
libinput list-devices | grep -A2 -i touch
```

If the prefix differs (e.g., `14-005d` instead of `13-005d`), edit `labwc/rc.xml` accordingly.
