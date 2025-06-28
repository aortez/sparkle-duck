# Under Pressure

## Overview

Pressure in WorldB combines two sources:
1. **Hydrostatic Pressure**: From gravity and material weight (physics-based)
2. **Dynamic Pressure**: From blocked transfers and movement resistance (interaction-based)

This dual-source system provides both realistic gravitational pressure distribution and responsive dynamic pressure buildup.

## Core Concepts

**Hydrostatic Pressure = Weight-Based Force Distribution**
- Calculated in slices perpendicular to gravity direction
- Accumulates based on material density and gravity magnitude
- Creates realistic pressure gradients in fluid and granular materials

**Dynamic Pressure = Accumulated Transfer Resistance**
- When a cell's COM crosses a boundary but transfer is blocked
- Attempted transfer energy accumulates as pressure in target cell

