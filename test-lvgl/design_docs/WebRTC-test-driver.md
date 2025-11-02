# WebRTC Test Driver Implementation Plan

## Status: Major Architectural Refactoring Complete

**Completed (Phase 1 - Foundation):**
- ✅ Draw area refactoring (World is headless, draw area passed as parameter)
- ✅ Removed RulesA/WorldType (single World implementation now)
- ✅ Cell JSON serialization (17 tests passing)
- ✅ World JSON serialization (21 tests passing)
- ✅ World is fully copyable (via WorldEventGenerator cloning)

**Completed (Phase 2 - API Architecture):**
- ✅ DirtSim::Api::* namespace pattern for commands (CellGet, CellSet, GravitySet, Reset, StateGet, StepN)
- ✅ CommandWithCallback<Command, Response> template for async responses
- ✅ CommandDeserializerJson (pure JSON → Command deserialization)
- ✅ ResponseSerializerJson (pure Response → JSON serialization)
- ✅ StateMachineInterface for dependency inversion
- ✅ State handlers for all 6 API commands in SimRunning state
- ✅ WebSocketServer with libdatachannel v0.23.2
- ✅ ApplyScenarioCommand and ResizeWorldCommand events

**Completed (Phase 3 - Architecture Cleanup):**
- ✅ WorldSetup → WorldEventGenerator rename (clearer purpose)
- ✅ Static event generation state → instance members (enables copying)
- ✅ Eliminated SimulationManager (unnecessary abstraction layer)
- ✅ DirtSimStateMachine owns World directly
- ✅ UIUpdateEvent simplified (World + metadata, no dirty flags)
- ✅ UI completely decoupled (communicates only via EventRouter, no direct world access)
- ✅ Removed 600+ lines of dead UI callback code

