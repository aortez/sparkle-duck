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
# Build release version (optimized)
make release

# Build debug version (with symbols and LOG_DEBUG)
make debug

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
make test ARGS='--gtest_filter=WorldB*'

# The test binary:
./build/bin/sparkle-duck-tests
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
- **Vector2d**: 2D floating point vector class
- **Vector2i**: 2D integer vector class
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

### Notes
Visual tests can display simulation while running when `SPARKLE_DUCK_VISUAL_TESTS=1` is enabled.

Always rebuild test binaries after modifying code - running outdated test executables will show old behavior and error messages that don't match the current source.

  Example: make -C build sparkle-duck-tests -j12 && ./build/bin/sparkle-duck-tests --gtest_filter=MyTest

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
  ├── src/
  │   ├── main.cpp                           # Application entry point
  │   ├── Cell.{cpp,h}                       # RulesA cell implementation
  │   ├── CellB.{cpp,h}                      # RulesB cell implementation
  │   ├── CellInterface.h                    # Cell abstraction interface
  │   ├── World.{cpp,h}                      # RulesA physics system
  │   ├── WorldB.{cpp,h}                     # RulesB physics system
  │   ├── WorldInterface.{cpp,h}             # Physics/UI abstraction
  │   ├── WorldFactory.{cpp,h}               # World creation factory
  │   ├── WorldState.{cpp,h}                 # Cross-world state management
  │   ├── WorldSetup.{cpp,h}                 # World initialization utilities
  │   ├── SimulatorUI.{cpp,h}                # LVGL-based UI
  │   ├── SimulationManager.{cpp,h}          # UI/World coordinator
  │   ├── MaterialType.{cpp,h}               # Material system for WorldB
  │   ├── MaterialPicker.{cpp,h}             # Material selection UI
  │   ├── Vector2d.{cpp,h}                   # 2D floating point vector
  │   ├── Vector2i.{cpp,h}                   # 2D integer vector
  │   ├── WorldDiagramGenerator.{cpp,h}      # ASCII visualization
  │   ├── WorldInterpolationTool.{cpp,h}     # World data interpolation
  │   ├── WorldBCohesionCalculator.{cpp,h}   # Cohesion physics for WorldB
  │   ├── WorldBPressureCalculator.{cpp,h}   # WorldB Pressure calculator
  │   ├── WorldBSupportCalculator.{cpp,h}    # Support calculations for WorldB
  │   ├── CrashDumpHandler.{cpp,h}           # Debug crash handling
  │   ├── Timers.{cpp,h}                     # Performance timing
  │   ├── ScopeTimer.h                       # RAII timing utility
  │   ├── SparkleAssert.h                    # Custom assertion macros
  │   ├── lib/                               # LVGL backend support
  │   │   ├── driver_backends.{cpp,h}        # Display backend abstraction
  │   │   ├── backends.h                     # Backend interface definitions
  │   │   ├── simulator_loop.h               # Event loop utilities
  │   │   ├── simulator_settings.h           # Application settings
  │   │   ├── simulator_util.{c,h}           # Utility functions
  │   │   ├── mouse_cursor_icon.c            # Mouse cursor graphics
  │   │   └── display_backends/              # Platform-specific backends
  │   │       ├── wayland.cpp                # Wayland backend
  │   │       ├── x11.cpp                    # X11 backend
  │   │       ├── sdl.cpp                    # SDL backend
  │   │       └── fbdev.cpp                  # Linux framebuffer backend
  │   └── tests/                             # Testing framework
  │       ├── TestUI.{cpp,h}                 # Testing interface
  │       ├── visual_test_runner.{cpp,h}     # Visual test framework
  │       ├── Vector2d_test.{cpp,h}          # Vector math tests
  │       ├── Vector2i_test.cpp              # Integer vector tests
  │       ├── WorldVisual_test.cpp           # RulesA physics tests
  │       ├── WorldBVisual_test.cpp          # RulesB physics tests
  │       ├── InterfaceCompatibility_test.cpp # WorldInterface tests
  │       ├── PressureSystemVisual_test.cpp  # Pressure system tests
  │       ├── PressureSystem_test.cpp        # Pressure mechanics tests
  │       ├── PressureDynamic_test.cpp       # Dynamic pressure tests
  │       ├── PressureHydrostatic_test.cpp   # Hydrostatic pressure tests
  │       ├── DensityMechanics_test.cpp      # Density behavior tests
  │       ├── WaterPressure180_test.cpp      # Water physics tests
  │       ├── CollisionSystem_test.cpp       # Collision mechanics tests
  │       ├── COMCohesionForce_test.cpp      # Cohesion force tests
  │       ├── ForceCalculation_test.cpp      # Force computation tests
  │       ├── ForceDebug_test.cpp            # Force debugging tests
  │       ├── ForceInfluencedMovement_test.cpp # Force-based movement
  │       ├── ForcePhysicsIntegration_test.cpp # Force integration
  │       ├── HorizontalLineStability_test.cpp # Stability tests
  │       ├── DistanceToSupport_test.cpp     # Support distance tests
  │       ├── TimersTest.cpp                 # Timer system tests
  │       ├── CrashDumpHandler_test.cpp      # Crash handler tests
  │       ├── MaterialTypeJSON_test.cpp      # Material JSON tests
  │       ├── ResultTest.cpp                 # Result type tests
  │       ├── Vector2dJSON_test.cpp          # Vector JSON tests
  │       └── WorldStateJSON_test.cpp        # World state JSON tests
  ├── design_docs/                           # Architecture documentation
  │   ├── GridMechanics.md                   # RulesB physics design
  │   ├── MaterialPicker-UI-Design.md        # Material picker UI design
  │   ├── WebRTC-test-driver.md              # P2P API for test framework
  │   ├── plantA.md                          # Plant organism design
  │   ├── runtime_api_design.md              # Runtime communication API
  │   ├── ui_overview.md                     # UI architecture and layout
  │   ├── under_pressure.md                  # Pressure system design
  │   ├── visual_test_framework.md           # Visual testing framework
  │   └── web-rtc-interactivity.md           # WebRTC interactivity design
  ├── scripts/                               # Build and utility scripts
  │   ├── run_build.sh                       # Main build script
  │   ├── run_main.sh                        # Run simulation
  │   ├── run_tests.sh                       # Unit tests
  │   ├── run_visual_tests.sh                # Visual tests
  │   ├── build_debug.sh                     # Debug build
  │   ├── build_release.sh                   # Release build
  │   ├── format.sh                          # Code formatting
  │   ├── debug_latest_crash.sh              # Crash debugging
  │   ├── backend_conf.sh                    # Backend configuration
  │   ├── gen_wl_protocols.sh                # Wayland protocol generation
  │   └── wl_protocols/                      # Generated Wayland protocols
  │       ├── wayland_xdg_shell.c            # XDG shell protocol
  │       └── wayland_xdg_shell.h            # XDG shell headers
  ├── lvgl/                                  # LVGL graphics library (submodule)
  ├── spdlog/                                # Logging library (submodule)
  ├── build/                                 # Build artifacts (generated)
  ├── bin/                                   # Compiled executables (generated)
  ├── lib/                                   # Static libraries (generated)
  ├── core/                                  # Shared utilities
  │   └── Result.h                           # Error handling types
  ├── world-interface-plan.md                # WorldInterface implementation plan
  └── CMakeLists.txt                         # CMake configuration


