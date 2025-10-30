# UI Overview - SimulatorUI Architecture and Layout

This document provides a comprehensive overview of the user interface architecture for the Sparkle Duck physics simulation, focusing on widget organization, screen layout, and code structure.

## UI Architecture Overview

### Core UI Components

#### 1. **SimulatorUI** - Main User Interface (`src/SimulatorUI.h/.cpp`)
- **Primary UI Class**: The main interface for the physics simulation application
- **Built on LVGL**: Uses LVGL (Light and Versatile Graphics Library) for cross-platform UI rendering
- **Layout Architecture**:
  - **Draw Area**: 850x850 pixel simulation rendering area (left side)
  - **Left Column**: 200px wide - Scenario selector and material picker
  - **Right Column**: 200px wide - Simulation controls and physics parameters
  - **Slider Column**: Additional column for parameter sliders
  - **Total Window**: Approximately 1400x900 pixels (configurable via command line)

#### 2. **TestUI** - Testing Interface (`src/tests/TestUI.h/.cpp`)
- **Simplified UI**: Smaller testing interface (400x400 pixel draw area)
- **Test-Specific**: Used for unit and visual tests
- **Minimal Controls**: Focused on testing functionality rather than full simulation control

### UI Integration Architecture

#### 3. **SimulationManager** - Central Coordinator (`src/SimulationManager.h`)
- **Ownership Model**: Owns both UI and physics world instances
- **Headless Support**: Can run without UI for testing/automation
- **State Management**: Handles world resizing and scenario switching

#### 4. **WorldInterface** - Physics/UI Bridge (`src/WorldInterface.h`)
- **Abstraction Layer**: Provides unified API for physics system
- **UI Integration**: Methods for UI callbacks and parameter updates
- **Bidirectional Communication**: UI can call world methods, world can update UI
- **Headless-Friendly**: Draw operations take draw area as parameter (not stored)

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

## Control Organization

### Simulation Area (Left Column)
- **Draw Area**: Interactive physics grid
- **Status Labels**: Total mass and FPS display

### Left Column (Center-Left)
- **Scenario Selector**: Dropdown for world setup scenarios (Sandbox, Dam Break, etc.)
- **Material Grid**: 8 material buttons (DIRT, WATER, WOOD, SAND, METAL, LEAF, WALL, AIR)

### Right Column (Center-Right)
Contains physics parameters and world setup controls organized into sections:

#### Physics Force Controls
- **Gravity Slider**: Gravity strength and direction
- **Viscosity Slider**: Global viscosity multiplier
- **Cohesion Force Toggle Slider**: Attractive forces between same-material particles
- **Cohesion Range Slider**: Neighbor detection distance
- **Friction Toggle Slider**: Static/kinetic friction system
- **Adhesion Toggle Slider**: Attractive forces between different materials

#### World Setup Switches
- **Left Throw**: Particle generation from left side (disabled by default)
- **Right Throw**: Particle generation from right side (enabled by default)
- **Quadrant**: Lower-right dirt pile (enabled by default)
- **Water Column**: Left-side water reservoir (enabled by default)

#### Utility Buttons
- **Screenshot**: Capture current state
- **Print ASCII**: Output text representation

### Simulation Controls Column (Right)

#### Playback Controls
- **Pause/Resume Button**: Simulation control
- **Reset Button**: Return to initial state
- **Time History Controls**: Step backward/forward through history

#### Simulation Parameters
- **Timescale Slider**: Simulation speed
- **Elasticity Slider**: Collision bounce factor
- **Rain Rate Slider**: Particle generation rate

#### Pressure Controls
- **Hydrostatic Pressure Switch**: Gravitational pressure accumulation
- **Dynamic Pressure Switch**: Impact/movement pressure
- **Pressure Diffusion Switch**: Pressure propagation
- **Pressure Strength Sliders**: Hydrostatic and dynamic multipliers
- **Air Resistance Slider**: Velocity damping

### Bottom Right
- **Quit Button**: Red button to exit application

## Design Rationale