**Completed (2025-11-02):**
- ✅ Directory restructure (core/, server/, ui/) - COMPLETE
  - ✅ Complete file reorganization (34 files changed, -962/+222 lines)
  - ✅ EventProcessor separate for server/ui (no templates, forward declarations)
  - ✅ Events properly split: server/Event.h, ui/events/*.h
  - ✅ API commands split: server/api/*.h (one file per command)
  - ✅ CMakeLists.txt updated for new structure
  - ✅ All ~100+ #includes fixed
  - ✅ StateMachineInterface templated
  - ✅ Obsolete files removed (EventDispatcher, EventRouter, EventTraits from core/)
  - ✅ 38 obsolete tests removed
- ✅ UiStateMachine created (separate UI states from simulation states)
  - ✅ ui/StateMachine.{h,cpp} with full implementation
  - ✅ ui/states/ created (Startup, MainMenu, SimRunning, Paused, Shutdown)
  - ⏳ Network integration pending (WebSocketClient for connecting to server)
- ✅ Headless server build (95% complete)
  - ✅ server/StateMachine uses Server namespace
  - ✅ UI dependencies removed from server states
  - ✅ Cell rendering removed (draw methods, canvas, buffer, markDirty)
  - ✅ sparkle-duck renamed to sparkle-duck-ui (client-only)
  - ⏳ Final issue: rapidjson → nlohmann/json migration

**Completed (2025-11-02): Aggregate Types + Reflection-Based Serialization**

**Phase 1: Remove Obsolete Abstractions** ✅
- ✅ Removed CellInterface (only 1 implementation)
- ✅ Removed WorldInterface (only 1 implementation)
- ✅ Removed PressureSystem enum (WorldA legacy)
- ✅ Result: -688 lines of code

**Phase 2: Convert to Aggregate Types** ✅
- ✅ Vector2d: Converted to struct with default initializers (x=0.0, y=0.0)
- ✅ Cell: Converted to struct with 13 public members
  - Removed 2 constructors, destructor, copy/assignment (~84 lines)
  - Removed ~40 trivial getters/setters
  - Kept helpers with invariants (setFillRatio, setCOM, pressure helpers)
- ✅ Updated ~100 Vector2d call sites: `Vector2d(x,y)` → `Vector2d{x,y}`
- ✅ Fixed parameter shadowing in Cell methods

**Phase 3: Reflection-Based Serialization** ✅
- ✅ Added qlibs/reflect v1.3.1 (header-only, C++20 reflection)
- ✅ Created ReflectSerializer.h (generic JSON serialization)
- ✅ Vector2d: 24 lines manual code → 3 lines automatic
- ✅ Cell: 62 lines manual code → 3 lines automatic
- ✅ Uses nlohmann/json (no LVGL dependencies)

**Benefits:**
- Aggregate types enable qlibs/reflect automatic serialization
- Vector2d and Cell serialize themselves with zero boilerplate
- Adding new members to structs automatically includes them in JSON
- Clean separation: data (public members) vs behavior (helper methods)

**Completed (2025-11-02): nlohmann/json Migration**

✅ **Migration Complete** - `sparkle-duck-server` builds and links without LVGL dependencies!

**Migrated Components:**
1. ✅ **MaterialType** - ADL functions (to_json/from_json) for automatic conversion
2. ✅ **CommandDeserializerJson** - 164 lines → 67 lines (reflection-based)
3. ✅ **ResponseSerializerJson** - Template-based serialization with ADL
4. ✅ **World.cpp** - toJSON/fromJSON methods migrated (cleaner, no allocators)
5. ✅ **CrashDumpHandler** - Now uses nlohmann::json with complete state dumps
6. ✅ **API Command Structs** - All 6 commands use ReflectSerializer (zero boilerplate)
7. ✅ **Type Support** - ADL functions for Cell, Vector2d, MaterialType, World::MotionState

**Key Achievements:**
- Reflection-based serialization eliminates boilerplate
- ADL pattern enables automatic type conversions
- Command structs serialize themselves automatically
- World serialization: 69 lines → 57 lines (simpler, cleaner)
- CommandDeserializer: 164 lines → 67 lines (58% reduction)

**Build Status:**
```
✅ sparkle-duck-server compiles successfully
✅ sparkle-duck-server links without LVGL dependencies
✅ No lv_malloc, lv_free, lv_log_add references
✅ Ready for headless deployment
```

**TODO:**
- CLI swiss-army-knife tool (sparkle-duck-cli) for controlling both server and UI
- WebRTC video streaming
- mDNS service discovery
- Network client examples

**Long-term TODO:**
- Re-implement physics tests for new architecture (removed 38 obsolete tests during Phase 3 cleanup)

## Overview

This document outlines the plan for a WebSocket/WebRTC-based test driver for the Sparkle Duck physics simulation. The driver provides real-time bidirectional communication between clients and the simulation, enabling automated testing, remote control, and live monitoring capabilities.

## Goals

### Primary Use Cases
1. **Automated Test Driver**: Execute test sequences, capture world state, validate simulation behavior.
2. **Remote UI**: Real-time display/control of simulation from web browser.
3. **Debugging Interface**: Step-by-step simulation control and state inspection.

## Simulation Control Architecture

### Server-Driven Simulation with UI-Controlled Rendering

The UI and server are separate processes communicating via WebSocket. The server runs the simulation autonomously while the UI controls its own rendering rate:

**Flow:**
1. UI connects to server via WebSocket
2. UI sends `StartSimulation{timestep, duration}` command
3. Server runs simulation autonomously, sending lightweight `StepCompleted{stepNum, timestamp}` notifications
4. UI tracks server FPS from notifications
5. UI requests `GetState{}` when ready to render (adaptive to UI performance)
6. UI renders received world state while server continues computing

**Benefits:**
- Server runs at full speed independently
- UI controls rendering rate (can skip frames if slow, render all if fast)
- Decoupled: simulation rate ≠ rendering rate
- Lightweight notifications (step number + timestamp only)
- Adaptive performance

### Key Features
- Fluent CLI interface.
- JSON data format.
- Advance simulation by N frames.
- Capture complete grid state dumps.
- Control simulation parameters.
- Stream visual snapshots/screenshots.
- Real-time bidirectional communication.
- Multiple concurrent client connections.
- Automated discovery of server from client (maybe only on lan?).

## Technology Choices

### Network Protocol: WebSocket + WebRTC Hybrid

**WebSocket via libdatachannel** (Current implementation):
- Command/control protocol (JSON-based Result responses).
- Simple request/response for simulation control.
- libdatachannel chosen because it provides both WebSocket and WebRTC.
- Single dependency for current WebSocket and future WebRTC features.
- No Boost dependency needed.

**WebRTC via libdatachannel** (Future):
- Real-time video streaming of LVGL framebuffer.
- Efficient compression (H.264/VP8) - ~500KB/s vs 5MB/s for PNG.
- Low latency (~50-100ms) for smooth remote viewing.
- Uses WebSocket for signaling (offer/answer exchange).

### Discovery: Multi-Backend Architecture

**Initial implementation - LAN Discovery (mDNS/Avahi)**:
- Service type: `_sparkleduck._tcp.local`.
- Automatic server discovery on local network.
- Zero-configuration, works out of the box.
- Available on Linux (Avahi), macOS (Bonjour), Windows (Bonjour for Windows).

**Future extension - Internet Discovery**:
- Abstract `DiscoveryBackend` interface for pluggable discovery.
- LAN backend: mDNS (Phase 3).
- Internet backend options (future phases):
  - **Central directory**: Simple REST API for server registration/query.
  - **DHT**: Distributed discovery via libp2p or OpenDHT.
  - **Hybrid**: Support multiple backends simultaneously.
- WebRTC already supports internet connections via STUN/TURN servers.
- Only discovery mechanism needs to change - connection layer stays the same.

### Client Types Supported

1. **Python test scripts**: WebSocket commands + JSON responses.
2. **Web browsers**: WebSocket + WebRTC (native browser APIs).
3. **LVGL native client**: WebSocket commands + WebRTC video rendering.
4. **Command-line tools**: websocat, curl for simple testing.

## Key Architectural Decisions (2025-11-01)

### API Design Pattern

All network API commands follow this namespace pattern:

```cpp
namespace DirtSim::Api::<CommandName> {
    struct Command {
        // Command parameters
    };

    struct Okay {
        // Response data (structured types, NOT JSON)
    };
    using Response = Result<Okay, ApiError>;
    using Cwc = CommandWithCallback<Command, Response>;
}
```

**Examples:**
- `DirtSim::Api::CellGet::Command{x, y}` → `Response{Cell}`
- `DirtSim::Api::StateGet::Command{}` → `Response{World}`
- `DirtSim::Api::GravitySet::Command{gravity}` → `Response{std::monostate}`

**Key principles:**
1. API layer returns **structured data** (Cell, World objects), not JSON
2. JSON serialization happens in network layer (ResponseSerializerJson)
3. Commands are alphabetically organized (CellGet, CellSet, GravitySet, etc.)
4. Responses use Result<OkayType, ErrorType> for type-safe error handling

### Event Flow Architecture

```
WebSocket → JSON → CommandDeserializerJson → ApiCommand → Wrap in Cwc
                                                              ↓
                                                          Event Queue
                                                              ↓
                                                      State Machine
                                                              ↓
                                                      Process & Respond
                                                              ↓
                              Response → ResponseSerializerJson → JSON → WebSocket
```

**Separation of concerns:**
- **CommandDeserializerJson**: Pure JSON → Command (no callbacks, no state machine knowledge)
- **WebSocketServer**: Network layer (wraps Commands in Cwc, manages callbacks)
- **ResponseSerializerJson**: Pure Response → JSON (calls `.toJson()` on structured types)
- **State Machine**: Processes events, calls `cwc.sendResponse()`

### World Copyability

World is now fully copyable to support:
1. Sending complete world state over network (StateGet API)
2. UIUpdateEvent containing World copy for UI rendering
3. Future save/load functionality

**Implementation:**
- WorldEventGenerator has virtual `clone()` method for polymorphic copying
- WorldInterface copy constructor clones worldEventGenerator_
- World copy constructor chains to base, copies all state
- Static event generation vars → instance members (enables state preservation)

**Performance:**
- 50×50 world: ~156 KB (9 MB/sec at 60 FPS)
- 100×100 world: ~625 KB (37 MB/sec at 60 FPS)
- 200×150 world: ~1.8 MB (110 MB/sec at 60 FPS)
- Very practical for network transmission and UI updates

### Automatic Serialization (Planned)

**qlibs/reflect** (https://github.com/qlibs/reflect)
- Automatic C++20 reflection-based serialization
- Will replace manual `.toJson()` implementations
- Use `[[reflect]]` attribute on structs to auto-generate serialization code? Do we have a reasonable way to do this?

### UI/Simulation Separation

**Before:** UI had direct pointers to World and SimulationManager
**After:** UI communicates ONLY via events

**UI receives:** UIUpdateEvent{World world, fps, stepCount, isPaused}
**UI sends:** Events via EventRouter (SetGravityCommand, SelectMaterialCommand, etc.)
**UI stores:** `std::optional<World> lastWorldState_` for rendering/comparison

**Benefits:**
- UI can run in separate process (future WebRTC client)
- No tight coupling between UI and simulation
- Thread-safe by design (event queue between threads)

### SimulationManager Elimination

SimulationManager was unnecessary indirection:
- Just wrapped World ownership
- All methods delegated to World
- Forced UI dependency on headless code

**Replaced with:** DirtSimStateMachine owns World directly via `std::unique_ptr<WorldInterface> world`

### UiStateMachine (Next Step)

Current DirtSimStateMachine mixes simulation and UI concerns:
- Simulation states: Startup, SimRunning, Shutdown
- UI states: MainMenu, SimPaused, Config

**Plan:** Split into two state machines:
- **DirtSimStateMachine**: Pure simulation (headless-compatible)
- **UiStateMachine**: UI-only states with lifecycle:
  - **StartUp**: Initialize comms, then graphics layer
  - **StartMenu**: Let user press start button
  - **SimRunning**: Rendering simulation, handling user interactions
  - **Shutdown**: Cleanup, take exit screenshot

## Current Architecture

### Directory Structure

**New Architecture (Phase 3 - In Progress):**
```
src/
├── core/                              # Shared headless components
│   ├── World.{h,cpp}                  # Physics system
│   ├── Cell.{h,cpp}                   # Cell with JSON serialization
│   ├── World*Calculator.{h,cpp}       # Physics calculators (8 files)
│   ├── WorldEventGenerator.{h,cpp}    # World setup & particle generation
│   ├── EventProcessor.{h,cpp}         # Event queue processing
│   ├── EventRouter.{h,cpp}            # Dual-path event routing
│   ├── SharedSimState.h               # Thread-safe shared state
│   ├── SynchronizedQueue.h            # Thread-safe queue
│   ├── StateMachineBase.{h,cpp}       # Base class for state machines
│   └── serialization/                 # Shared JSON serialization
│       ├── CommandSerializer.h        # Command → JSON (for clients)
│       ├── CommandDeserializer.h      # JSON → Command (for servers)
│       ├── ResponseSerializer.h       # Response → JSON (for servers)
│       └── ResponseDeserializer.h     # JSON → Response (for clients)
│
├── server/                            # Headless server
│   ├── main_server.cpp                # Server entry point
│   ├── StateMachine.{h,cpp}           # Server state machine (owns World)
│   ├── Event.h                        # Server events (physics, API)
│   ├── ApiCommands.h                  # API command/response types
│   ├── states/                        # Server states (Startup, SimRunning, Shutdown)
│   ├── scenarios/                     # Scenario system
│   └── network/
│       └── WebSocketServer.{h,cpp}    # WebSocket server
│
├── ui/                                # UI application
│   ├── main.cpp                       # UI entry point
│   ├── StateMachine.{h,cpp}           # UI state machine (lifecycle, screenshots)
│   ├── Event.h                        # UI events (start, quit, screenshot)
│   ├── SimulatorUI.{h,cpp}            # LVGL UI rendering
│   ├── states/                        # UI states (Startup, MainMenu, SimRunning, Paused, Config, Shutdown)
│   ├── ui_builders/                   # LVGL widget builders
│   ├── network/                       # UI remote control
│   │   └── WebSocketServer.{h,cpp}    # WebSocket server for UI commands
│   └── lib/                           # LVGL backends (wayland, x11, etc.)
│
├── cli/                               # CLI control tool
│   ├── main_cli.cpp                   # CLI entry point
│   ├── WebSocketClient.{h,cpp}        # WebSocket client
│   └── CommandBuilder.h               # Build commands from CLI args
│
└── tests/                             # Test suite
    ├── CellJSON_test.cpp              # 17 tests passing
    ├── WorldJSON_test.cpp             # 21 tests passing
    └── ... (core utilities)
```

### Phase 3 Refactoring Notes (2025-11-01)

**Refactor Checkpoint - Before Include Fixes:**

Current directory structure successfully reorganized:
- core/: 45+ files (World, Cell, physics, event infrastructure, StateMachineBase)
- server/: StateMachine, states/, scenarios/, network/, api/
- ui/: StateMachine, states/, events/, ui_builders/, lib/
- Tests reduced to 12 core tests (38 obsolete tests removed)

**Known Issues (Build Breaking):**
1. StateMachineInterface references non-existent Event.h (needs templating)
2. ~85 files with broken #includes (old paths)
3. Obsolete files in core/ (EventDispatcher, EventRouter, EventTraits - reference old DirtSimStateMachine)
4. ui/lib/ files incorrectly include server/StateMachine.h (architectural violation)

**Next Steps:**
1. Delete obsolete core/ event files
2. Template StateMachineInterface
3. Fix network layer includes
4. Systematic include path fixes (~85 files)

**Rollback Point:** Git status shows all moves staged, easy to reset if needed.

---

### Key Simplifications

**Removed complexity:**
- ❌ No dual physics systems (RulesA removed)
- ❌ No WorldType enum (only one World now)
- ❌ No WorldState struct (direct JSON serialization instead)
- ❌ No WorldFactory (just `new World(w, h)`)
- ❌ No SimulationManager (unnecessary abstraction)
- ❌ No CommandProcessor (replaced with CommandDeserializerJson + event system)
- ❌ No direct UI→World coupling (all via events now)

**Result: Simpler, cleaner, event-driven architecture with clear separation of concerns.**

### Simulation Control: DirtSimStateMachine

**Decision:** DirtSimStateMachine owns World directly. SimulationManager eliminated as unnecessary abstraction.

**Rationale:**
- SimulationManager was just thin delegation to World
- Forced UI dependencies on headless code
- DirtSimStateMachine already existed and is more appropriate owner

```cpp
// Server usage (headless)
auto stateMachine = std::make_unique<DirtSimStateMachine>(nullptr); // No display
stateMachine->world->advanceTime(deltaTime);

// Network layer uses state machine
WebSocketServer server(*stateMachine, 8080);
server.start();
// Commands queued as events → state machine processes → responses via callbacks
```

### JSON Serialization (Completed)

**Cell serialization:**
```cpp
rapidjson::Value Cell::toJson(allocator) const;
static Cell Cell::fromJson(const rapidjson::Value& json);
```

**World serialization:**
```cpp
rapidjson::Document World::toJSON() const;  // Lossless, complete state
void World::fromJSON(const rapidjson::Document& doc);
```

**Format:**
- Grid metadata (width, height, timestep)
- All physics parameters (~20 fields)
- Sparse cell encoding (only non-empty cells)
- Hierarchical JSON structure

### JSON Command Protocol (Completed)

**All responses use Result-based format:**
```json
// Success response
{"value": {...response data...}}

// Error response
{"error": "error message"}
```

**Supported Commands:**

Advance Simulation n ms.

```json
// Step simulation
{"command": "step", "frames": 10}
→ {"value": {"timestep": 1234}}

// Place material
{"command": "place_material", "x": 50, "y": 75, "material": "WATER", "fill": 1.0}
→ {"value": {}}

// Get full world state (returns complete World object serialized to JSON)
{"command": "get_state"}
→ {"value": {...complete World JSON...}}

// Get specific cell (returns Cell object serialized to JSON)
{"command": "get_cell", "x": 10, "y": 20}
→ {"value": {...Cell JSON...}}

// Set gravity
{"command": "set_gravity", "value": 9.8}
→ {"value": {}}

// Reset world
{"command": "reset"}
→ {"value": {}}
```

**Implementation:**
- `CommandDeserializerJson`: Parses JSON into ApiCommand variant
- `WebSocketServer`: Wraps commands in Cwc with response callbacks
- `ResponseSerializerJson`: Converts structured Response types to JSON
- State handlers in `states/SimRunning.cpp` process commands and respond

### UI App (Current - Unchanged)

```cpp
// src/main.cpp (existing, works as before)
int main(int argc, char** argv) {
    // Create state machine with UI
    auto stateMachine = std::make_unique<DirtSimStateMachine>(lv_disp_get_default());

    // Get SimulationManager from state machine
    auto* simManager = stateMachine->getSimulationManager();

    // Run LVGL event loop
    // ... (existing code)
}
```

### Server App (Planned)

```cpp
// Future: src/network/main_server.cpp
int main(int argc, char** argv) {
    // Parse server args.
    auto config = parseServerArgs(argc, argv);

    // Create headless simulation.
    auto manager = std::make_unique<SimulationManager>(
        config.width, config.height,
        nullptr,  // No screen (headless)
        nullptr   // No event router
    );
    manager->initialize();

    // Create command processor.
    CommandProcessor processor(manager.get());

    // Create WebSocket server.
    WebSocketServer server(config.port);
    server.onMessage([&processor](const std::string& msg) {
        auto result = processor.processCommand(msg);
        if (result.isValue()) {
            return std::string("{\"value\":") + result.value() + "}";
        } else {
            return std::string("{\"error\":\"") + result.error().message + "\"}";
        }
    });

    // Run server.
    server.run();
}
```

### 5. Build System (CMakeLists.txt)

```cmake
# Core library (no UI dependencies).
add_library(sparkle_core STATIC
    src/core/World.cpp
    src/core/Cell.cpp
    src/core/SimulationController.cpp
    # ... all physics code
)
target_link_libraries(sparkle_core PUBLIC spdlog::spdlog nlohmann_json::nlohmann_json)

# UI executable.
add_executable(sparkle-duck
    src/bin/main.cpp
    src/ui/SimulatorUI.cpp
    src/ui/SimulationManager.cpp
    # ... UI code
)
target_link_libraries(sparkle-duck PRIVATE sparkle_core lvgl)

# Server executable.
add_executable(sparkle-duck-server
    src/bin/server_main.cpp
    src/network/WebRTCServer.cpp
    src/network/NetworkController.cpp
    # ... network code
)
target_link_libraries(sparkle-duck-server PRIVATE sparkle_core webrtc_library)

# Server doesn't need LVGL at all!
```

### 6. Benefits of This Structure

**Separation of Concerns**:
- Core physics has zero UI dependencies.
- Server has zero LVGL dependencies.
- Each executable optimized for its purpose.

**Code Reuse**:
- Tests, UI app, and server all use same `sparkle_core` library.
- Physics bugs fixed once, affects all executables.

**Future LVGL Client**:
```cpp
// Future: src/bin/client_main.cpp
int main() {
    auto backend = parse_backend();

    // Connect to remote server.
    auto network_client = std::make_unique<WebRTCClient>("server_address");

    // Create UI that renders remote state.
    auto client_ui = std::make_unique<RemoteSimulatorUI>(
        std::move(network_client), backend);

    client_ui->run();  // Renders state from server.
}
```

**Multiple Clients Viewing Same Sim**:
- One `sparkle-duck-server` running the physics.
- Multiple `sparkle-duck` (local UI) instances.
- Multiple web browsers.
- Multiple future LVGL clients.
- All seeing the same simulation state in real-time!

### Implementation Roadmap

**Phase 1: Foundation (COMPLETED ✅)**
1. Make World headless - pass draw area as parameter
2. Remove dual world system (RulesA/RulesB)
3. Add JSON serialization to Cell and World
4. Create CommandProcessor with Result-based API
5. Comprehensive testing (55 JSON tests)

**Phase 2: WebSocket Server (IN PROGRESS ⏳)**
1. Add libdatachannel dependency
2. Implement WebSocketServer.{h,cpp}
3. Implement NetworkController (owns SimulationManager + WebSocketServer)
4. Create main_server.cpp entry point
5. Design and create C++ CLI client.

**Phase 3: Test Automation (NEXT 🎯)**
1. Extend Test client
2. Automated test scenarios
3. CI/CD integration
4. Performance benchmarking

**Phase 4: WebRTC Video (FUTURE)**
1. Integrate libdatachannel WebRTC
2. Stream LVGL framebuffer as H.264/VP8
3. WebSocket for signaling (offer/answer)
4. Low-latency video for remote monitoring

**Phase 5: Discovery & Polish (FUTURE)**
1. mDNS/Avahi service discovery
2. Connection management (multiple clients)
3. Authentication/rate limiting
4. Documentation and examples

## Example Usage

### Python Test Client

```python
import websocket
import json

ws = websocket.create_connection("ws://localhost:8080")

# Step simulation
response = json.loads(ws.send(json.dumps({"command": "step", "frames": 100})))
print(f"Timestep: {response['value']['timestep']}")

# Place water
ws.send(json.dumps({
    "command": "place_material",
    "x": 50, "y": 75,
    "material": "WATER",
    "fill": 1.0
}))

# Get world state
ws.send(json.dumps({"command": "get_state"}))
state = json.loads(ws.recv())
print(f"World: {state['value']['grid']['width']}x{state['value']['grid']['height']}")
print(f"Cells: {len(state['value']['cells'])}")
```

### Command-line (websocat)

```bash
# Connect to server
websocat ws://localhost:8080

# Send commands
{"command": "step", "frames": 5}
{"command": "set_gravity", "value": 15.0}
{"command": "get_state"}
```


## Summary: What We Built

### Architecture Achievements

**Headless World:**
- World can run without any LVGL dependencies.
- `draw(lv_obj_t& drawArea)` only called when rendering needed.
- Perfect for server deployment.

**Single Physics System:**
- Removed RulesA (mixed materials) entirely.
- WorldB → World (pure materials, fill ratios).
- Eliminated WorldType enum, WorldFactory, WorldState complexity.
- Result: ~3000 lines of code removed, cleaner architecture.

**Lossless JSON Serialization:**
- Cell: Serialize material, fill, COM, velocity, pressure.
- World: Complete state - grid, physics, all parameters, cells.
- Sparse encoding (only non-empty cells).
- 38 comprehensive tests validate round-trip accuracy.

**Command Processing Layer:**
- CommandProcessor translates JSON → SimulationManager method calls.
- Result<string, CommandError> for type-safe error handling.
- 6 commands: step, place_material, get_state, get_cell, set_gravity, reset.
- 17 tests validate command parsing, execution, and error handling.

### Next Steps

**Immediate (WebSocket Server):**
1. Add libdatachannel to CMakeLists.txt.
2. Create WebSocketServer wrapper around libdatachannel.
3. Create main_server.cpp using CommandProcessor.
4. Test with websocat or Python client.

**Near-term (Test Automation):**
1. Python library for test automation.
2. Automated test scenarios.
3. Network-based CI/CD testing.

**Long-term (WebRTC Video):**
1. Add video track to libdatachannel.
2. Stream LVGL framebuffer as H.264.
3. Enable real-time remote viewing.

### Testing Summary

| Test Suite | Tests | Status |
|------------|-------|--------|
| CellJSON_test | 17 | ✅ All passing |
| WorldJSON_test | 21 | ✅ All passing |  
| CommandProcessor_test | 17 | ✅ All passing |
| **Total** | **55** | **✅ All passing** |

The foundation is solid and ready for network layer implementation.


Misc thoughts:
* It might be cool to both allow the server to run fully headlessly and for it to require a UI connection before starting the sim.