## Development Status

### Current Focus: WorldB Physics Development ✅
**Foundation Complete** - Now enhancing WorldB to be a full-featured pure-material physics system:

**Recently Completed:**
- First pass at Cohesion and adhesion.

** Up Next:
- **1**: Refined Pressure - refer to under_pressure.md.  Verify pressure works.  Fix tests.
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

## Misc TODO
[ ] - In WorldB, Update left click, so if it is on a currently on a filled cell that is not the selected type, or is not full, fill it with the selected type.
[ ] - How can we make denser materials sink below less dense ones?
[ ] - Complete VisualTest system's UI controls for starting tests and switching tests. The test should show the initial world state, then wait for the user to press Start (run the test), Step (advance the sim forward 1 step, update display, wait for further input), or Next (skip the test).
[ ] - Collisons having effects, splash, fragmentation, etc.
[ ] - Add material-specific collision behaviors?
[ ] - chain reaction mechanics?
[ ] - CellInterface?
[ ] - some way to talk to the application while it runs... a DBus API, a socket API, what are the other options? This would be useful!

## Coding Guidelines
- Comments ALWAYS end with a period.
- Exit early.
- Use const.
- Use explicit names.
- If a comment says the same approximate thing as the explicit name right next to it, the comment is not needed.
- Look for opportunities to refactor.

## Conversation Guidelines

Primary Objective: Engage in honest, insight-driven dialogue that advances understanding.

### Core Principles 

    Intellectual honesty: Share genuine insights without unnecessary flattery or dismissiveness
    Critical engagement: Push on important considerations rather than accepting ideas at face value
    Balanced evaluation: Present both positive and negative opinions only when well-reasoned and warranted
    Directional clarity: Focus on whether ideas move us forward or lead us astray
    🖖

### What to Avoid

    Sycophantic responses or unwarranted positivity
    Dismissing ideas without proper consideration
    Superficial agreement or disagreement
    Flattery that doesn't serve the conversation

### Success Metric

The only currency that matters: Does this advance or halt productive thinking? If we're heading down an unproductive path, point it out directly.

When you contemplate the problem to understand it, you find the solution you like best.
