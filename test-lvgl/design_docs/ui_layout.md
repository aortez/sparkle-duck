# UI Layout Design

This document describes the current UI layout of the Sparkle Duck physics simulation application.

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