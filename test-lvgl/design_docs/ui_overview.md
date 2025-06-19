# UI Overview - SimulatorUI Architecture and Layout

This document provides a comprehensive overview of the user interface architecture for the Sparkle Duck physics simulation, focusing on widget organization, screen layout, and code structure.

## UI Architecture Overview

### Core UI Components

#### 1. **SimulatorUI** - Main User Interface (`src/SimulatorUI.h/.cpp`)
- **Primary UI Class**: The main interface for the physics simulation application
- **Built on LVGL**: Uses LVGL (Light and Versatile Graphics Library) for cross-platform UI rendering
- **Layout Architecture**: 
  - **Draw Area**: 850x850 pixel simulation rendering area (left side)
  - **World Type Column**: 150px wide column for physics system selection (WorldA/WorldB)
  - **Main Controls**: 200px wide column on the right side for simulation controls
  - **Total Window**: 1200x1200 pixels (configurable via command line)

#### 2. **TestUI** - Testing Interface (`src/tests/TestUI.h/.cpp`)
- **Simplified UI**: Smaller testing interface (400x400 pixel draw area)
- **Test-Specific**: Used for unit and visual tests
- **Minimal Controls**: Focused on testing functionality rather than full simulation control

### UI Integration Architecture

#### 3. **SimulationManager** - Central Coordinator (`src/SimulationManager.h`)
- **Ownership Model**: Owns both UI and physics world instances
- **Headless Support**: Can run without UI for testing/automation
- **World Switching**: Coordinates switching between WorldA (RulesA) and WorldB (RulesB) physics systems
- **State Management**: Handles cross-world state preservation during switching

#### 4. **WorldInterface** - Physics/UI Bridge (`src/WorldInterface.h`)
- **Abstraction Layer**: Provides unified API for both physics systems
- **UI Integration**: Methods for UI callbacks and parameter updates
- **Bidirectional Communication**: UI can call world methods, world can update UI

### Display Backend System

#### 5. **Display Backends** (`src/lib/`)
- **Multi-Platform Support**: Wayland, X11, SDL, FBDEV backends
- **Backend Abstraction**: `driver_backends.h` provides unified interface
- **Runtime Selection**: Choose display backend via command line (`-b` option)
- **Event Loop**: Each backend provides its own event loop integration

## Layout Overview

The UI is organized into several distinct columns from left to right:

1. **Simulation Area** (left): Interactive physics grid - the "World"
2. **World Type & Material Picker Column**: World selection and material controls
3. **Physics Controls Column**: Physics parameter toggles and settings
4. **Simulation Controls & Sliders Column**: Playback controls and parameter sliders

## Detailed Layout

### Simulation Area (Left)
- **Draw Area**: 850x850px interactive physics simulation grid
- **Mass Label**: "Total Mass: X.XX" (top-left of screen)
- **FPS Label**: "FPS: XX" (below mass label)

### World Type & Material Picker Column (Position: 860px from left)
- **World Type Label**: "World Type:"
- **World Type Buttons**: Vertical button matrix
  - WorldA (RulesA system)
  - WorldB (RulesB system - default)
- **Materials Label**: "Materials:"
- **Material Grid**: 4x2 grid of material buttons
  - Row 1: DIRT, WATER, WOOD, SAND
  - Row 2: METAL, LEAF, WALL, AIR

### Physics Controls Column (Position: 1230px from left)
- **Debug Toggle**: "Debug: Off/On"
- **Pressure System Label**: "Pressure System:"
- **Pressure System Dropdown**: System selection
  - Original (COM)
  - Top-Down Hydrostatic
  - Iterative Settling
- **Force Toggle**: "Force: Off/On"
- **Gravity Toggle**: "Gravity: On/Off"
- **Cohesion Toggle**: "Cohesion: On/Off"
- **Left Throw Toggle**: "Left Throw: On/Off"
- **Right Throw Toggle**: "Right Throw: On/Off"
- **Quadrant Toggle**: "Quadrant: On/Off"
- **Screenshot Button**: Captures simulation image
- **Print ASCII Button**: Outputs world state to console

### Simulation Controls & Sliders Column (Position: 1440px from left)

#### Simulation Controls (Top Section)
- **Pause/Resume Button**: Toggles simulation pause/resume
- **Reset Button**: Resets simulation to initial state
- **Time History Toggle**: "Time History: On/Off"
- **Backward/Forward Buttons**: Time navigation controls (side-by-side)

#### Physics Parameter Sliders (Below Controls)
- **Timescale Slider**: Simulation speed control (1.0x default)
- **Elasticity Slider**: Material elasticity (0.8 default)
- **Dirt Fragmentation Slider**: Fragmentation factor (0.00 default)
- **Cell Size Slider**: Grid cell size (50 default)
- **Pressure Scale Slider**: Pressure system scaling (10 default)
- **Rain Rate Slider**: Rain generation rate (0/s default)
- **Water Cohesion Slider**: Water cohesion strength (0.500 default)
- **Water Viscosity Slider**: Water viscosity factor (0.100 default)
- **Water Pressure Threshold Slider**: Pressure threshold (0.0004 default)
- **Water Buoyancy Slider**: Buoyancy strength (0.100 default)

### Bottom Right
- **Quit Button**: Red button to exit application

## Layout Constants

- `DRAW_AREA_SIZE`: 850px
- `CONTROL_WIDTH`: 200px
- `WORLD_TYPE_COLUMN_WIDTH`: 150px
- `WORLD_TYPE_COLUMN_X`: 860px
- `MAIN_CONTROLS_X`: 1230px
- `SLIDER_COLUMN_X`: 1440px

