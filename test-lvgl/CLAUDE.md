# CLAUDE.md

This file provides guidance when working with code in this repository.

## Project Overview

Sparkle Duck is a playground for experimenting with Yocto, Zephyr, and LVGL technologies. The main application is a **cell-based multi-material physics simulation** located in the `test-lvgl` directory that demonstrates advanced physics simulation with interactive UI controls.

The project features a **pure-material physics system** with fill ratios and 9 material types (AIR, DIRT, LEAF, METAL, SAND, SEED, WALL, WATER, WOOD).

Read design_docs/coding_convention.md for coding guidelines.

## Essential Commands

### Building
```bash
# Build release version
make release

# Build debug version (since we're usually doing development, generally prefer this over the release version)
make debug

# Make the unit tests
make build-tests

# Format source code
make format

# Clean build artifacts
make clean

# Show all available targets
make help

# Manual CMake build (if needed)
cmake -B build -S .
make -C build -j12
```

### Running
```bash
# Run headless DSSM server (Dirt Sim State Machine)
./build/bin/sparkle-duck-server -p 8080 -s 1000

# Run UI client (auto-connects to server)
./build/bin/sparkle-duck-ui -b wayland --connect localhost:8080

# Run server and UI together (two terminals)
# Terminal 1:
./build/bin/sparkle-duck-server -p 8080

# Terminal 2:
./build/bin/sparkle-duck-ui -b wayland --connect localhost:8080

# UI options
./build/bin/sparkle-duck-ui -b wayland        # Wayland backend
./build/bin/sparkle-duck-ui -b x11            # X11 backend
./build/bin/sparkle-duck-ui -W 1200 -H 1200   # Custom window size
./build/bin/sparkle-duck-ui -s 100            # Auto-exit after 100 steps

# CLI client for sending commands
./build/bin/cli ws://localhost:8080 state_get
./build/bin/cli ws://localhost:8080 sim_run '{"timestep": 0.016, "max_steps": 1}'
./build/bin/cli ws://localhost:8080 diagram_get

# CLI integration test
./build/bin/cli integration_test

# For complete CLI documentation, see:
# src/cli/README.md
```

### Testing
```bash
# Run all unit tests
make test

# Run tests with filters using ARGS
make test ARGS='--gtest_filter=State*'

# Run state machine tests directly
./build/bin/sparkle-duck-tests --gtest_filter="StateIdle*"
./build/bin/sparkle-duck-tests --gtest_filter="StateSimRunning*"

# List all available tests
./build/bin/sparkle-duck-tests --gtest_list_tests

# Run specific test
./build/bin/sparkle-duck-tests --gtest_filter="StateSimRunningTest.AdvanceSimulation_StepsPhysicsAndDirtFalls"
```

## Architecture

### Component Libraries

The project is organized into three component libraries:

- **sparkle-duck-core**: Shared types for serialization (MaterialType, Vector2d/i, Cell, WorldData, ScenarioConfig)
- **sparkle-duck-server**: Physics engine (World + calculators), server logic, scenarios, server API commands
- **sparkle-duck-ui**: UI components (controls, rendering, LVGL builders), UI state machine, UI API commands

Executables (server, UI, CLI, tests) link against these libraries.

### Physics System

- **World**: Grid-based physics simulation with pure-material cells
- **Cell**: Fill ratio [0,1] with single material type per cell
- **MaterialType**: Enum-based material system (AIR, DIRT, LEAF, METAL, SAND, SEED, WALL, WATER, WOOD)
- **Material Properties**: Each material has density, cohesion, adhesion, viscosity, friction, elasticity

### Core Components
- **Vector2d**: 2D floating point vector class
- **Vector2i**: 2D integer vector class
- **Server::StateMachine**: Aka DirtSimStateMachine (DSSM). Headless server state machine (Idle → SimRunning ↔ SimPaused → Shutdown)
- **Ui::StateMachine**: UI client state machine (Disconnected → StartMenu → SimRunning ↔ Paused → Shutdown)
- **WorldEventGenerator**: Strategy pattern for initial world setup and dynamic particle generation
- **ScenarioRegistry**: Registry of available scenarios (owned by each StateMachine)
- **EventSink**: Interface pattern for clean event routing

### UI Framework
- **ControlPanel**: LVGL controls for scenario toggles and simulation parameters
- **CellRenderer**: Renders world state to LVGL canvases
- **NeuralGridRenderer**: Renders 15×15 tree organism perception (side-by-side with world)
- **UiComponentManager**: Manages LVGL screen and containers (50/50 split layout)
- **WebSocketClient**: Connects to DSSM server for world data
- **WebSocketServer**: Accepts remote control commands (port 7070)

