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
# Main build (recommended)
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
# Run the simulation4
./run_main.sh

# Run with specific display backend (RECOMMENDED: always specify steps)
./build/bin/sparkle-duck -b wayland -s 100
./build/bin/sparkle-duck -b sdl -s 100

# Preferred method: Use Wayland backend with step limit
sparkle-duck -b wayland -s 100
```

### Testing
```bash
# Run all unit tests
./run_tests.sh

# Run visual tests (requires display)
./run_visual_tests.sh

# Enable visual mode for tests
SPARKLE_DUCK_VISUAL_TESTS=1 ./run_tests.sh
```

## Architecture

### Dual Physics Systems

#### RulesB (Default) - Pure Material System
- **WorldB**: Grid-based physics simulation using CellB
- **CellB**: Fill ratio [0,1] with pure materials (AIR, DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL)  
- **RulesBNew**: Simplified physics rules with gravity, velocity limits, and COM physics
- **MaterialType**: Enum-based material system with material-specific properties

#### RulesA (Legacy) - Mixed Material System  
- **World**: Original grid-based simulation using Cell
- **Cell**: Individual simulation units with mixed dirt/water amounts
- **WorldRules**: Complex physics with pressure systems and material mixing

### Shared Components
- **Vector2d**: 2D floating point mathematics for physics calculations
- **Vector2i**: 2D integer mathematics for physics calculations
- **SimulatorUI**: LVGL-based interface supporting both physics systems

### UI Framework
- **SimulatorUI**: LVGL-based interface with real-time physics controls
- **Interactive Elements**: Mouse/touch interaction, drag-and-drop particles
- **Visual Controls**: Live sliders for gravity, elasticity, pressure parameters

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
- design_docs/GridMechanics.md  #<-- For the WorldB system.
- design_docs/ui_overview.md  #<-- UI architecture and widget layout
- design_docs/WebRTC-test-driver.md
- design_docs/*.md

## Development Status

### Current State: WorldInterface Implementation Complete ✅
**All 7 phases of world-interface-plan.md are complete:**

1. ✅ **WorldInterface Foundation** - Abstract interface with 57 methods for dual physics systems
2. ✅ **SimulatorUI Migration** - UI uses WorldInterface polymorphically for both world types  
3. ✅ **Cell Type Strategy** - Separate Cell/CellB implementations without forced interface
4. ✅ **Testing Strategy** - 36 tests passing across both World (RulesA) and WorldB (RulesB) systems
5. ✅ **Integration & Factory** - Command-line world selection: `./sparkle-duck -w rulesA/rulesB`
6. ✅ **WorldB Rendering** - Complete pure-material rendering with MaterialType system
7. ✅ **World State Management** - Cross-world state preservation and conversion infrastructure

### Recently Completed Critical Fixes:
✅ **World Switching Crash Fix** - Fixed stale world reference causing segmentation faults
✅ **Material Density Conversion** - Fixed visual transparency issues during WorldA/B state conversion
✅ **Runtime UI Switching** - Implemented world type switch control in SimulatorUI for live RulesA ↔ RulesB switching

### Phase 7.2 Complete: Runtime UI Switching ✅
**All world switching functionality implemented:**
✅ World type switch control in SimulatorUI control panel
✅ Smooth runtime switching between RulesA ↔ RulesB without restart
✅ Visual feedback and error handling for world transitions  
✅ State preservation during runtime world type changes

### Other Completed Tasks:
[x] - Adding an interface to World ✅ (WorldInterface implemented)
[x] - Update log file to overwrite at startup ✅ 
[x] - Add a new WorldB type ✅ (Complete pure-material physics system)

### Switching Between Systems ✅ FULLY IMPLEMENTED
- **WorldA (RulesA)**: mixed dirt/water materials, complex physics, time reversal
- **WorldB (RulesB)**: pure materials (8 types), simplified physics, efficient rendering
- **Testing**: Both systems have parallel test suites with 36 tests passing
- **Command Selection**: `./sparkle-duck -w rulesA` or `./sparkle-duck -w rulesB`
- **Runtime UI Switching**: World type switch control for live switching during simulation
- **State Conversion**: Cross-world material conversion with density compensation
- **CellInterface**: Implemented for cross-compatibility between Cell/CellB systems

## Misc TODO
[x] - Make a UI design document.
[ ] - Update UI so user can select which type of material to add to the world.
[ ] - some way to talk to the application while it runs... a DBus API, a socket API, what are the other options?
[ ] - run a clean build and fix all warnings, then make all warnings into errors.