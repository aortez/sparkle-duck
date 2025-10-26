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

#### WorldA Pressure Controls Section
- **Section Header**: "=== WorldA Pressure ==="
- **Pressure System Label**: "Pressure System:"
- **Pressure System Dropdown**: System selection (WorldA only)
  - Original (COM)
  - Top-Down Hydrostatic
  - Iterative Settling
- **Pressure Scale Slider**: Pressure strength for WorldA (0.0-10.0)

#### General Physics Controls
- **Gravity Toggle**: "Gravity: On/Off"
- **Cohesion Toggle**: "Cohesion: On/Off"
- **Cohesion Bind Strength Slider**: Bind strength (0.0-2.0)
- **Cohesion Force Toggle**: "Cohesion Force: On/Off"
- **Cohesion Strength Slider**: COM cohesion strength (0.0-300.0)
- **Left Throw Toggle**: "Left Throw: On/Off"
- **Right Throw Toggle**: "Right Throw: On/Off"
- **Quadrant Toggle**: "Quadrant: On/Off"
- **Screenshot Button**: Captures simulation image
- **Print ASCII Button**: Outputs world state to console
- **COM Cohesion Mode Label**: "COM Cohesion Mode:"
- **COM Cohesion Mode Radio Buttons**: Mode selection
  - Original (default)
  - Centering
  - Mass-Based

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
- **Rain Rate Slider**: Rain generation rate (0/s default)
- **Water Cohesion Slider**: Water cohesion strength (0.500 default)
- **Water Viscosity Slider**: Water viscosity factor (0.100 default)
- **Water Pressure Threshold Slider**: Pressure threshold (0.0004 default)
- **Water Buoyancy Slider**: Buoyancy strength (0.100 default)

#### WorldB Pressure Controls Section
- **Section Header**: "=== WorldB Pressure ==="
- **Hydrostatic Pressure Toggle**: Enable/disable hydrostatic pressure
- **Dynamic Pressure Toggle**: Enable/disable dynamic pressure accumulation
- **Pressure Diffusion Toggle**: Enable/disable pressure diffusion
- **Hydrostatic Strength Slider**: Strength of hydrostatic pressure (0.0-3.0, default 1.0)
- **Dynamic Strength Slider**: Strength of dynamic pressure (0.0-3.0, default 1.0)
- **Air Resistance Slider**: Air resistance strength (0.0-1.0, default 0.10)

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

### Pressure System Organization

The pressure controls are explicitly separated by world type to prevent confusion:

#### **WorldA Pressure Controls**
- Located in the main controls column (X=1230)
- Grouped under "=== WorldA Pressure ===" header
- Includes pressure system algorithm selection and unified pressure scale
- Only affects WorldA (RulesA) physics system

#### **WorldB Pressure Controls**  
- Located in the slider column (X=1440)
- Grouped under "=== WorldB Pressure ===" header
- Individual toggles and strength controls for hydrostatic and dynamic pressure
- Only affects WorldB (RulesB) physics system

This separation ensures users understand which controls affect which physics system, preventing confusion when switching between WorldA and WorldB.
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

### **Main Controls Column** (X=1230):
- Reset (Y=10), Pause (Y=70), Debug (Y=130)
- **WorldA Pressure Header** (Y=70)
- Pressure System dropdown (Y=115-155) - WorldA only
- Pressure Scale slider (Y=165-185) - WorldA only
- Gravity toggle (Y=285)
- Cohesion Bind toggle (Y=325)
- Cohesion Bind Strength slider (Y=380-400)
- Cohesion Force toggle (Y=415)
- Cohesion Strength slider (Y=470-490)
- COM Range slider (Y=530-550)
- Adhesion toggle (Y=590)
- Adhesion Strength slider (Y=650-670)
- Left Throw toggle (Y=690)
- Right Throw toggle (Y=750)
- Quadrant toggle (Y=810)
- Screenshot button (Y=815)
- Print ASCII button (Y=875)
- COM Cohesion Mode label (Y=930)
- COM Cohesion Mode radio buttons (Y=950)

