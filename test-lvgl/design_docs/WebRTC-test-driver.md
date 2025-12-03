# Sparkle Duck WebSocket Server

## Current Status - DUAL SYSTEM OPERATIONAL ✅

**DSSM Server:** Headless WebSocket server with autonomous physics simulation and frame notifications.
**UI Client:** LVGL-based UI with WebSocket client/server, connects to DSSM for simulation viewing.

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

## Network Services

### HTTP Dashboard (port 8081)

The server provides a web dashboard at `http://localhost:8081/garden` for monitoring all Sparkle Duck instances on the network.

**Features:**
- Auto-discovers instances via mDNS/Avahi peer discovery
- Shows real-time state for both server (physics) and UI
- Refreshes every 5 seconds
- Works across network (monitors remote Raspberry Pi instances)

**Peer Discovery:**
- PeerDiscovery service browses for `_sparkle-duck._tcp` mDNS services
- Dashboard queries `peers_get` API to find all instances
- Always includes localhost (8080 physics, 7070 UI) plus discovered remote peers
- Displays hostname, port, role (physics/ui), and current state

**State Display:**
- Physics server: "Running (scenario, step N)" or "Idle (ready)"
- UI: State machine state (StartMenu, SimRunning, Paused, etc.)

**Implementation:**
- HttpServer.cpp: Serves static HTML with embedded JavaScript
- Uses WebSocket API for status queries with correlation ID filtering
- Gracefully handles WorldData broadcasts from active simulations

## API Commands

### DSSM Server API (port 8080)

| Command | Description | Parameters |
|---------|-------------|------------|
| `sim_run` | Start autonomous simulation | `{timestep, max_steps}` |
| `cell_get` | Get cell state | `{x, y}` |
| `cell_set` | Place material | `{x, y, material, fill}` |
| `diagram_get` | Get emoji visualization | none |
| `state_get` | Get complete world JSON | none |
| `status_get` | Get server status (scenario, timestep, size) | none |
| `peers_get` | Get discovered network peers | none |
| `gravity_set` | Set gravity | `{gravity}` |
| `reset` | Reset simulation | none |
| `exit` | Shutdown server | none |

**Frame Notifications:** Server broadcasts `{type: "frame_ready", stepNumber, timestamp}` after each physics step.

### UI Server API (port 7070)

| Command | Description | Parameters |
|---------|-------------|------------|
| `sim_run` | Start simulation (forwards to DSSM) | none |
| `sim_pause` | Pause simulation | none |
| `sim_stop` | Stop simulation and return to start menu | none |
| `status_get` | Get UI state (state, connected, fps, display size) | none |
| `screenshot` | Capture screenshot | `{filepath}` (optional) |
| `mouse_down` | Mouse press event | `{pixelX, pixelY}` |
| `mouse_move` | Mouse drag event | `{pixelX, pixelY}` |
| `mouse_up` | Mouse release event | `{pixelX, pixelY}` |
| `exit` | Shutdown UI | none |

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

### DSSM Server States

`Startup` → `Idle` → `SimRunning` ↔ `SimPaused` → `Shutdown`

- Each state in separate header
- Generic error responses for unhandled commands
- SimRunning owns World, broadcasts frame_ready after each step

### UI Client States

`Startup` → `Disconnected` ↔ `StartMenu` → `SimRunning` ↔ `Paused` → `Shutdown`

- **Disconnected:** No DSSM connection, shows connection UI
- **StartMenu:** Connected to DSSM, ready to start simulation
- **SimRunning:** Receives frame_ready → requests state_get → renders world
- **Paused:** Simulation stopped, world frozen

### Communication Flow

```
UI (client) ←WebSocket→ DSSM (server)
    ↓                         ↓
port 7070              port 8080
(accepts remote      (accepts commands,
 UI commands)         broadcasts frames)
```

**Frame Update Cycle:**
1. DSSM advances physics → broadcasts `frame_ready`
2. UI receives notification → requests `state_get`
3. DSSM responds with WorldData JSON
4. UI parses via ReflectSerializer → renders to LVGL

### Key Implementation Details

**State machine pattern:** States returning same type must be moved back into variant (preserves state data).

**Serialization:** WorldData + ReflectSerializer = zero-boilerplate JSON.
- toJSON(): `return ReflectSerializer::to_json(data);` (1 line!)
- fromJSON(): `data = ReflectSerializer::from_json<WorldData>(doc);` (1 line!)

**Stateless calculators:** All physics calculators are stateless, methods take World& parameter.
- No world_ reference stored
- Trivially copyable
- World copy/move now = default

**Error handling:** Compile-time safety with `static_assert`, runtime generic errors for unhandled commands.

## Directory Structure

```
src/
├── core/                           # Shared components
│   ├── World.{h,cpp}              # Physics system
│   ├── WorldData.h                # Serializable state (auto via ReflectSerializer)
│   ├── Cell.{h,cpp}               # Cell with JSON serialization
│   ├── World*Calculator.{h,cpp}   # Stateless physics calculators (7 types)
│   ├── ReflectSerializer.h        # Zero-boilerplate JSON serialization
│   └── CommandWithCallback.h     # Async command pattern
│
├── server/                        # DSSM - Headless physics server
│   ├── main.cpp                  # Server entry point
│   ├── StateMachine.{h,cpp}      # State machine (Startup → Idle → SimRunning)
│   ├── Event.h                   # Server events
│   ├── states/                   # Server states (5 states)
│   ├── api/                      # API commands (15+ commands)
│   ├── network/                  # WebSocketServer, HttpServer, PeerDiscovery
│   │   ├── WebSocketServer       # Broadcasts frame_ready notifications (port 8080)
│   │   ├── HttpServer            # Web dashboard (port 8081/garden)
│   │   ├── PeerDiscovery         # mDNS/Avahi service discovery
│   │   └── CommandDeserializer   # JSON → Command
│   └── scenarios/                # Scenario system
│
├── ui/                           # UI Client - LVGL display + remote control
│   ├── main.cpp                 # UI entry point
│   ├── state-machine/           # UI State machine
│   │   ├── StateMachine.{h,cpp} # (Startup → Disconnected → StartMenu → SimRunning)
│   │   ├── Event.h              # UI events
│   │   ├── states/              # UI states (6 states)
│   │   ├── api/                 # UI API commands (7 commands)
│   │   └── network/             # WebSocket client + server
│   │       ├── WebSocketServer  # Accepts remote UI commands (port 7070)
│   │       └── WebSocketClient  # Connects to DSSM (receives frame notifications)
│   ├── SimulatorUI.{h,cpp}      # LVGL UI components (TODO: integrate)
│   ├── rendering/               # CellRenderer (TODO: integrate)
│   └── lib/                     # LVGL backends (wayland, x11, fbdev)
│
└── cli/                         # CLI client
    ├── main.cpp                # Programmatic command registry
    └── WebSocketClient.{h,cpp} # Single-request client
```

## Next Steps

**Cleanup:**
- Remove obsolete UIUpdateConsumer/SharedSimState (if any remain)
- Fix UI build (separate from server)

**Features:**
- ✅ mDNS service discovery (implemented via PeerDiscovery + HttpServer dashboard)
- C++ remote test suite
- WebRTC video streaming
- Server SimStop command (currently server stays in SimRunning, needs explicit stop)

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
