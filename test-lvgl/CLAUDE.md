# CLAUDE.md

This file provides guidance when working with code in this repository.

## Project Overview

Sparkle Duck is a playground for experimenting with Yocto, Zephyr, and LVGL technologies. The main application is a **cell-based multi-material physics simulation** located in the `test-lvgl` directory that demonstrates advanced physics simulation with interactive UI controls.

The project features a **pure-material physics system** with fill ratios and 8 material types (AIR, DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL).

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
# Run the simulation (defaults to wayland backend)
make run

# Run with specific options using ARGS
make run ARGS='-b sdl -s 100'

# Run directly with specific display backend
# RECOMMENDED: always specify steps with -s option,
# otherwise it will not automatically exit.
./build/bin/sparkle-duck -b wayland -s 100
./build/bin/sparkle-duck -b sdl -s 100

# Enable push-based UI updates (experimental, thread-safe)
./build/bin/sparkle-duck -b wayland -s 100 --push-updates
# Or use short form:
./build/bin/sparkle-duck -b x11 -s 100 -p
```

### Testing
```bash
# Run unit tests (builds debug first)
make test

# Run all tests (unit + visual)
make test-all

# Run visual tests (requires display)
make visual-tests

# Run tests with filters using ARGS
make test ARGS='--gtest_filter=World*'

# The test binary:
./build/bin/sparkle-duck-tests