### Physics Overview

- Pure material cells with fill ratios [0,1]
- Material-specific density affecting gravity response
- Center of mass (COM) physics within [-1,1] bounds
- 9 material types: AIR, DIRT, LEAF, METAL, SAND, SEED, WALL, WATER, WOOD
- Cohesion (same-material attraction) and adhesion (different-material attraction)
- Viscosity and friction (static/kinetic)
- Pressure systems: hydrostatic, dynamic, diffusion
- Air resistance
- Tree organisms with organism_id tracking

## Testing Framework

Uses GoogleTest for unit and state machine testing.

### State Machine Tests
- **StateIdle_test.cpp**: Tests Idle state event handlers (SimRun, Exit)
- **StateSimRunning_test.cpp**: Tests SimRunning state (physics, toggles, API commands)
- One happy-path test per event handler
- Tests verify state transitions, callbacks, and world state changes

### Core Tests
- **Vector2d_test.cpp**: 2D mathematics validation
- **Vector2i_test.cpp**: 2D integer vector operations
- **ResultTest.cpp**: Result<> class behavior
- **TimersTest.cpp**: Timing infrastructure

### Integration Testing
- **CLI client** can send commands to server or UI for scripted testing
- WebSocket API enables external test drivers (Python, bash scripts, etc.)
- Server and UI can be tested independently

## Performance Testing

### Benchmark Tool
The CLI tool includes a benchmark mode for measuring physics performance:

```bash
# Basic benchmark (headless server, 120 steps)
./build/bin/cli benchmark --steps 120

# Simulate UI client load (realistic with frame_ready responses)
./build/bin/cli benchmark --steps 120 --simulate-ui

# Different scenario
./build/bin/cli benchmark --scenario dam_break --steps 120
```

### Code Formatter
Run the formatter before committing.
```bash
make format
```

The benchmark auto-launches the server, runs the simulation, collects performance metrics from both server and client, then outputs JSON results including FPS, physics timing, serialization timing, and round-trip latencies.

## Coding Practices

### Serialization

**Use ReflectSerializer for automatic JSON conversion:**
```cpp
// Define aggregate struct
struct MyData {
    int x = 0;
    double y = 0.0;
    std::string name;
};

// Automatic serialization (zero boilerplate!)
MyData data{42, 3.14, "test"};
nlohmann::json j = ReflectSerializer::to_json(data);
MyData data2 = ReflectSerializer::from_json<MyData>(j);
```

ReflectSerializer uses qlibs/reflect for compile-time introspection. It works automatically with any aggregate type - no manual field listing needed. See existing usage in Cell, WorldData, Vector2d, and API command/response types.

## Development Environment

### Display Backends
Supports SDL, Wayland, X11, and Linux FBDEV backends. Primary development is Linux-focused with Wayland backend as default.

## Logging

### Log Outputs
- **Console**: INFO level and above (colored output)
- **File**: DEBUG/TRACE level and above (`sparkle-duck.log`)

### Log Levels
- **INFO**: Important events (startup, world creation, user interactions)
- **DEBUG**: Moderate frequency events (drag updates, timestep tracking)
- **TRACE**: High-frequency per-frame events (pressure systems, physics details)

### Log Files
- **File**: `sparkle-duck.log` main application
- **File**: `sparkle-duck.log` unit tests
- **Location**: Same directory as executable
- **Behavior**: File is truncated at startup for fresh logs each session

You can troubleshoot behavior by examining the TRACE logs.

## References
### Lvgl reference:
If you need to, read the docs here:
https://docs.lvgl.io/master/details/widgets/index.html

### Design docs

Can be found here:
- @design_docs/GridMechanics.md           #<-- Physics system foundations (pressure, friction, cohesion, etc.)
- design_docs/WebRTC-test-driver.md       #<-- Client/Server architecture (DSSM server + UI client)
- design_docs/coding_convention.md        #<-- Code style guidelines
- design_docs/plant.md                    #<-- Tree/organism feature (Phase 1 in progress)
- design_docs/ai-integration-ideas.md     #<-- AI/LLM integration ideas for future

