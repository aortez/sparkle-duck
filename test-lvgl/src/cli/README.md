# Sparkle Duck CLI Client

Command-line client for interacting with Sparkle Duck server and UI via WebSocket.

## Overview

The CLI provides three modes of operation:

1. **Command Mode**: Send individual commands to server or UI
2. **Benchmark Mode**: Automated performance testing with metrics collection
3. **Integration Test Mode**: Automated server + UI lifecycle testing

## Usage

### Command Mode

Send commands to the server or UI:

```bash
# Basic command (no parameters)
./build/bin/cli ws://localhost:8080 state_get

# Command with JSON parameters
./build/bin/cli ws://localhost:8080 sim_run '{"timestep": 0.016, "max_steps": 10}'

# Place material
./build/bin/cli ws://localhost:8080 cell_set '{"x": 50, "y": 50, "material": "WATER", "fill": 1.0}'

# Get emoji visualization
./build/bin/cli ws://localhost:8080 diagram_get

# Control simulation
./build/bin/cli ws://localhost:8080 sim_run '{"timestep": 0.016, "max_steps": 100}'
./build/bin/cli ws://localhost:8080 reset
./build/bin/cli ws://localhost:8080 exit

# Send commands to UI (port 7070)
./build/bin/cli ws://localhost:7070 draw_debug_toggle '{"enabled": true}'
```

### Benchmark Mode

Automated performance testing with server auto-launch:

```bash
# Basic benchmark (headless server, 120 steps)
./build/bin/cli benchmark --steps 120

# Simulate UI client load (realistic with frame_ready responses)
./build/bin/cli benchmark --steps 120 --simulate-ui

# Different scenario
./build/bin/cli benchmark --scenario dam_break --steps 120
```

**Output**: JSON results including:
- Server FPS
- Physics timing (avg/total/calls)
- Serialization timing
- Client round-trip latency (when --simulate-ui)
- Detailed timer statistics (subsystem breakdown: cohesion, adhesion, pressure, etc.)

**Example output:**
```json
{
  "scenario": "sandbox",
  "steps": 120,
  "server_physics_avg_ms": 0.52,
  "timer_stats": {
    "cohesion_calculation": {"avg_ms": 0.09, "total_ms": 10.8, "calls": 120},
    "adhesion_calculation": {"avg_ms": 0.03, "total_ms": 3.6, "calls": 120},
    "resolve_forces_total": {"avg_ms": 0.28, "total_ms": 33.6, "calls": 120}
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
# Launch server
./build/bin/sparkle-duck-server -p 8080 &
SERVER_PID=$!

# Wait for ready
sleep 2

# Run commands
./build/bin/cli ws://localhost:8080 sim_run '{"timestep": 0.016, "max_steps": 100}'
./build/bin/cli ws://localhost:8080 state_get > world_state.json
./build/bin/cli ws://localhost:8080 diagram_get

# Cleanup
./build/bin/cli ws://localhost:8080 exit
wait $SERVER_PID
```