# The application saves a screenshot at exit, take a look!
./build/bin/screenshot-last-exit.png
```

## Architecture

### Physics System

- **World**: Grid-based physics simulation with pure-material cells
- **Cell**: Fill ratio [0,1] with single material type per cell
- **MaterialType**: Enum-based material system (AIR, DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL)
- **Material Properties**: Each material has density, cohesion, adhesion, viscosity, friction, elasticity

### Core Components
- **Vector2d**: 2D floating point vector class
- **Vector2i**: 2D integer vector class
- **DirtSimStateMachine**: Main event-driven state machine managing simulation lifecycle
- **WorldInterface**: Abstract interface for the physics system
- **WorldEventGenerator**: Strategy pattern for initial world setup and dynamic particle generation
- **SimulatorUI**: LVGL-based interface with real-time physics controls (communicates via events)

### UI Framework
- **SimulatorUI**: LVGL-based interface with real-time physics controls
- **Interactive Elements**: Smart cell grabber with intelligent material interaction
- **Visual Controls**: Live sliders for gravity, elasticity, pressure parameters
- **Push-Based Updates**: Thread-safe UI update system (experimental, use --push-updates flag)

### Smart Cell Grabber
The intelligent interaction system provides seamless material manipulation:

**Features**:
- **Smart Interaction**: Automatically detects user intent based on cell content
- **Material Addition**: Click empty cells to add selected material type
- **Material Dragging**: Click occupied cells to grab and drag existing material
- **Floating Particle Preview**: Real-time visual feedback with particle following cursor
- **Physics-Based Tossing**: Drag velocity translates to material momentum on release
- **Collision Foundation**: Infrastructure for particle-world collision interactions

**Material Selection**:
- **8-Material Grid**: Choose from DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL, AIR
- **Visual Material Picker**: 4×2 grid layout with color-coded material buttons

**User Experience**:
- **No Mode Switching**: System intelligently handles both add and grab operations
- **Sub-Cell Precision**: Accurate center-of-mass calculation from cursor position
- **Velocity Tracking**: Recent position history enables realistic physics momentum
- **Visual Feedback**: Floating particles show exactly what's being manipulated

### Physics Features

- Pure material cells with fill ratios [0,1]
- Material-specific density affecting gravity response
- Velocity limiting (max 0.9 cells/timestep, 10% slowdown above 0.5)
- Center of mass (COM) physics within [-1,1] bounds
- 8 material types: AIR, DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL
- Cohesion (same-material attraction) and adhesion (different-material attraction)
- Viscosity and friction (static/kinetic)
- Pressure systems: hydrostatic, dynamic, diffusion
- Air resistance
- Wall boundaries with configurable behavior

## Testing Framework

Uses GoogleTest with custom visual test framework:

### Physics Tests
- **WorldVisual_test.cpp**: Material initialization, empty world advancement, material properties
- **PressureSystemVisual_test.cpp**: Pressure system validation
- **ForceCalculation_test.cpp**: Cohesion, adhesion, and force integration
- **CollisionSystem_test.cpp**: Material collision and transfer mechanics
- **HorizontalLineStability_test.cpp**: Structural stability tests

### Core Tests
- **Vector2d_test.cpp**: 2D mathematics validation
- **UI component tests**: SimulatorUI functionality

### Visual Test Framework
The project uses a unified visual test framework:
- Tests can be written in a generic way that allows them to run headless, or visually, allowing the user to run or step through the tests and see the results.
- Design doc: visual_test_framework.md
- **UnifiedSimLoopExample_test.cpp**: REFERENCE GUIDE: demonstrating best practices for writing tests with the new `runSimulationLoop()` pattern.

### Notes
Visual tests can display simulation while running when `SPARKLE_DUCK_VISUAL_TESTS=1` is enabled.

Always rebuild test binaries after modifying code - running outdated test executables will show old behavior and error messages that don't match the current source.

  Example: make -C build sparkle-duck-tests -j12 && ./build/bin/sparkle-duck-tests --gtest_filter=MyTest

## Development Environment

### Display Backends
Supports SDL, Wayland, X11, and Linux FBDEV backends. Primary development is Linux-focused with Wayland backend as default.

### Performance Tools
- Built-in timing systems and FPS tracking
- Debug visualization modes
- Screenshot capture functionality
- Memory profiling capabilities

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
- @design_docs/GridMechanics.md           #<-- For the World physics system foundations.
- design_docs/MaterialPicker-UI-Design.md #<-- Material picker UI design (IN DEVELOPMENT)
- design_docs/ui_overview.md              #<-- UI architecture and widget layout
- design_docs/WebRTC-test-driver.md       #<-- P2P API for test framework purposes.
- design_docs/*.md

## Project Directory Structure

  test-lvgl/
  ├── src/                                   # Source code
  │   ├── main.cpp                           # UI application entry point
  │   ├── main_server.cpp                    # Headless WebSocket server entry point
  │   ├── World.{cpp,h}                      # Physics system (pure materials, copyable)
  │   ├── Cell.{cpp,h}                       # Physics cell implementation
  │   ├── WorldEventGenerator.{cpp,h}        # World initialization and particle generation strategies
  │   ├── World*Calculator.{cpp,h}           # Physics calculators (8 files)
  │   ├── DirtSimStateMachine.{cpp,h}        # Main event-driven state machine
  │   ├── Event.h                            # Event definitions for state machine
  │   ├── ApiCommands.h                      # Network API command/response types
  │   ├── SimulatorUI.{cpp,h}                # LVGL-based UI (event-driven, no direct world access)
  │   ├── network/                           # Network layer
  │   │   ├── CommandDeserializerJson        # JSON → Command deserialization
  │   │   ├── ResponseSerializerJson         # Response → JSON serialization
  │   │   └── WebSocketServer                # WebSocket server implementation
  │   ├── states/                            # State machine states
  │   ├── scenarios/                         # Scenario system
  │   ├── ui/                                # UI builders and event handlers
  │   ├── lib/                               # LVGL backend support
  │   └── tests/                             # Comprehensive test suite
  ├── design_docs/                           # Architecture documentation
  │   ├── GridMechanics.md                   # World physics design
  │   ├── WebRTC-test-driver.md              # Network API architecture
  │   ├── coding_convention.md               # Code style guidelines
  │   └── *.md                               # Other design documents
  ├── scripts/                               # Build and utility scripts
  ├── lvgl/                                  # LVGL graphics library (submodule)
  ├── spdlog/                                # Logging library (submodule)
  └── CMakeLists.txt                         # CMake configuration


## Development Status

### Current Focus: WebRTC/Network API Development (In Progress)

**Completed:**
- ✅ World is fully copyable (via WorldEventGenerator cloning)
- ✅ JSON serialization for Cell and World (lossless round-trip)
- ✅ API command system with typed request/response (DirtSim::Api::* namespace pattern)
- ✅ WebSocket server infrastructure (libdatachannel integration)
- ✅ Event-driven architecture (UI ↔ StateMachine via events only)
- ✅ WorldEventGenerator refactor (formerly WorldSetup - now with instance state)
- ✅ Eliminated SimulationManager (DirtSimStateMachine owns World directly)
- ✅ UIUpdateEvent simplified (contains full World copy for rendering)

**In Progress:**
- ⏳ Create UiStateMachine (StartUp → StartMenu → SimRunning → Shutdown)
- ⏳ Complete headless server build (remove remaining UI dependencies)

**Next Steps:**
- Integrate qlibs/reflect for automatic serialization
- Python test client for network API
- WebRTC video streaming for remote UI

## Misc TODO
- How can we make denser materials sink below less dense ones?
- Add Tree organism to simulation

## Interaction Guidelines
Let me know if you have any questions!