## Project Directory Structure

  test-lvgl/
  ├── src/                                   # Source code
  │   ├── core/                              # Shared physics and utilities
  │   │   ├── World.{cpp,h}                  # Grid-based physics simulation
  │   │   ├── Cell.{cpp,h}                   # Pure-material cell implementation
  │   │   ├── WorldEventGenerator.{cpp,h}    # World initialization and particle generation
  │   │   ├── World*Calculator.{cpp,h}       # Physics calculators (8 files)
  │   │   ├── Vector2d.{cpp,h}               # 2D floating point vectors
  │   │   ├── ScenarioConfig.h               # Scenario configuration types
  │   │   ├── WorldData.h                    # Serializable world state
  │   │   └── api/UiUpdateEvent.h            # World update events
  │   ├── server/                            # Headless DSSM server
  │   │   ├── main.cpp                       # Server entry point
  │   │   ├── StateMachine.{cpp,h}           # Server state machine
  │   │   ├── EventProcessor.{cpp,h}         # Event queue processor
  │   │   ├── Event.h                        # Server event definitions
  │   │   ├── states/                        # Server states (Idle, SimRunning, etc.)
  │   │   ├── api/                           # API commands (CellGet, StateGet, etc.)
  │   │   ├── network/                       # WebSocket server and serializers
  │   │   ├── scenarios/                     # Scenario registry and implementations
  │   │   └── tests/                         # Server state machine tests
  │   ├── ui/                                # UI client application
  │   │   ├── main.cpp                       # UI entry point
  │   │   ├── state-machine/                 # UI state machine
  │   │   │   ├── StateMachine.{cpp,h}       # UI state machine
  │   │   │   ├── EventSink.h                # Event routing interface
  │   │   │   ├── states/                    # UI states (Disconnected, SimRunning, etc.)
  │   │   │   ├── api/                       # UI API commands (DrawDebugToggle, etc.)
  │   │   │   └── network/                   # WebSocket client and server
  │   │   ├── rendering/CellRenderer         # World rendering to LVGL
  │   │   ├── controls/ControlPanel          # UI controls (toggles, buttons)
  │   │   ├── ui_builders/LVGLBuilder        # Fluent API for LVGL widgets
  │   │   └── lib/display_backends/          # Wayland, X11, FBDEV backends
  │   ├── cli/                               # Command-line client
  │   │   ├── main.cpp                       # CLI entry point
  │   │   └── WebSocketClient                # CLI WebSocket client
  │   └── tests/                             # Core unit tests
  │       ├── Vector2d_test.cpp              # Math tests
  │       ├── ResultTest.cpp                 # Result<> tests
  │       └── TimersTest.cpp                 # Timer tests
  ├── design_docs/                           # Architecture documentation
  │   ├── GridMechanics.md                   # Physics system design
  │   ├── WebRTC-test-driver.md              # Client/Server architecture
  │   ├── coding_convention.md               # Code style guidelines
  │   └── plant.md                           # Tree/organism feature
  ├── lvgl/                                  # LVGL graphics library (submodule)
  ├── spdlog/                                # Logging library (submodule)
  └── CMakeLists.txt                         # CMake configuration


## Development Status

### Current Focus: Tree Organisms (Phase 1 Complete, Phase 2 Next)

**Completed:**
- ✅ Neural grid visualization (side-by-side UI with 15×15 tree vision)
- ✅ Efficient organism tracking (transfer-based updates)
- ✅ Basic germination (SEED → WOOD → ROOT)

**Next Steps:**
- Examine germination in detail. Update implementation and tests.
- Phase 2: Advanced growth patterns (SAPLING/MATURE stages)
- Phase 3: Resource systems (light, water, nutrients, photosynthesis)
- Performance testing and optimization

Awesome Ideas to do soon:
- Add label for Fractal showing which event type it was currently running.
- Add button to StartMenu to generate the next fractal.
- Make the water column size scale with the world size
- FIX: After resetting, the tree visualization is still showing, it should Only
be active if a tree is around.
- Add label to tree's view saying which layer it is from.
- Consider making air into a gaseous type, rather than the current "empty" behavior.
- Audit GridMechanics for correctness/relevance.  It might be getting out of date.
- Refactor PhysicsControls to normalize/DRY up the patterns? (and prevent bugs/share enhancements)

### Client/Server Architecture (DSSM + UI Client)
- ✅ Headless server with WebSocket API
- ✅ UI client with controls and rendering
- ✅ Binary serialization (zpp_bits) with custom support for 10+ field structs
- ✅ Frame-based synchronization

See design_docs/plant.md and design_docs/ai-integration-ideas.md for details.

## Interaction Guidelines
Let me know if you have any questions!