### **Slider Column** (X=1440):
- Timescale (Y=90-110)
- Elasticity (Y=130-150)
- Dirt Fragmentation (Y=170-190)
- Cell Size (Y=210-230)
- Rain Rate (Y=250-270)
- Water Cohesion (Y=290-310)
- Water Viscosity (Y=330-350)
- Water Pressure Threshold (Y=370-390)
- Water Buoyancy (Y=410-430)
- **WorldB Pressure Header** (Y=620)
- Hydrostatic Pressure toggle (Y=645)
- Dynamic Pressure toggle (Y=675)
- Pressure Diffusion toggle (Y=705)
- Hydrostatic Strength slider (Y=745-765)
- Dynamic Strength slider (Y=795-815)
- Air Resistance slider (Y=845-865)

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
- Gravity toggle
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
- **Real-time Updates**: Mouse position tracked for particle addition
- **Coordinate Translation**: Pixel coordinates converted to grid coordinates

#### LVGLEventBuilder Pattern
The LVGLEventBuilder extends the existing LVGLBuilder pattern to integrate with the event-driven state machine architecture:

**Key Features:**
- **Type-Safe Event Generation**: UI widgets emit strongly-typed events into the event system
- **Builder Pattern Integration**: Extends existing builders with event handling methods
- **Lambda-Based Callbacks**: Clean syntax using lambdas to generate events from UI interactions

**Example Usage:**
```cpp
// Create UI elements that emit events
auto pauseBtn = LVGLEventBuilder::button(parent, eventRouter)
    .text("Pause")
    .size(100, 40)
    .onPauseResume()  // Emits PauseCommand/ResumeCommand
    .buildOrLog();

auto slider = LVGLEventBuilder::slider(parent, eventRouter)
    .label("Speed")
    .range(0, 100)
    .onTimescaleChange()  // Emits SetTimescaleCommand
    .buildOrLog();
```

**Benefits:**
- **Clean State Code**: UI creation and event routing in one fluent interface
- **No Manual Callbacks**: Eliminates boilerplate callback code and user_data casting
- **Reusable Patterns**: Common UI patterns (pause/resume, sliders) have convenience methods
- **Event System Integration**: Automatic routing to immediate or queued processing based on event type

This pattern bridges LVGL's C-style callbacks with our type-safe event system, making UI code more maintainable and testable.

### UI Lifecycle Management Strategy

#### Current Lifecycle Model
The current UI lifecycle is tightly coupled to the application lifecycle:
1. **Creation**: UI is created once at application startup by SimulationManager
2. **Lifetime**: UI persists for the entire application lifetime
3. **Destruction**: UI is destroyed only at application shutdown
4. **World Switching**: When switching physics systems, the UI updates its world pointer but isn't recreated

#### Proposed State-Based Lifecycle
In the new event-driven architecture, UI components will have dynamic lifecycles tied to state transitions:

**State-Specific UI Components:**
- **MainMenu State**: Creates `MainMenuUI` with start/config/exit buttons
- **SimRunning State**: Creates `SimulatorUI` with full simulation controls
- **SimPaused State**: Keeps existing `SimulatorUI` but may disable certain controls
- **Config State**: Creates `ConfigUI` for settings management
- **Loading/Saving States**: Creates progress dialogs or file browsers

**Lifecycle Hooks:**
```cpp
// States can optionally define these methods
void onEnter(DirtSimStateMachine& dsm);  // Create UI components
void onExit(DirtSimStateMachine& dsm);   // Destroy UI components
```

**Benefits:**
1. **Memory Efficiency**: Only UI components for current state are in memory
2. **Clean Separation**: Each state manages its own UI requirements
3. **Flexibility**: Easy to add new UI states without affecting others
4. **Testability**: UI components can be tested in isolation per state

**UI Persistence Strategy:**
For smooth transitions, some UI state needs to persist:
- **Material Selection**: Currently selected material type
- **Physics Parameters**: Slider values and toggle states
- **Camera/View State**: Zoom level, pan position (if implemented)

This data will be stored in `SharedSimState` and restored when recreating UI components.

### Integration with Physics Systems

#### Dual Physics Support
- **WorldA/RulesA**: Original mixed-material system with complex pressure physics
- **WorldB/RulesB**: New pure-material system with simplified physics (default)
- **Runtime Switching**: Can switch between systems while preserving state
- **Unified Controls**: Same UI works with both physics systems via WorldInterface

#### Material Interaction
- **Click to Add**: Mouse clicks add selected material to simulation
- **Drag System**: Click and drag to move materials

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
