# Sparkle Duck CLI Client

Command-line client for interacting with Sparkle Duck server and UI via WebSocket.

## Quick Start

**New to the CLI?** Try these commands first:

```bash
# 1. Launch everything (easiest way to get started)
./build/bin/cli run-all

# 2. In another terminal, send a command
./build/bin/cli status_get ws://localhost:8080

# 3. See a visual snapshot
./build/bin/cli diagram_get ws://localhost:8080

# 4. Clean up when done
./build/bin/cli cleanup
```

That's it! Now read below for details...

## Overview

The CLI provides five modes of operation:

1. **Command Mode**: Send individual commands to server or UI
2. **Run-All Mode**: Launch both server and UI with one command
3. **Benchmark Mode**: Automated performance testing with metrics collection
4. **Cleanup Mode**: Find and gracefully shutdown rogue sparkle-duck processes
5. **Integration Test Mode**: Automated server + UI lifecycle testing

## Usage

### Command Mode

Send commands to the server or UI:

```bash
# Syntax: cli [command] [address] [params]

# Basic command (no parameters)
./build/bin/cli state_get ws://localhost:8080

# Command with JSON parameters
./build/bin/cli sim_run ws://localhost:8080 '{"timestep": 0.016, "max_steps": 10}'

# Place material
./build/bin/cli cell_set ws://localhost:8080 '{"x": 50, "y": 50, "material": "WATER", "fill": 1.0}'

# Get emoji visualization
./build/bin/cli diagram_get ws://localhost:8080

# Control simulation
./build/bin/cli sim_run ws://localhost:8080 '{"timestep": 0.016, "max_steps": 100}'
./build/bin/cli reset ws://localhost:8080
./build/bin/cli exit ws://localhost:8080

# Send commands to UI (port 7070)
./build/bin/cli draw_debug_toggle ws://localhost:7070 '{"enabled": true}'
```

### Run-All Mode

Launch both server and UI with a single command:

```bash
# Auto-detects display backend and launches both processes
./build/bin/cli run-all
```

**What it does**:
- Auto-detects display backend (Wayland/X11)
- Launches server on port 8080
- Launches UI and auto-connects to server
- Monitors UI process
- Auto-shuts down server when UI exits

**Use case**: Quickest way to launch everything for interactive testing.

**Note**: Runs in foreground - press Ctrl+C to exit both processes.

### Benchmark Mode

Automated performance testing with server auto-launch:

```bash
# Basic benchmark (headless server, default: benchmark scenario, 120 steps)
./build/bin/cli benchmark --steps 120

# Different scenario
./build/bin/cli benchmark --scenario sandbox --steps 120

# Custom world size (default: scenario default)
./build/bin/cli benchmark --world-size 150 --steps 120

# Full control: scenario, world size, and step count
./build/bin/cli benchmark --scenario sandbox --world-size 150 --steps 1000
```

**Output**: Clean JSON results including:
- Scenario name and grid size
- Total duration
- Server FPS
- Physics timing (avg/total/calls)
- Serialization timing
- Detailed timer statistics (subsystem breakdown: cohesion, adhesion, pressure, etc.)

**Features**:
- Server runs with logging disabled (`--log-config benchmark-logging-config.json`) for clean output
- Client logs suppressed (use `--verbose` to see debug info)
- Pure JSON output suitable for piping to `jq` or CI/CD tools

**Example output:**
```json
{
  "scenario": "sandbox",
  "steps": 120,
  "server_physics_avg_ms": 0.52,
  "timer_stats": {
    "cohesion_calculation": {"avg_ms": 0.09, "total_ms": 10.8, "calls": 120},
    "adhesion_calculation": {"avg_ms": 0.03, "total_ms": 3.6, "calls": 120},
    "resolve_forces": {"avg_ms": 0.28, "total_ms": 33.6, "calls": 120}
  }
}
```

**Using for optimization testing:**
```bash
# Baseline measurement
./build/bin/cli benchmark --steps 1000 > baseline.json

# After optimization
./build/bin/cli benchmark --steps 1000 > optimized.json

# Compare specific subsystems
jq '.timer_stats.cohesion_calculation.avg_ms' baseline.json optimized.json
```

### Cleanup Mode

Find and gracefully shutdown rogue sparkle-duck processes:

```bash
# Clean up all sparkle-duck processes
./build/bin/cli cleanup
```

**Shutdown cascade** (tries each method in order):
1. **WebSocket API** - Send Exit command (most graceful)
   - Server: `ws://localhost:8080`
   - UI: `ws://localhost:7070`
2. **SIGTERM** - Graceful OS signal
3. **SIGKILL** - Force kill (last resort)

**All waits exit early** if process dies before timeout.

**Performance**: Typical cleanup time is under 500ms. WebSocket shutdown usually completes in 200-400ms.

**Use cases:**
- Clean up after crashes during development
- Ensure clean slate before running benchmarks or tests
- Fix "port already in use" errors

### Integration Test Mode

Automated end-to-end testing:

```bash
./build/bin/cli integration_test
```

**What it does**:
1. Launches server on port 8080
2. Launches UI with Wayland backend, auto-connects to server
3. Starts simulation with `sim_run` (1 step)
4. Sends `exit` command to server
5. Shuts down UI
6. Verifies clean shutdown of both processes

