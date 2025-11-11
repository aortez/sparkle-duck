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
./build/bin/cli ws://localhost:8080 step_n '{"frames": 10}'

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
- Detailed timer statistics

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

## Available Commands

| Command | Description | Parameters |
|---------|-------------|------------|
| `benchmark` | Run performance benchmark | `--steps`, `--scenario`, `--simulate-ui` |
| `integration_test` | Run integration test | None |
| `cell_get` | Get cell state | `{"x": 10, "y": 20}` |
| `cell_set` | Place material | `{"x": 50, "y": 50, "material": "WATER", "fill": 1.0}` |
| `diagram_get` | Get emoji visualization | None |
| `exit` | Shutdown server | None |
| `gravity_set` | Set gravity value | `{"gravity": 15.0}` |
| `perf_stats_get` | Get performance stats | None |
| `physics_settings_get` | Get physics settings | None |
| `physics_settings_set` | Set physics parameters | `{"settings": {"timescale": 1.5}}` |
| `reset` | Reset simulation | None |
| `scenario_config_set` | Update scenario config | `{"config": {...}}` |
| `sim_run` | Start simulation | `{"timestep": 0.016, "max_steps": 100}` |
| `state_get` | Get complete world state | None |
| `timer_stats_get` | Get timing breakdown | None |
| `step_n` | Advance N frames | `{"frames": 1}` |

## State Machine Flow

The server follows this state flow:

```
Startup → Idle → SimRunning → SimPaused → Shutdown
              ↑         ↓
              └─────────┘
```

**Important**:
- `sim_run` creates World and transitions Idle → SimRunning
- `step_n` only works in SimRunning state (requires existing World)
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

### Debugging

```bash
# Enable verbose logging
./build/bin/cli -v ws://localhost:8080 state_get

# Custom timeout
./build/bin/cli -t 10000 ws://localhost:8080 state_get
```

## Implementation Notes

### Subprocess Management

- Uses `fork()` + `execv()` for process spawning
- SIGTERM for graceful shutdown, SIGKILL fallback after 500ms
- Proper argument parsing for multi-argument commands

### Error Handling

- Connection timeouts (default 5s, configurable)
- Response timeouts for commands
- Process death detection
- Graceful cleanup on failures

### Performance

- Non-blocking server launch (background process)
- Concurrent connection attempts with retry logic
- Efficient binary protocol support for WorldData