## Design Rationale

The layout follows a logical left-to-right flow:
1. **Simulation** (primary focus)
2. **Setup Controls** (world type and material selection)
3. **Physics Toggles** (runtime physics parameter switches)
4. **Simulation & Parameter Controls** (playback controls and precise parameter adjustment)

This organization groups related functionality and provides clear visual separation between different types of controls. Simulation controls (pause/reset) are positioned in the rightmost column for easy access during operation. Physics parameter toggles are separated from continuous parameter sliders to distinguish between discrete on/off settings and graduated adjustments.
## Widget Organization in Code

The UI initialization follows this order in `SimulatorUI::initialize()`:
1. `createDrawArea()` - Physics rendering surface
2. `createLabels()` - Status displays (mass, FPS)
3. `createWorldTypeColumn()` - Physics system selector
4. `createControlButtons()` - Action buttons
5. `createSliders()` - Parameter controls
6. `setupDrawAreaEvents()` - Mouse interaction

## Main Controls Column - Vertical Layout

### **Status Labels** (Y=10-40):
- Mass: "Total Mass: 0.00" at Y=10
- FPS: "FPS: 0" at Y=40

### **Action Buttons** (Y=10-635, 50px height each):
- Reset (Y=10), Pause (Y=70), Debug (Y=130)
- Pressure System dropdown (Y=190-210)
- Force toggle (Y=260), Gravity toggle (Y=310)
- Left/Right Throw toggles (Y=370, Y=430)
- Quadrant toggle (Y=490), Screenshot (Y=550)
- Time History toggle (Y=600)
- Backward/Forward buttons (Y=635, split width)

### **Parameter Sliders** (Y=670-1050, 40px spacing):
- Timescale (Y=670-690)
- Elasticity (Y=710-730)
- Dirt Fragmentation (Y=750-770)
- Cell Size (Y=790-810)
- Pressure Scale (Y=830-850)
- Rain Rate (Y=870-890)
- Water Cohesion (Y=910-930)
- Water Viscosity (Y=950-970)
- Water Pressure Threshold (Y=990-1010)
- Water Buoyancy (Y=1030-1050)

### **Special Elements**:
- Quit button: Bottom-right corner with red background
- Value labels: Right-aligned next to sliders showing current values

## Key Architectural Features

### **Callback System**:
- Uses `CallbackData` struct containing UI, World, and Manager pointers
- Static callback functions with event-driven updates
- Managed storage prevents memory leaks

### **World Integration**:
- WorldInterface abstraction supports both RulesA and RulesB physics
- SimulationManager coordinates world switching
- Real-time parameter updates via world interface methods

### **Interactive Elements**:
- Mouse events: PRESSED, PRESSING, RELEASED for particle manipulation
- Coordinate translation: Screen pixels to grid coordinates
- Force application and drag system for material interaction

## UI Control Structure

### Interactive Elements (in SimulatorUI)

**World Type Selection:**
- Button matrix for switching between WorldA (mixed materials) and WorldB (pure materials)
- Real-time physics system switching without restart

**Simulation Controls:**
- Reset, Pause/Resume, Debug toggle
- Pressure system dropdown (Original/Top-Down/Iterative)
- Force and gravity toggles
- Screenshot capture functionality

**Physics Parameter Sliders:**
- Timescale
- Elasticity
- Dirt fragmentation
- Cell size (affects grid resolution)
- Pressure scale
- Rain rate
- Water physics parameters (cohesion, viscosity, pressure threshold, buoyancy)

**Advanced Features:**
- Time reversal controls (<<, >>, history toggle)
- World setup controls (left/right throw, quadrant controls)
- Real-time FPS and mass display

### Event Handling System

#### Callback Architecture
- **Static Callbacks**: Event handlers are static methods in SimulatorUI
- **CallbackData Structure**: Carries UI, world, and manager pointers to callbacks
- **Managed Storage**: Callbacks stored in `callback_data_storage_` for memory management

#### Mouse/Touch Interaction
- **Draw Area Events**: Click, drag, and release for material placement
- **Real-time Updates**: Mouse position tracked for particle addition and cursor force
- **Coordinate Translation**: Pixel coordinates converted to grid coordinates

### Integration with Physics Systems

#### Dual Physics Support
- **WorldA/RulesA**: Original mixed-material system with complex pressure physics
- **WorldB/RulesB**: New pure-material system with simplified physics (default)
- **Runtime Switching**: Can switch between systems while preserving state
- **Unified Controls**: Same UI works with both physics systems via WorldInterface

#### Material Interaction
- **Click to Add**: Mouse clicks add water/dirt to simulation
- **Drag System**: Click and drag to move materials
- **Force Application**: Optional cursor force for particle manipulation

## Notable Features

### Screenshot System
- **PNG Export**: Uses lodepng for screenshot capture
- **Timestamped Files**: Automatic filename generation with ISO8601 timestamps
- **Exit Screenshots**: Automatically captures state on application exit

### Logging Integration
- **spdlog Integration**: Structured logging with console and file output
- **Debug Information**: UI events logged at appropriate levels
- **Performance Tracking**: Timer statistics and FPS monitoring

### Configuration System
- **Command Line Options**: Window size, backend selection, step limits, world type
- **Environment Variables**: `LV_SIM_WINDOW_WIDTH`, `LV_SIM_WINDOW_HEIGHT`
- **Responsive Layout**: UI adapts to different window sizes

The UI architecture is intended to be a well-structured separation of concerns with the SimulationManager coordinating between physics and presentation layers, enabling both interactive use and automated testing while supporting multiple physics systems and display platforms.
