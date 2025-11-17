# Logging Channels Design

## Overview

This document describes the implementation of logging channels for the Sparkle Duck physics simulator to enable fine-grained control over log output filtering.

## Motivation

The application generates extensive logging across multiple subsystems (physics, cohesion, pressure, UI, networking, etc.). Current single-logger approach makes it difficult to:
- Focus on specific subsystem debugging
- Reduce log noise when investigating particular behaviors
- Maintain different log levels for different components

## Implementation

### Named Logger Architecture

Each subsystem gets its own named spdlog logger sharing common sinks:

```cpp
// Core channels
physics     - General physics calculations
cohesion    - Cohesion force calculations
pressure    - Pressure system (hydrostatic, dynamic, diffusion)
collision   - Collision detection and material transfers
swap        - Material swapping behavior
friction    - Static/kinetic friction calculations
support     - Support detection and structural analysis
viscosity   - Viscosity and flow resistance

// System channels
ui          - UI components and rendering
network     - WebSocket client/server communications
state       - State machine transitions
scenario    - Scenario loading and management
```

### Logger Factory Pattern

```cpp
class LoggingChannels {
public:
    static void initialize(spdlog::level::level_enum consoleLevel,
                          spdlog::level::level_enum fileLevel);

    static std::shared_ptr<spdlog::logger> get(const std::string& channel);

    // Convenience accessors
    static std::shared_ptr<spdlog::logger> physics();
    static std::shared_ptr<spdlog::logger> cohesion();
    // ... etc
};
```

### Usage Pattern

```cpp
// Get channel logger
auto log = LoggingChannels::cohesion();
log->trace("Cohesion force at ({},{}): {}", x, y, force);

// Or inline
LoggingChannels::physics()->debug("Velocity: {}", velocity);
```

### Command Line Control

```bash
# Enable specific channels
./build/bin/sparkle-duck-ui -C "physics:trace,cohesion:debug,ui:info"

# Disable noisy channels
./build/bin/sparkle-duck-ui -C "pressure:off,friction:off"

# Focus on single subsystem
./build/bin/sparkle-duck-ui -C "collision:trace,*:off"
```

### Configuration Format

Channel specifications use comma-separated `channel:level` pairs:
- `physics:trace` - Set physics channel to trace level
- `cohesion:off` - Disable cohesion logging
- `*:info` - Set all channels to info (wildcard)
- `*:off,collision:trace` - Disable all except collision

## Integration Points

### Main Initialization

Both server and UI main.cpp will initialize channels after parsing arguments:

```cpp
// Parse channel configuration
if (log_channels) {
    LoggingChannels::initialize(console_level, file_level);
    LoggingChannels::configureFromString(args::get(log_channels));
}
```

### Migration Strategy

1. **Phase 1**: Add LoggingChannels class, keep existing spdlog calls working
2. **Phase 2**: Gradually migrate subsystems to use channel loggers
3. **Phase 3**: Add runtime channel control via UI or WebSocket API

## Benefits

- **Performance**: Zero cost when channels disabled (level check happens before formatting)
- **Debugging**: Focus on specific subsystems without log flooding
- **Flexibility**: Different log levels per component
- **Backwards Compatible**: Existing `spdlog::info()` calls continue working
- **Runtime Control**: Change channel levels without recompilation

## Example Scenarios

```bash
# Debug pressure system issues
./build/bin/sparkle-duck-ui -C "pressure:trace,*:warn"

# Investigate collision problems
./build/bin/sparkle-duck-ui -C "collision:trace,support:debug,*:error"

# Performance profiling (minimal logging)
./build/bin/sparkle-duck-ui -C "*:error"

# Full trace of physics but quiet UI
./build/bin/sparkle-duck-ui -C "physics:trace,cohesion:trace,ui:error"
```

## Future Enhancements

- **UI Controls**: Add toggles in PhysicsControls to enable/disable channels at runtime
- **WebSocket API**: Control logging channels via remote commands
- **Persistent Config**: Save preferred channel settings to config file
- **Log Routing**: Send different channels to different files
- **Structured Logging**: Add JSON output format for log analysis tools