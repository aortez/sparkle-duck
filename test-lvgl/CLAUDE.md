# CLAUDE.md

This file provides guidance when working with code in this repository.

## Project Overview

Sparkle Duck is a playground for experimenting with Yocto, Zephyr, and LVGL technologies. The main application is a **cell-based multi-material physics simulation** located in the `test-lvgl` directory that demonstrates advanced physics simulation with interactive UI controls.

The project features a **pure-material physics system** with fill ratios and 9 material types (AIR, DIRT, LEAF, METAL, SAND, SEED, WALL, WATER, WOOD).

Read design_docs/coding_convention.md for coding guidelines.

## Essential Commands

### Git
Use git as needed but never push.
Don't use git -C, instead just make sure you're in the right directory beforehand.

#### Git Hooks
Install pre-commit hooks to automatically format code and run tests:
```bash
./hooks/install-hooks.sh
```

### Building
```bash
# Build debug version (outputs to build-debug/).
make debug

# Build optimized release version (outputs to build-release/).
make release

# Make the unit tests.
make build-tests

# Format source code.
make format

# Clean build artifacts (removes both build-debug/ and build-release/).
make clean

# Show all available targets.
make help

# Manual CMake build (if needed).
cmake -B build-debug -S . -DCMAKE_BUILD_TYPE=Debug
make -C build-debug -j12

# Note: Debug and release builds use separate directories to avoid conflicts.
# - build-debug/  - Debug builds (-O0 -g)
# - build-release/ - Release builds (-O3 optimizations)
```

### Running
```bash
# Run both client and server (debug build).
./build-debug/bin/cli run-all

# Run with optimized release build for performance testing.
./build-release/bin/cli run-all

# CLI integration test (quick, verifies ui, server, and cli).
./build-debug/bin/cli integration_test

# Clean up all sparkle-duck processes.
./build-debug/bin/cli cleanup

# Run benchmark and output results to file (use release build for accurate performance!).
./build-release/bin/cli benchmark > benchmark.json && cat benchmark.json | jq .server_fps

# Sending commands to server and ui (syntax: cli [command] [address] [params]).
./build-debug/bin/cli state_get ws://localhost:8080
./build-debug/bin/cli sim_run ws://localhost:8080 '{"timestep": 0.016, "max_steps": 1}'
./build-debug/bin/cli diagram_get ws://localhost:8080

# Run headless DSSM server (Dirt Sim State Machine).
./build-debug/bin/sparkle-duck-server -p 8080 -s 1000

# Run UI client (auto-connects to server).
./build-debug/bin/sparkle-duck-ui -b wayland --connect localhost:8080

# Run server and UI together (two terminals).
# Terminal 1:
./build-debug/bin/sparkle-duck-server -p 8080

# Terminal 2:
./build-debug/bin/sparkle-duck-ui -b wayland --connect localhost:8080

# UI options.
./build-debug/bin/sparkle-duck-ui -b wayland        # Wayland backend
./build-debug/bin/sparkle-duck-ui -b x11            # X11 backend
./build-debug/bin/sparkle-duck-ui -W 1200 -H 1200   # Custom window size
./build-debug/bin/sparkle-duck-ui -s 100            # Auto-exit after 100 steps
```

### CLI documentation
src/cli/README.md

### Testing
```bash
# Run all unit tests (uses debug build).
make test

# Run tests with filters using ARGS.
make test ARGS='--gtest_filter=State*'

# Run state machine tests directly.
./build-debug/bin/sparkle-duck-tests --gtest_filter="StateIdle*"
./build-debug/bin/sparkle-duck-tests --gtest_filter="StateSimRunning*"

# List all available tests.
./build-debug/bin/sparkle-duck-tests --gtest_list_tests

# Run specific test.
./build-debug/bin/sparkle-duck-tests --gtest_filter="StateSimRunningTest.AdvanceSimulation_StepsPhysicsAndDirtFalls"
```

### Debugging and Logging

