# WebRTC Test Driver Implementation Plan

## Status: In Progress

NOTE: we need to add an intermediate layer for commands - they should be deserilized from Json into a command, _then_ fed to the state machine for processing.  I don't know if we need CommandProcessor, let's discuss.

**Completed:**
- ✅ Draw area refactoring (World is headless, draw area passed as parameter)
- ✅ Removed RulesA/WorldType (single World implementation now)
- ✅ Removed WorldState (replaced with native JSON serialization)
- ✅ Cell JSON serialization (17 tests passing)
- ✅ World JSON serialization (21 tests passing)
- ✅ CommandProcessor with Result-based API (17 tests passing)
- ✅ JSON command protocol defined

**In Progress:**
- ⏳ WebSocket server implementation

**TODO:**
- WebRTC video streaming
- mDNS service discovery
- Python test client
- Network client examples

## Overview

This document outlines the plan for a WebSocket/WebRTC-based test driver for the Sparkle Duck physics simulation. The driver provides real-time bidirectional communication between clients and the simulation, enabling automated testing, remote control, and live monitoring capabilities.

## Goals

### Primary Use Cases
1. **Automated Test Driver**: Execute test sequences, capture world state, validate simulation behavior.
2. **Remote UI**: Real-time display/control of simulation from web browser.
3. **Debugging Interface**: Step-by-step simulation control and state inspection.

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

## Current Architecture

### Directory Structure

**Current (minimal reorganization):**
```
src/
├── World.{h,cpp}                      # Pure-material physics (formerly WorldB)
├── Cell.{h,cpp}                       # Pure-material cell (formerly CellB)
├── World*Calculator.{h,cpp}           # Physics calculators (8 files)
├── SimulationManager.{h,cpp}          # Coordinates UI + World (headless-capable)
├── SimulatorUI.{h,cpp}                # LVGL-based UI (optional)
├── MaterialType, Vector2d, etc.       # Core types with JSON
├── Result.h                           # Result<T,E> type for error handling
├── network/                           # ✅ NEW: Network layer
│   ├── CommandProcessor.{h,cpp}       # ✅ DONE: JSON command handler
│   ├── CommandResult.h                # ✅ DONE: Result wrapper
│   ├── WebSocketServer.{h,cpp}        # TODO: WebSocket server
│   └── NetworkController.{h,cpp}      # TODO: Server coordinator
├── main.cpp                           # UI app entry point
└── tests/                             # Comprehensive test suite
    ├── CellJSON_test.cpp              # ✅ NEW: 17 tests
    ├── WorldJSON_test.cpp             # ✅ NEW: 21 tests
    ├── CommandProcessor_test.cpp      # ✅ NEW: 17 tests
    └── ... (other tests)

# Future reorganization (when needed):
# - Create src/core/ for physics when building separate server executable
# - Move UI-specific code to src/ui/ if it becomes cluttered
```

### Key Simplifications

**Removed complexity:**
- ❌ No dual physics systems (RulesA removed)
- ❌ No WorldType enum (only one World now)
- ❌ No WorldState struct (direct JSON serialization instead)
- ❌ No WorldFactory (just `new World(w, h)`)
- ❌ No preserveState/restoreState (use toJSON/fromJSON)

**Result: Simpler, cleaner codebase focused on single physics system.**

### Simulation Control: Using SimulationManager Directly

**Decision:** Keep using `SimulationManager` directly rather than creating a separate `SimulationController`.

**Rationale:**
- SimulationManager already supports headless mode (`screen=nullptr, eventRouter=nullptr`)
- Thin delegation layer - no business logic to extract
- Adding another abstraction layer would be unnecessary complexity
- Network code can use SimulationManager directly

```cpp
// Server usage (headless)
auto manager = std::make_unique<SimulationManager>(
    200, 150,      // Grid dimensions
    nullptr,       // No screen (headless)
    nullptr        // No event router
);
manager->initialize();

// Network layer uses it directly
CommandProcessor processor(manager.get());
processor.processCommand(R"({"command": "step", "frames": 10})");
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

```json
// Step simulation
{"command": "step", "frames": 10}
→ {"value": {"timestep": 1234}}

// Place material
{"command": "place_material", "x": 50, "y": 75, "material": "WATER", "fill": 1.0}
→ {"value": {}}

// Get full world state
{"command": "get_state"}
→ {"value": {...complete World JSON...}}

// Get specific cell
{"command": "get_cell", "x": 10, "y": 20}
→ {"value": {...Cell JSON...}}

// Set gravity
{"command": "set_gravity", "value": 9.8}
→ {"value": {}}

// Reset world
{"command": "reset"}
→ {"value": {}}
```

**Implementation:** `src/network/CommandProcessor.{h,cpp}` (226 lines, 17 tests passing)

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
