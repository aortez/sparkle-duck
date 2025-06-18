# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Sparkle Duck is a playground for experimenting with Yocto, Zephyr, and LVGL technologies. The main application is a **cell-based multi-material physics simulation** located in the `test-lvgl` directory that demonstrates advanced physics simulation with interactive UI controls.

The project features **dual physics systems**:
- **RulesA**: Original dirt/water simulation with mixed materials per cell
- **RulesB**: New pure-material system with fill ratios and enhanced material types (default)

## Essential Commands

### Building
```bash
# Main build
./run_build.sh

# Debug/release variants
./build_debug.sh
./build_release.sh

# Manual CMake build
cmake -B build -S .
make -C build -j12
```

### Running
```bash
# Run the simulation
./run_main.sh

# Run with specific display backend.
# RECOMMENDED: always specify steps with -s option,
# otherwise it will not automatically exit.
./build/bin/sparkle-duck -b wayland -s 100
./build/bin/sparkle-duck -b sdl -s 100
```

### Testing
```bash
# Run all unit tests
./run_tests.sh

# Run visual tests (requires display)
./run_visual_tests.sh

# Enable visual mode for tests manually.
SPARKLE_DUCK_VISUAL_TESTS=1 ./run_tests.sh
```

## Architecture

### Dual Physics Systems

#### RulesB (Default) - Pure Material System
- **WorldB**: Grid-based physics simulation using CellB
- **CellB**: Fill ratio [0,1] with pure materials (AIR, DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL)  
- **MaterialType**: Enum-based material system with material-specific properties

#### RulesA (Legacy) - Mixed Material System  
- **World**: Original grid-based simulation using Cell (should be renamed WorldA)
- **Cell**: Individual simulation units with mixed dirt/water amounts (should be renamed CellA)
- **WorldRules**: Complex physics with pressure systems and material mixing

### Shared Components
- **Vector2d**: 2D floating point mathematics for physics calculations
- **Vector2i**: 2D integer mathematics for physics calculations
- **SimulatorUI**: LVGL-based interface supporting both physics systems

### UI Framework
- **SimulatorUI**: LVGL-based interface with real-time physics controls
- **Interactive Elements**: Smart cell grabber with intelligent material interaction
- **Visual Controls**: Live sliders for gravity, elasticity, pressure parameters

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
- **Cross-World Support**: Material selection works with both WorldA and WorldB

**User Experience**:
- **No Mode Switching**: System intelligently handles both add and grab operations
- **Sub-Cell Precision**: Accurate center-of-mass calculation from cursor position
- **Velocity Tracking**: Recent position history enables realistic physics momentum
- **Visual Feedback**: Floating particles show exactly what's being manipulated

### Physics Features

#### RulesB (Current Default)
- Pure material cells with fill ratios [0,1] 
- Material-specific density affecting gravity response
- Velocity limiting (max 0.9 cells/timestep, 10% slowdown above 0.5)
- Center of mass (COM) physics within [-1,1] bounds
- 8 material types: AIR, DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL
- Wall boundaries with material reflection (planned)
- Transfer/pressure systems (in development)

#### RulesA (Legacy)
- Mixed dirt/water materials per cell
- Complex pressure systems (COM-based, hydrostatic, iterative settling)
- Density-based material swapping and realistic material behavior  
- Time reversal capability (step backward/forward)
- Fragmentation and advanced physics parameters

## Testing Framework

Uses GoogleTest with custom visual test framework supporting both physics systems:

### RulesB Tests
- **WorldBVisual_test.cpp**: Material initialization, empty world advancement, material properties
- Basic physics validation for the new pure-material system

### RulesA Tests  
- **WorldVisual_test.cpp**: Complex physics scenarios (momentum transfer, boundary reflection)
- **PressureSystemVisual_test.cpp**: Pressure system validation
- **DensityMechanics_test.cpp**: Material density and swapping mechanics

### Shared Tests
- **Vector2d_test.cpp**: 2D mathematics validation
- **UI component tests**: SimulatorUI functionality

Visual tests can display simulation while running when `SPARKLE_DUCK_VISUAL_TESTS=1` is enabled.

## Development Environment

### Display Backends
Supports SDL, Wayland, X11, and Linux FBDEV backends. Primary development is Linux-focused with Wayland backend as default.

### IDE Integration
- ClangD configuration (`.clangd`)
- Generated `compile_commands.json` for LSP support
- AI development setup (`.aiignore`)

### Performance Tools
- Built-in timing systems and FPS tracking
- Debug visualization modes
- Screenshot capture functionality
- Memory profiling capabilities

## Logging

The application uses spdlog for structured logging with dual output:

### Log Outputs
- **Console**: INFO level and above (colored output)
- **File**: DEBUG/TRACE level and above (`sparkle-duck.log`)

### Log Levels
- **INFO**: Important events (startup, world creation, user interactions)
- **DEBUG**: Moderate frequency events (drag updates, timestep tracking)
- **TRACE**: High-frequency per-frame events (pressure systems, physics details)

### Log Files
- **File**: `sparkle-duck.log` (overwrites on each run)
- **Location**: Same directory as executable
- **Behavior**: File is truncated at startup for fresh logs each session

You can troubleshoot behavior by examining the TRACE logs.