#### Log Levels
All applications support `--log-level` flag to control logging verbosity:
```bash
# Server with debug logging
./build/bin/sparkle-duck-server --log-level debug -p 8080

# UI with trace logging
./build/bin/sparkle-duck-ui --log-level trace -b wayland

# Use with run_debug.sh
./run_debug.sh -l debug      # Debug level
./run_debug.sh -l trace      # Maximum verbosity
./run_debug.sh --log-level info  # Default

# Valid levels: trace, debug, info, warn, error, critical, off
```

#### Core Dumps for Crash Analysis
When applications crash with segmentation faults, core dumps provide invaluable debugging information.

**Locating Core Dumps:**
```bash
# List recent core dumps (systemd-coredump)
coredumpctl list | grep sparkle-duck | tail -5

# Get info about the latest crash
coredumpctl info

# Analyze with gdb
coredumpctl gdb <PID>

# Quick backtrace from most recent crash
coredumpctl gdb --batch -ex "bt" -ex "quit"
```

**Analyzing Core Dumps:**
```bash
# Get backtrace of all threads
coredumpctl gdb <PID>
(gdb) bt            # Main thread backtrace
(gdb) info threads  # List all threads
(gdb) thread apply all bt  # Backtrace of all threads
(gdb) frame 5       # Jump to specific frame
(gdb) print variable_name  # Inspect variables
(gdb) quit

# Or use batch mode for automated analysis
cat > /tmp/gdb_commands.txt << 'EOF'
set pagination off
bt
info threads
thread apply all bt
quit
EOF

coredumpctl gdb <PID> < /tmp/gdb_commands.txt > crash_analysis.txt 2>&1
```

## Architecture

### Component Libraries

The project is organized into three component libraries:

- **sparkle-duck-core**: Shared types for serialization (MaterialType, Vector2d/i, Cell, WorldData, ScenarioConfig, RenderMessage)
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

## Code Formatter
The code formatter will run via hook, but you can also run it like so:
```bash
make format
```

The benchmark auto-launches the server, runs the simulation, collects performance metrics from both server and client, then outputs JSON results including FPS, physics timing, serialization timing, round-trip latencies, and detailed timer statistics for each physics subsystem (cohesion, adhesion, pressure diffusion, etc.).

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

### Deployment to Raspberry Pi

The project includes deployment scripts for developing on a workstation and deploying to a Pi.

**Remote deploy (from dev machine):**
```bash
./deploy-to-pi.sh              # Sync, build, restart service
./deploy-to-pi.sh --no-build   # Sync and restart only
./deploy-to-pi.sh --dry-run    # Preview what would happen
```

**Local deploy (on Pi itself via SSH):**
```bash
./deploy-local.sh              # Build and restart service
./deploy-local.sh --no-build   # Restart only
```

**Setup:** See `src/scripts/deploy/README.md` for SSH key setup and configuration.

### systemd Service (Pi)

The app runs as a user service on the Pi, auto-starting on boot:
```bash
systemctl --user status sparkle-duck    # Check status
systemctl --user restart sparkle-duck   # Restart
systemctl --user stop sparkle-duck      # Stop
journalctl --user -u sparkle-duck -f    # Follow logs
```

Service file: `~/.config/systemd/user/sparkle-duck.service`

### Remote CLI Control

The app communicates with two WebSocket endpoints:
- **Port 8080** (Server): Physics simulation control, world state queries
- **Port 7070** (UI): UI state machine control, display settings

**Check if service is running:**
```bash
./build-debug/bin/cli status_get ws://dirtsim.local:7070
# Returns: {"state":"StartMenu","connected_to_server":true,"fps":0.0,...}
```

**Shutdown the remote service:**
```bash
./build-debug/bin/cli exit ws://dirtsim.local:7070
# Returns: {"success":true}
```