**Exit Codes**:
- `0`: All tests passed
- `1`: Test failed (check stderr for details)

## Architecture

### Components

**IntegrationTest** (`IntegrationTest.{h,cpp}`)
- Orchestrates server + UI launch and testing
- Manages full lifecycle from launch to cleanup
- Returns exit code for CI/CD integration

**BenchmarkRunner** (`BenchmarkRunner.{h,cpp}`)
- Launches server subprocess
- Runs simulation and collects metrics
- Supports UI client simulation mode

**SubprocessManager** (`SubprocessManager.{h,cpp}`)
- RAII wrapper for fork/exec/kill
- Manages both server and UI subprocesses
- Handles graceful shutdown (SIGTERM) with SIGKILL fallback

**WebSocketClient** (`WebSocketClient.{h,cpp}`)
- Dual-mode WebSocket client (blocking + async)
- Supports JSON and binary (zpp_bits) messages
- 10MB message size limit for large WorldData

### Communication Protocols

**JSON Commands** (text):
```json
{"command": "state_get"}
{"command": "sim_run", "timestep": 0.016, "max_steps": 100}
```

**Binary Messages** (zpp_bits):
- WorldData serialized with zpp_bits for efficiency
- Automatically unpacked to JSON for compatibility

**Notes**:
- `sim_run` creates World and transitions Idle → SimRunning
- Set `max_steps` to control simulation duration:
  - `-1` = unlimited (runs until paused or stopped)
  - `>0` = runs that many steps then transitions to SimPaused
- `exit` works from any state

## Troubleshooting

### "Failed to connect to ws://localhost:8080"

Server not running? Launch it:
```bash
./build/bin/cli run-all
```

Or check what's running:
```bash
pgrep -fa sparkle-duck
```

### "Port already in use"

Clean up any rogue processes:
```bash
./build/bin/cli cleanup
```

### Cleanup seems slow or hangs

If `run-all` is running in another terminal, it monitors the UI and won't let the server exit until the UI closes. Either:
- Use Ctrl+C on the `run-all` terminal first
- Or just use `cleanup` - it will force shutdown with SIGTERM/SIGKILL

### Response timeout errors

Increase timeout for slow operations:
```bash
./build/bin/cli --timeout 10000 state_get ws://localhost:8080  # 10 second timeout
```

### Want to see what's happening?

Use verbose mode to see WebSocket traffic:
```bash
./build/bin/cli --verbose status_get ws://localhost:8080
```

## Use Cases

### CI/CD Integration

```bash
# Performance regression testing
./build/bin/cli benchmark --steps 120 > benchmark_results.json

# Sanity check
./build/bin/cli integration_test || exit 1
```

### Scripted Testing

```bash
#!/bin/bash
# Launch server and UI using CLI
./build/bin/cli run-all &

# Wait for ready
sleep 3

# Run commands
./build/bin/cli sim_run ws://localhost:8080 '{"timestep": 0.016, "max_steps": 100}'
./build/bin/cli state_get ws://localhost:8080 > world_state.json
./build/bin/cli diagram_get ws://localhost:8080

# Cleanup (gracefully shuts down both server and UI)
./build/bin/cli cleanup
```

## Tips & Best Practices

### Use StatusGet for Polling

StatusGet is lightweight (~15ms response time) compared to StateGet (full world data):

```bash
# Fast status check for polling
./build/bin/cli status_get ws://localhost:8080 | jq '{timestep: .value.timestep}'

# Full world state (slower, use for detailed inspection)
./build/bin/cli state_get ws://localhost:8080
```

### Polling Pattern

Wait for simulation to complete a certain number of steps:

```bash
./build/bin/cli sim_run ws://localhost:8080 '{"max_steps": 100}'

while true; do
    STEP=$(./build/bin/cli status_get ws://localhost:8080 | jq '.value.timestep')
    echo "Current step: $STEP"
    [ "$STEP" -ge 100 ] && break
    sleep 0.1
done
```

### Debugging with Verbose Mode

```bash
# See WebSocket traffic and correlation IDs
./build/bin/cli --verbose status_get ws://localhost:8080

# Shows:
# - Connection establishment
# - Correlation ID: 1
# - JSON request/response
# - Response routing
```

### Timing Analysis

```bash
# Time individual commands
time ./build/bin/cli status_get ws://localhost:8080

# Timestamp entire workflow
./workflow_script.sh 2>&1 | ts '[%H:%M:%.S]'
```

### Always Cleanup

The cleanup command is robust and handles edge cases:

```bash
# Gracefully shuts down ALL sparkle-duck processes
./build/bin/cli cleanup

# Shows which method worked:
# ✓ WebSocket API (most graceful)
# ✓ SIGTERM (graceful signal)
# ✓ SIGKILL (force kill)
```

### Auto-Generated Command List

The CLI automatically discovers all server and UI commands at compile-time.
Check the help to see what's available:

```bash
# Always up-to-date with actual server/UI capabilities
./build/bin/cli --help

# Server API Commands (18 total)
# UI API Commands (10 total)
```

When new commands are added to the server or UI, they automatically appear in the CLI help.
