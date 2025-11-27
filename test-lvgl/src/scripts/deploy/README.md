# Sparkle Duck Deployment

Deploy Sparkle Duck from your development machine to a Raspberry Pi.

## Overview

The deploy script syncs your local code to the Pi, builds it remotely, and restarts the systemd service.

## Prerequisites

- Node.js installed on dev machine
- SSH access to the Pi
- rsync installed on both machines

## One-Time Setup

### 1. Generate SSH Key (on dev machine)

```bash
# Generate a dedicated key for deployments
ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519_sparkle_duck -C "sparkle-duck-deploy"

# When prompted for passphrase:
# - Press Enter for no passphrase (convenient but less secure)
# - Or enter a passphrase and use ssh-agent (more secure)
```

### 2. Copy Public Key to Pi

```bash
# Replace dirtsim.local with your Pi's hostname or IP
ssh-copy-id -i ~/.ssh/id_ed25519_sparkle_duck.pub oldman@dirtsim.local
```

You'll be prompted for the Pi's password once. After this, key auth is set up.

### 3. Test Key Authentication

```bash
ssh -i ~/.ssh/id_ed25519_sparkle_duck oldman@dirtsim.local "echo 'Key auth works!'"
```

### 4. (Optional) Add SSH Config Entry

Add to `~/.ssh/config` on your dev machine for convenience:

```
Host dirtsim
    HostName dirtsim.local
    User oldman
    IdentityFile ~/.ssh/id_ed25519_sparkle_duck
```

Now you can use `ssh dirtsim` instead of the full command.

### 5. Set Up systemd Service (on Pi)

The service file should be at `~/.config/systemd/user/sparkle-duck.service` on the Pi:

```ini
[Unit]
Description=Sparkle Duck Physics Simulation
After=graphical-session.target

[Service]
Type=simple
WorkingDirectory=/home/oldman/workspace/sparkle-duck/test-lvgl
Environment=WAYLAND_DISPLAY=wayland-0
ExecStart=/home/oldman/workspace/sparkle-duck/test-lvgl/build-debug/bin/cli run-all
ExecStop=/home/oldman/workspace/sparkle-duck/test-lvgl/build-debug/bin/cli cleanup
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
```

Enable it:

```bash
systemctl --user daemon-reload
systemctl --user enable sparkle-duck.service
```

## Usage

### Remote Deploy (from dev machine)

From your dev machine, in the `test-lvgl` directory:

```bash
# Full deploy: sync, build, restart
./deploy-to-pi.sh

# Sync and restart without rebuilding
./deploy-to-pi.sh --no-build

# Just sync files (no build or restart)
./deploy-to-pi.sh --no-build --no-restart

# Preview what would happen
./deploy-to-pi.sh --dry-run

# Show all options
node src/scripts/deploy/deploy.mjs --help
```

### Local Deploy (on Pi itself)

When SSH'd into the Pi, or working directly on it:

```bash
# Full local deploy: build, restart
./deploy-local.sh

# Just restart (skip build)
./deploy-local.sh --no-build

# Preview what would happen
./deploy-local.sh --dry-run
```

## Customization

Edit `deploy-to-pi.sh` to change defaults:

```bash
PI_HOST="oldman@dirtsim.local"    # SSH host
PI_PATH="/home/oldman/..."         # Remote project path
SSH_KEY="~/.ssh/id_ed25519_..."    # SSH key path
```

## Controlling the Service (on Pi)

```bash
# Check status
systemctl --user status sparkle-duck

# View logs
journalctl --user -u sparkle-duck -f

# Stop/start/restart
systemctl --user stop sparkle-duck
systemctl --user start sparkle-duck
systemctl --user restart sparkle-duck
```

## Controlling via CLI (from anywhere with network access)

```bash
# Get simulation state (talks to server on port 8080)
./build-debug/bin/cli diagram_get ws://dirtsim.local:8080

# Start simulation (talks to UI on port 7070)
./build-debug/bin/cli sim_run ws://dirtsim.local:7070 '{"scenario_id": "sandbox"}'

# Get UI status
./build-debug/bin/cli status_get ws://dirtsim.local:7070
```

## Troubleshooting

**"Permission denied (publickey)"**
- Check key path in deploy-to-pi.sh matches your actual key
- Ensure public key was copied: `ssh-copy-id -i ~/.ssh/YOUR_KEY.pub user@host`

**"Connection refused"**
- Is the Pi online? `ping dirtsim.local`
- Is SSH running? `sudo systemctl status ssh` (on Pi)

**Service won't start**
- Check logs: `journalctl --user -u sparkle-duck -e`
- Is WAYLAND_DISPLAY correct? Run `echo $WAYLAND_DISPLAY` on Pi's desktop session

**Build fails on Pi**
- SSH in and build manually to see errors: `ssh dirtsim` then `cd ... && make debug`