**Important:** To start a simulation remotely, send commands to the **UI** (port 7070), not the server:
```bash
# Start simulation (UI coordinates with server)
./build-debug/bin/cli sim_run ws://dirtsim.local:7070 '{"scenario_id": "sandbox"}'

# Query world state (server)
./build-debug/bin/cli diagram_get ws://dirtsim.local:8080
./build-debug/bin/cli state_get ws://dirtsim.local:8080
```

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
  ├── deploy-to-pi.sh                        # Remote deployment script
  ├── deploy-local.sh                        # Local deployment script
  ├── src/                                   # Source code
  │   ├── scripts/deploy/                    # Deployment tooling (Node.js)
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

### Current Focus: Tree Organisms (Phase 2 Complete, Phase 3 Next)

**Phase 2 Completed:**
- ✅ ROOT material type (grips soil, can bend)
- ✅ Continuous time system (real deltaTime, all timing in seconds)
- ✅ Contact-based germination (observe dirt 2s → ROOT 2s → WOOD 3s)
- ✅ SEED stays permanent as tree core
- ✅ TreeCommandProcessor (validates energy, adjacency, bounds)
- ✅ Adjacency validation (respects WALL/METAL/WATER boundaries)
- ✅ Balanced growth (maintains ROOT/WOOD/LEAF ratios based on water access)
- ✅ Water-seeking behavior (roots adjust target ratios when water found)
- ✅ LEAF restrictions (air-only, grows from WOOD, cardinal directions)
- ✅ Swap physics integration (organism tracking works with material swaps)
- ✅ UI displays (energy level, current thought)
- ✅ Test coverage (6 passing tests with emoji visualization)

**Known Limitations:**
- Growth only from seed position (trees grow as blobs, not realistic branching)
- No energy regeneration (trees deplete and stop)
- No MATURE stage transition

**Next Steps:**
- Fix growth topology (extend from tree edges for realistic branching)
- Add basic energy regeneration (LEAFs produce energy over time)
- Phase 3: Resource systems (light ray-casting, photosynthesis, water/nutrient absorption)
- Performance testing and optimization

Awesome Ideas to do soon:
- Swapping behavior - don't swap down if there is an opening to the side, instead displace!!!
- WorldEventGenerator methods should be moved into the Scenarios.
- FIX: After resetting, the tree visualization is still showing, it should Only
be active if a tree is around.
- Add label to tree's view saying which layer it is from.
- Consider making air into a gaseous type, rather than the current "empty" behavior.
It could affect how other things move/displace in interesting/subtle ways.  The sim speed might drop some.
- Audit GridMechanics for correctness/relevance.  It might be getting out of date.
- Refactor PhysicsControls to normalize/DRY up the patterns? (and prevent bugs/share enhancements)
- Implement fragmentation on high energy impacts (see WorldCollisionCalculator).
- Improve some of the scenarios - like the dam break and water equalization ones.
- Fractal world generator?  Or Start from fractal?
- mass as a gravity source!  allan.pizza but in a grid!!!
- quad-tree or quantization or other spatial optimization applied to the grid?
- cli send/receive any command/response automatically.
- Review CLI and CAUDE README/md files for accuracy and gross omitition  Test things
to see if they work.
- refactor World to use Pimple pattern.  Use elsewhere too?
- bit grid cache for has_support instead of storing it in each cell.
- debug and release builds in different directories, then performance testing with release builds.
- Add light tracing and illumination! (from top down)
- Per-cell neighborhood cache: 64-bit bitmap in each Cell for instant neighbor queries (see design_docs/optimization-ideas.md Section 10).

### Client/Server Architecture (DSSM + UI Client)
- ✅ Headless server with WebSocket API
- ✅ UI client with controls and rendering
- ✅ Optimized RenderMessage format (BASIC: 2 bytes/cell, DEBUG: 16 bytes/cell)
- ✅ Binary serialization (zpp_bits)
- ✅ Per-client format selection with render_format_set API

See design_docs/plant.md and design_docs/ai-integration-ideas.md for details.

## Interaction Guidelines
Let me know if you have any questions!
