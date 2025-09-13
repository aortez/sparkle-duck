Scenario System Design Overview

  Current Status: Phase 2 Complete (UI Integration & Scenarios)
  
  ✅ Completed:
  - WorldInterface now supports swapping WorldSetup strategies
  - ScenarioWorldSetup wrapper for functional scenario definitions
  - ScenarioRegistry singleton for scenario management
  - UI dropdown integrated into SimulatorUI
  - Scenario switching fully functional
  - Six scenarios implemented:
    - Empty: Truly empty world
    - Sandbox: Default world with materials
    - Raining: Water drops from sky
    - Dam Break: Classic fluid dynamics demo
    - Water Equalization: Hydrostatic pressure demo
    - Falling Dirt: Dirt particles accumulating
  - Self-registering scenario pattern
  - CMakeLists.txt updated
  - Basic unit tests
  
  🔄 Known Issues:
  - setWorldSetup() calls setup() immediately, causing world reset
  - Could benefit from deferred setup application
  
  📋 Future Enhancements:
  - Step controls in UI for frame-by-frame debugging
  - Save/restore capability for scenarios
  - Additional demo scenarios (hourglass, terrain, etc.)
  - Fix immediate setup() call in setWorldSetup()

  Core Concept

  Scenarios are reusable world configurations that leverage the existing WorldSetup pattern. Each scenario
  provides:
  - Initial setup: Configure physics, place materials, set parameters
  - Ongoing behavior: Timed events, continuous effects, dynamic changes

  Key Components

  1. ScenarioWorldSetup - Wraps setup/update functions in WorldSetup interface
  2. ScenarioRegistry - Central registry of available scenarios
  3. SimulatorUI Integration - Dropdown to select and load scenarios
  4. Scenario Library - Collection of interesting scenarios (tests, demos, sandboxes)

  Design Benefits

  - Minimal Changes: Reuses existing WorldSetup infrastructure
  - Full Power: Scenarios can do anything code can do
  - Test Integration: Easy to extract scenarios from existing tests
  - User-Friendly: Simple dropdown selection in UI

  Implementation Plan

✅ 1.1 Add WorldSetup Management to WorldInterface

  // WorldInterface.h
  virtual void setWorldSetup(std::unique_ptr<WorldSetup> setup) = 0;
  virtual WorldSetup* getWorldSetup() const = 0;
  
  COMPLETED: Added to WorldInterface.h, implemented in World.cpp and WorldB.cpp

✅ 1.2 Create ScenarioWorldSetup Class

  // ScenarioWorldSetup.h
  class ScenarioWorldSetup : public WorldSetup {
      // Wraps std::function for setup and update
  };
  
  COMPLETED: Created with support for setup, update, and reset functions

✅ 1.3 Implement ScenarioRegistry

  // ScenarioRegistry.h/cpp
  class ScenarioRegistry {
      // Singleton pattern
      // Register/retrieve scenarios
      // Manage scenario metadata
  };
  
  COMPLETED: Singleton registry with filtering by world type and category

✅ 1.4 Create Basic Scenarios

  - "Sandbox" - World populated by DefaultWorldSetup, current default behavior. ✅
  - "Empty" - Truly empty, no particles ✅
  - "raining" - Just rain. ✅
  
  COMPLETED: All three scenarios created with self-registration

  Phase 2: UI Integration

● Update Todos
  ⎿  ☐ Add scenario dropdown to SimulatorUI
     ☐ Show current scenario name in UI

● 2.1 Add Scenario Controls to SimulatorUI

  - Dropdown populated from ScenarioRegistry
  - Current scenario indicator

  Looks something like this:  
    Scenario: [Sandbox    ▼]

  2.2 Implement Switching Logic

  - Reset world
  - Apply scenario's WorldSetup

  Phase 3: Test Scenario Extraction

● 3.1 Extract Key Test Scenarios

  - Dam Break (with timed wall removal)
  - Water Equalization

  3.2 Create Demo Scenarios

  - Rain on Terrain
  - Throwing Range
  - falling dirt
  -  (rain, falling dirt, sand fallling in metal hourglass)

  Phase 4: Enhanced Features

● 4.1 Step Controls in Main UI

  - Add Step/Step10 buttons for debugging
  - Enable when paused

  4.2 Scenario Management

  - Reset current scenario
  - Save/restore capability
  - Categories for organization

  File Structure

  src/
  ├── scenarios/
  │   ├── Scenario.h              # Core types
  │   ├── ScenarioWorldSetup.h/cpp
  │   ├── ScenarioRegistry.h/cpp
  │   └── scenarios/
  │       ├── Scenario-xyz.cpp
  │       ├── Scenario-abc.cpp
  │       ├── etc.cpp
  ├── SimulatorUI.cpp             # Add dropdown/controls
  ├── WorldInterface.h            # Add setup methods
  ├── World.cpp                   # Implement setup methods
  └── WorldB.cpp                  # Implement setup methods

  Implementation Order

  1. Start with core - Get basic infrastructure working ✅
  2. Add minimal UI - Just dropdown and load button
  3. Create 2-3 scenarios - Prove the concept works ✅
  4. Iterate and expand - Add more scenarios and features

  Current Implementation Details

  Enhanced ScenarioWorldSetup Features:
  - SetupFunction: Initial world configuration
  - UpdateFunction: Per-timestep behavior (particles, events)
  - ResetFunction: Optional custom reset behavior
  - All functions use std::function for maximum flexibility

  Scenario Metadata:
  - name: Display name for UI
  - description: Tooltip/help text
  - category: Organization (test, demo, sandbox)
  - supportsWorldA/B: Compatibility flags

  Self-Registration Pattern:
  - Scenarios register themselves at program startup
  - Uses anonymous namespace with static initializer
  - No manual registration needed
