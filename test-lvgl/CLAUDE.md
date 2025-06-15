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
- design_docs/GridMechanics.md          #<-- For the WorldB system foundations.
- design_docs/WorldB-Development-Plan.md #<-- Complete WorldB development roadmap.
- design_docs/MaterialPicker-UI-Design.md #<-- Material picker UI design (IN DEVELOPMENT)
- design_docs/ui_overview.md            #<-- UI architecture and widget layout
- design_docs/WebRTC-test-driver.md     #<-- P2P API for test framework purposes.
- design_docs/*.md

## Development Status

### Current Focus: WorldB Advanced Development ✅
**Foundation Complete** - Now enhancing WorldB as a full-featured pure-material physics system:

**Recently Completed:**
✅ **WorldInterface Implementation** - Complete dual physics system architecture  
✅ **Runtime World Switching** - Live WorldA ↔ WorldB transitions with UI switch control
✅ **Material Density Conversion** - Fixed visual consistency during world switching
✅ **Crash Dump Feature** - Completed feature for capturing system state during unexpected termination

**Current Priority (Phase 1):**
🔄 **Complete Material Rendering** - Visual implementation for all 8 material types (DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL, AIR)
⚙️ **Material Picker UI** - Allow users to select and place any material type (IN DEVELOPMENT - seems complete)
🔄 **Cell Grabber Tool** - Interactive cell manipulation and movement system

**Next Phases:**
- **Phase 2**: More physics features for WorldB.
- **Phase 3**: Add Tree organism to simulation.
- **Phase 4**: Simulation scenarios, analysis tools, performance optimization.

**Reference**: See `design_docs/WorldB-Development-Plan.md` for complete roadmap

### Architecture Status
- **WorldA (RulesA)**: Mixed dirt/water materials, complex physics, time reversal ✅  
- **WorldB (RulesB)**: Pure materials (8 types), simplified physics, enhanced rendering 🔄
- **SimulationManager**: Handles world switching and ownership ✅
- **WorldInterface**: Unified API for both physics systems ✅

## Misc TODO
🔄 - Update UI so user can select which type of material to add to the world. (Phase 1)
[ ] - Add an exit hook that shows a full dump of the world state after an ASSERT.
[ ] - some way to talk to the application while it runs... a DBus API, a socket API, what are the other options? This would be useful!
[ ] - run a clean build and fix all warnings, then make all warnings into errors. Please.