# WebRTC Test Driver Implementation Plan

## Overview

This document outlines the plan for a WebRTC-based client/test driver for the Sparkle Duck physics simulation. The client will provide real-time bidirectional communication between clients and the simulation, enabling automated testing, remote control, live monitoring capabilities, and shared access to a single simulation.

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

**WebSocket** (websocketpp or Boost.Beast):
- Command/control protocol (JSON-based).
- Simple request/response for simulation control.
- WebRTC signaling channel (offer/answer exchange).
- No NAT traversal needed on LAN.

**WebRTC** (libdatachannel):
- Real-time video streaming of LVGL framebuffer.
- Efficient compression (H.264/VP8) - ~500KB/s vs 5MB/s for PNG.
- Low latency (~50-100ms) for smooth remote viewing.
- Data channels for large binary state dumps.

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

## Proposed Architecture

### 1. Core Library Refactoring

Create a shared library that both executables link against:

```
src/
├── core/                              # Shared simulation core
│   ├── World.{cpp,h}
│   ├── Cell.{cpp,h}
│   ├── WorldInterface.{cpp,h}
│   ├── SimulationController.{cpp,h}   # NEW: headless simulation control
│   ├── All physics calculators
│   ├── Vector2d, MaterialType, etc.
│   └── serialization/
│       ├── WorldJSON.{cpp,h}
│       ├── CellJSON.{cpp,h}
│       ├── MaterialTypeJSON.{cpp,h}
│       └── ...
│
├── ui/                                # UI-specific code
│   ├── SimulatorUI.{cpp,h}
│   ├── MaterialPicker.{cpp,h}
│   ├── SimulationManager.{cpp,h}      # UI + Simulation coordinator
│   ├── lib/                           # LVGL backends
│   └── LVGLEventBuilder.{cpp,h}
│
├── network/                           # Server-specific code
│   ├── WebRTCServer.{cpp,h}
│   ├── NetworkController.{cpp,h}      # Network + Simulation coordinator
│   ├── ClientConnection.{cpp,h}
│   ├── CommandProcessor.{cpp,h}
│   └── DiscoveryService.{cpp,h}
│
└── bin/
    ├── main.cpp                       # UI app entry point
    ├── server_main.cpp                # Server app entry point
    └── test_main.cpp                  # Tests (already exists)
```

### 2. Key Abstraction: SimulationController

This is the piece that both UI and server would use:

```cpp
// src/core/SimulationController.h
class SimulationController {
    std::unique_ptr<WorldInterface> world_;
    bool paused_ = false;
    int frame_count_ = 0;

public:
    // Construction.
    SimulationController(WorldType type, int width, int height);

    // Control interface.
    void step(int frames = 1);
    void pause() { paused_ = true; }
    void resume() { paused_ = false; }
    void reset();

    // State access (const, thread-safe).
    const WorldInterface& getWorld() const { return *world_; }
    WorldStateSnapshot captureState() const;
    std::vector<uint8_t> captureScreenshot() const;

    // Parameter modification.
    void setGravity(const Vector2d& gravity);
    void setCellMaterial(int x, int y, MaterialType type, double fill);
    // ... other setters

    // Query.
    int getFrameCount() const { return frame_count_; }
    bool isPaused() const { return paused_; }
};
```

### 3. UI App Structure

```cpp
// src/bin/main.cpp
int main(int argc, char** argv) {
    // Parse display backend args.
    auto backend = parse_backend(argc, argv);

    // Create core simulation.
    auto sim_controller = std::make_unique<SimulationController>(200, 150);

    // Create UI that wraps the controller.
    auto sim_manager = std::make_unique<SimulationManager>(
        std::move(sim_controller), backend);

    // Run UI event loop.
    sim_manager->run();
}
```

### 4. Server App Structure

```cpp
// src/bin/server_main.cpp
int main(int argc, char** argv) {
    // Parse server args (port, world size, physics system).
    auto config = parse_server_config(argc, argv);

    // Create core simulation.
    auto sim_controller = std::make_unique<SimulationController>(
        config.world_type, config.width, config.height);

    // Create network controller that wraps the simulation.
    auto network_controller = std::make_unique<NetworkController>(
        std::move(sim_controller), config.port);

    // Run server event loop (WebRTC, command processing).
    network_controller->run();
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

### 7. Migration Path

1. **Phase 1a**: Extract `SimulationController` from existing `SimulationManager`.
   - Create new `SimulationController` class in existing location.
   - Refactor `SimulationManager` to use `SimulationController` internally.
   - Keep all files in current locations.

2. **Phase 1b**: Verify UI still works.
   - Build and run existing `sparkle-duck` executable.
   - Test all UI interactions (material selection, simulation controls, etc.).
   - Run existing visual tests to ensure physics unchanged.

3. **Phase 2a**: Reorganize directories (core/, ui/, network/).
   - Move files to new directory structure.
   - Update CMakeLists.txt to create `sparkle_core` library.
   - Update includes and build dependencies.

4. **Phase 2b**: Verify UI still works after reorganization.
   - Build and run `sparkle-duck` with new structure.
   - Ensure no regressions from directory reorganization.

5. **Phase 3**: Create basic `server_main.cpp` with WebSocket command protocol.
   - Implement JSON-based command/response protocol (step, pause, query state).
   - Add mDNS/Avahi service announcement for LAN discovery.
   - Test with simple command-line client (websocat, Python script).
   - WebSocket signaling will be reused for WebRTC negotiation later.

6. **Phase 4**: Add WebRTC video streaming.
   - Integrate libdatachannel for WebRTC support.
   - Use existing WebSocket for WebRTC signaling (offer/answer).
   - Add video track for real-time framebuffer streaming (H.264/VP8).
   - Add data channel for efficient binary state dumps.
   - Keep WebSocket commands alongside WebRTC for control.

7. **Phase 5**: Build LVGL network client.
   - Create `client_main.cpp` that connects to server.
   - Render simulation state from network in LVGL UI.
   - Test with local server first, then remote.

### 8. Refactoring SimulationManager

Currently `SimulationManager` does both simulation control AND UI coordination. We'd split it:

```cpp
// Before: SimulationManager does everything.
class SimulationManager {
    WorldInterface* world_;        // Simulation.
    SimulatorUI* ui_;              // UI.
    void advanceSimulation();      // Mixed concerns.
    void updateUI();
};

// After: Clean separation.
class SimulationController {
    WorldInterface* world_;
    void step(int frames);         // Pure simulation control.
};

class SimulationManager {
    SimulationController controller_;  // Owns simulation.
    SimulatorUI* ui_;                  // Owns UI.
    void syncUIToSimulation();         // UI coordination only.
};
```