## References
### Lvgl reference:
If you need to, read the docs here:
https://docs.lvgl.io/master/details/widgets/index.html

### Design docs

Can be found here:
- design_docs/GridMechanics.md            #<-- For the WorldB system foundations.
- design_docs/WorldB-Development-Plan.md  #<-- Complete WorldB development roadmap.
- design_docs/MaterialPicker-UI-Design.md #<-- Material picker UI design (IN DEVELOPMENT)
- design_docs/ui_overview.md              #<-- UI architecture and widget layout
- design_docs/WebRTC-test-driver.md       #<-- P2P API for test framework purposes.
- design_docs/*.md

## Project Directory Structure

  test-lvgl/
  ├── src/                                    # Main source code
  │   ├── main.cpp                           # Application entry point
  │   ├── Cell.{cpp,h}                       # RulesA cell implementation
  │   ├── CellB.{cpp,h}                      # RulesB cell implementation
  │   ├── World.{cpp,h}                      # RulesA physics system
  │   ├── WorldB.{cpp,h}                     # RulesB physics system
  │   ├── SimulatorUI.{cpp,h}                # LVGL-based UI
  │   ├── MaterialType.{cpp,h}               # Material system
  │   ├── Vector2{d,i}.{cpp,h}               # 2D math utilities
  │   ├── SimulationManager.{cpp,h}          # World switching logic
  │   ├── WorldFactory.{cpp,h}               # World creation
  │   ├── MaterialPicker.{cpp,h}             # Material selection UI
  │   ├── CrashDumpHandler.{cpp,h}           # Debug crash dumps
  │   ├── Timers.{cpp,h}                     # Performance timing
  │   ├── WorldCohesionCalculator.{cpp,h}    # Cohesion physics
  │   ├── WorldDiagramGenerator.{cpp,h}      # ASCII visualization
  │   ├── lib/                               # LVGL backend support
  │   │   ├── display_backends/              # SDL, Wayland, X11, FBDEV
  │   │   └── driver_backends.{cpp,h}        # Backend management
  │   └── tests/                             # Unit and visual tests
  │       ├── WorldBVisual_test.cpp          # RulesB physics tests
  │       ├── WorldVisual_test.cpp           # RulesA physics tests
  │       ├── Vector2d_test.cpp              # Math tests
  │       └── visual_test_runner.{cpp,h}     # Test framework
  ├── design_docs/                           # Architecture documentation
  │   ├── GridMechanics.md                   # RulesB physics design
  │   ├── WorldB-Development-Plan.md         # Development roadmap
  │   ├── ui_overview.md                     # UI architecture
  │   └── under_pressure.md                  # Pressure system design
  ├── lvgl/                                  # LVGL graphics library
  ├── spdlog/                                # Logging library
  ├── scripts/                               # Build and utility scripts
  │   ├── run_build.sh                       # Main build script
  │   ├── run_main.sh                        # Run simulation
  │   ├── run_tests.sh                       # Unit tests
  │   └── run_visual_tests.sh                # Visual tests
  ├── build/                                 # Build artifacts
  ├── bin/                                   # Compiled executables
  ├── CMakeLists.txt                         # CMake configuration
  ├── CLAUDE.md                              # AI assistant instructions
  └── core/                                  # Shared utilities
      └── Result.h                           # Error handling

## Development Status

### Current Focus: WorldB Physics Development ✅
**Foundation Complete** - Now enhancing WorldB to be a full-featured pure-material physics system:

**Recently Completed:**
- First pass at Cohesion and adhesion.

** Up Next:
- **1**: Add Pressure - refer to under_pressure.md
- **2**: Add Tree organism to simulation.

### Architecture Status
- **WorldA (RulesA)**: Mixed dirt/water materials, complex physics, time reversal ✅  
- **WorldB (RulesB)**: Pure materials (8 types), smart grabber, collision foundation ✅
- **SimulationManager**: Handles world switching and ownership ✅
- **WorldInterface**: Unified API for both physics systems ✅
- **Smart Cell Grabber**: Intelligent material interaction with floating particle system ✅

### Collision Implementation Todos
- [x] Implement elastic collision handler for METAL-METAL interactions
- [ ] Add splash effect system for WATER collisions
- [ ] Create fragmentation mechanics for brittle materials
- [x] Design absorption/penetration behaviors (WATER+DIRT, etc.)
- [ ] Implement absorption/penetration behaviors (WATER+DIRT, etc.)

## Misc TODO
[ ] - Add a Makefile to capture some common targets ('clean', 'debug', 'release', 'test-all'... anything else?); update CLAUDE.md with instructions.
[ ] - some way to talk to the application while it runs... a DBus API, a socket API, what are the other options? This would be useful!
[ ] - In WorldB, Update left click, so if it is on a currently on a filled cell that is not the selected type, or is not full, fill it with the selected type.
[ ] - How can we make denser materials sink below less dense ones?
[ ] - Complete VisualTest system's UI controls for starting tests and switching tests. The test should show the initial world state, then wait for the user to press Start (run the test), Step (advance the sim forward 1 step, update display, wait for further input), or Next (skip the test).
[ ] - Collisons having effects, splash, fragmentation, etc.
[ ] - Add material-specific collision behaviors?
[ ] - chain reaction mechanics?
[ ] - CellInterface?