The layout follows a logical left-to-right flow:
1. **Simulation** (primary focus)
2. **Setup Controls** (world type and material selection)
3. **Physics Controls** (force parameters and world setup)
4. **Simulation Parameters** (playback controls and continuous adjustments)

This organization groups related functionality and provides clear visual separation. Physics force controls use toggle sliders that combine enable/disable with strength adjustment. World setup controls use simple switches for immediate add/remove behavior.

### Pressure System Organization

Pressure system features three modes:
- **Hydrostatic**: Gravitational pressure accumulation (weight of material above)
- **Dynamic**: Impact and movement pressure (collisions and transfers)
- **Diffusion**: Pressure propagation through materials

Each can be toggled independently with separate strength multipliers.

## Widget Organization in Code

The UI initialization follows this order in `SimulatorUI::initialize()`:
1. `createDrawArea()` - Physics rendering surface
2. `createLabels()` - Status displays
3. `createScenarioDropdown()` - World setup selector
4. `createMaterialPicker()` - Material selection grid
5. `createControlButtons()` - Physics controls and world setup
6. `createSliders()` - Parameter sliders
6. `setupDrawAreaEvents()` - Mouse interaction

## Key Architectural Features

### **Callback System**:
- Uses `CallbackData` struct containing UI, World, and Manager pointers
- Event-driven updates through EventRouter
- Managed storage prevents memory leaks

### **World Integration**:
- WorldInterface abstraction supports both RulesA and RulesB physics
- SimulationManager coordinates world switching
- Real-time parameter updates via world interface methods

### **Interactive Elements**:
- Mouse events: PRESSED, PRESSING, RELEASED for particle manipulation
- Coordinate translation: Screen pixels to grid coordinates
- Force application and drag system for material interaction

## Event Handling System

### Mouse/Touch Interaction
- **Draw Area Events**: Click, drag, and release for material manipulation
- **Smart Cell Grabber**: Intelligently detects whether to add material or grab existing material
- **Coordinate Translation**: Pixel coordinates converted to grid coordinates
- **Velocity Tracking**: Drag velocity transferred to material momentum

### LVGLEventBuilder Pattern

Extends LVGL builder pattern to integrate with the event-driven state machine:

**Key Features:**
- **Type-Safe Event Generation**: UI widgets emit strongly-typed events
- **Builder Pattern**: Fluent interface for UI creation with event routing
- **Convenience Methods**: Common patterns (pause/resume, sliders) pre-configured

This pattern bridges LVGL's C-style callbacks with the type-safe event system.

### UI Component Patterns

#### ToggleSlider Component
Combines toggle switch with slider in a compact, bordered container:
- **State Management**: Saves last value when toggled off, restores when toggled on
- **Single Model Variable**: Numeric value only (0 = disabled)
- **Visual Feedback**: Slider grays out and indicator turns grey when disabled
- **Used For**: Cohesion Force, Friction, Adhesion

#### LabeledSwitch Component
Label + switch with automatic vertical centering:
- **Compact**: Single row, vertically aligned
- **Centered**: Switch positioned to align with label text baseline
- **Used For**: World setup controls (Quadrant, Water Column, Left/Right Throw), Pressure toggles

### UI Lifecycle Management

The UI is created at application startup and persists for the application lifetime. The UI manages a single World instance through the WorldInterface abstraction. State is managed through the DirtSimStateMachine and SharedSimState systems for thread-safe updates.

### Integration with Physics System

The UI interacts with the World through the WorldInterface abstraction layer. All physics controls update world parameters in real-time. The interface supports both headless (testing/automation) and graphical (interactive) operation.

Material interaction supports both addition (click) and manipulation (drag) modes, with the Smart Cell Grabber intelligently detecting user intent based on cell contents.

## Notable Features

- **Screenshot System**: PNG export with automatic timestamping and exit capture
- **Logging**: Structured logging via spdlog for debugging and performance tracking
- **Configuration**: Command-line and environment variable support for customization
- **Real-time Updates**: Immediate visual feedback for all control changes

The UI architecture provides a well-structured separation of concerns with the SimulationManager coordinating between physics and presentation layers, enabling both interactive use and automated testing.
