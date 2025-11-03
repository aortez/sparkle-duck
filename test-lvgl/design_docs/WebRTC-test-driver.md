# Sparkle Duck WebSocket Server

## Current Status - FULLY OPERATIONAL ✅

Headless WebSocket server with autonomous physics simulation and real-time emoji visualization.

### Quick Start

```bash
# Start server
./build/bin/sparkle-duck-server --port 8080

# Start simulation
./build/bin/cli ws://localhost:8080 sim_run '{}'

# Add materials
./build/bin/cli ws://localhost:8080 cell_set '{"x": 14, "y": 5, "material": "WATER", "fill": 1.0}'

# View world
./build/bin/cli ws://localhost:8080 diagram_get

# Shutdown
./build/bin/cli ws://localhost:8080 exit
```

## API Commands

All commands use snake_case. Legacy aliases supported on accident (remove these please).

| Command | Description | Parameters |
|---------|-------------|------------|
| `sim_run` | Start autonomous simulation | `{timestep, max_steps}` |
| `cell_get` | Get cell state | `{x, y}` |
| `cell_set` | Place material | `{x, y, material, fill}` |
| `diagram_get` | Get emoji visualization | none |
| `state_get` | Get complete world JSON | none |
| `gravity_set` | Set gravity | `{gravity}` |
| `reset` | Reset simulation | none |
| `exit` | Shutdown server | none |

## Architecture

### API Pattern

```cpp
namespace DirtSim::Api::CommandName {
    struct Command { /* params */ };
    struct Okay { /* response data */ };
    using Response = Result<Okay, ApiError>;
    using Cwc = CommandWithCallback<Command, Response>;
}
```

### States

`Startup` → `Idle` → `SimRunning` ↔ `SimPaused` → `Shutdown`

- Each state in separate header (StateForward.h + 5 state headers)
- Generic error responses for unhandled commands
- States own resources (SimRunning owns World)

### Key Implementation Details

**State machine bug fix:** States returning same type must be moved back into variant (StateMachine.cpp:79-82).

**Serialization:** qlibs/reflect + nlohmann/json with zero boilerplate.

**Error handling:** Compile-time safety with `static_assert`, runtime generic errors for unhandled commands.

## Directory Structure

```
src/
├── core/                    # Shared headless components
│   ├── World.{h,cpp}       # Physics system
│   ├── Cell.{h,cpp}        # Cell with JSON serialization
│   ├── WorldDiagramGeneratorEmoji.cpp  # Emoji rendering
│   └── CommandWithCallback.h  # Async command pattern
│
├── server/                 # Headless server
│   ├── main.cpp           # Server entry point
│   ├── StateMachine.{h,cpp}  # Server state machine
│   ├── Event.h            # Server events
│   ├── states/            # Server states (5 headers + StateForward.h)
│   ├── api/               # API commands (9 command files)
│   ├── network/           # WebSocketServer, serializers
│   └── scenarios/         # Scenario system
│
└── cli/                   # CLI client
    ├── main.cpp          # Programmatic command registry
    └── WebSocketClient.{h,cpp}
```

## Next Steps

**Cleanup:**
- Remove obsolete UIUpdateConsumer/SharedSimState
- Fix UI build (separate from server)
- Implement WebSocket-based UI stuff

**Features:**
- C++ remote test suite
- WebRTC video streaming
- mDNS service discovery

## Historical Notes

See git history for details on:
- nlohmann/json migration (removed LVGL dependencies)
- Aggregate types + reflection-based serialization
- Directory restructure (core/, server/, ui/)
- Removal of SimulationManager, WorldInterface, CellInterface

## Misc Ideas

- Variable timestep based on last frame time
- Require UI connection before starting simulation (optional mode)
- Scenario-specific WorldEventGenerator configuration
- The directory `uism` should be `state-machine`.
- Fix alphabetical order here:
⎿  Updated /home/oldman/workspace/sparkle-duck/test-lvgl/CMakeLists.txt with 1 addition
     147        src/ui/state-machine/states/StartMenu.cpp
     148        src/ui/state-machine/states/Startup.cpp
     149        src/ui/state-machine/network/CommandDeserializerJson.cpp
     150 +      src/ui/state-machine/network/WebSocketClient.cpp
     151        src/ui/state-machine/network/WebSocketServer.cpp
     152        src/ui/state-machine/api/Exit.cpp
     153        src/ui/state-machine/api/MouseDown.cpp
