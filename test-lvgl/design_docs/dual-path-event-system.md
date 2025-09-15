# Dual-Path Event System Design for Dirt Sim

## Introduction

This document describes the design and implementation plan for introducing a flexible state machine and dual-path event handling system to the Dirt Sim application. Currently, the application uses a hard-coded flow with direct UI callbacks. This design will transform it into a robust event-driven architecture with both immediate (synchronous) and queued (asynchronous) event processing.

## Current State

The application currently:
- Uses a hard-coded main loop in the display backend
- Handles all UI events synchronously via LVGL callbacks
- Has no event queue or state machine
- Passes UI/World/Manager pointers through callback data structures
- Manages UI lifecycle directly in SimulationManager

## Why?

### Current Limitations
- No event queue exists - all UI callbacks directly modify state
- Hard-coded application flow limits flexibility
- No clear separation between UI events and simulation state changes
- Difficult to add new features like save/load, benchmarking, or demos
- UI management is tightly coupled to simulation logic

### Benefits of Event-Driven Architecture
- **Flexible State Management**: Easy to add new states (demos, benchmarking, config screens)
- **Clean UI Lifecycle**: UI components created/destroyed based on state transitions
- **Low Latency**: Immediate responses for time-critical operations (FPS queries, pause/resume)
- **Ordered Processing**: Complex simulation state transitions remain sequential and predictable
- **Testability**: States and events can be tested in isolation
- **Extensibility**: New features can be added as new states without disrupting existing flow

## How

### Architecture Overview
```
UI/Input Thread                      Simulation Thread
    |                                        |
    ├─→ Immediate Events ──────┐            |
    |   (processEvent)         ↓            |
    |                    Shared State       |
    |                    (Protected)        |
    |                          ↑            |
    └─→ Queued Events ─────────┴────→ Event Queue → processEventsFromQueue
        (queueEvent)
```

### States

The DirtSimStateMachine manages these primary states:

```cpp
// File: src/states/SimStates.h
namespace DirtSim::State {
    struct Startup {};      // Initial state, loading resources
    struct MainMenu {};     // Main menu UI
    struct SimRunning {};   // Active simulation
    struct SimPaused {};    // Simulation paused, UI active
    struct UnitTesting {};  // Running unit tests
    struct Benchmarking {}; // Performance testing mode
    struct Loading {};      // Loading saved simulation
    struct Saving {};       // Saving current state
    struct Config {};       // Configuration/settings
    struct Demo {};         // Demo mode
    struct Shutdown {};     // Cleanup and exit
    
    using Any = std::variant<Startup, MainMenu, SimRunning, SimPaused, 
                            UnitTesting, Benchmarking, Loading, Saving, 
                            Config, Demo, Shutdown>;
}
```

### Event Categories

**Immediate Events** (UI/Input thread):
- SetGravityCommand (physics parameter update)
- SetTimescaleCommand (simulation speed control)
- SetElasticityCommand (physics parameter update)
- ToggleDebugCommand (UI state change)

**Queued Events** (Simulation thread):
- All state transition events (StartSimulationCommand, QuitApplicationCommand, etc.)
- PauseCommand and ResumeCommand (trigger state transitions)
- SelectMaterialCommand (updates shared state)
- ResetSimulationCommand (major state change)
- AdvanceSimulationCommand (simulation step)
- MouseDownEvent, MouseMoveEvent, MouseUpEvent (user interactions)
- CaptureScreenshotCommand (file I/O operation)

### LVGL Callback to Event Mapping

Each event type includes a static `name()` method for identification:

```cpp
struct PauseCommand {
    static constexpr const char* name() { return "PauseCommand"; }
};
```

**Current Callback Mappings:**

| LVGL Callback | Event Type | Processing |
|---------------|------------|------------|
| Timer-based updates | `GetFPSCommand`, `GetSimStatsCommand` | Immediate |
| `pauseBtnEventCb` | `PauseCommand`/`ResumeCommand` | Immediate |
| `resetBtnEventCb` | `ResetSimulationCommand` | Queued |
| `worldTypeButtonMatrixEventCb` | `SwitchWorldTypeCommand` | Queued |
| `drawAreaEventCb` | `MouseDownEvent`, `MouseMoveEvent`, `MouseUpEvent` | Queued |
| `onMaterialButtonClicked` | `SelectMaterialCommand` | Queued |
| `gravityBtnEventCb` | `SetGravityCommand` | Queued |
| `timescaleSliderEventCb` | `SetTimescaleCommand` | Queued |
| `elasticitySliderEventCb` | `SetElasticityCommand` | Queued |
| `screenshotBtnEventCb` | `CaptureScreenshotCommand` | Queued |
| Quit button | `QuitApplicationCommand` | Queued |

## Details

### 1. Core State Machine Files

```cpp
// File: src/DirtSimStateMachine.h
class DirtSimStateMachine {
    State::Any fsmState{State::Startup{}};
    EventProcessor eventProcessor;
    EventRouter eventRouter;
    SharedSimState sharedState;
    std::unique_ptr<WorldInterface> world;
    std::unique_ptr<UIManager> uiManager;
    std::unique_ptr<SimulationManager> simulationManager;
    
public:
    void queueEvent(const Event& event);
    void handleEvent(const Event& event);
    EventRouter& getEventRouter();
    SharedSimState& getSharedState();
    SimulationManager* getSimulationManager();
};

// File: src/EventProcessor.h
class EventProcessor {
    SynchronizedQueue<Event> eventQueue;
    
public:
    void processEventsFromQueue(DirtSimStateMachine& dsm);
    void processEvent(const Event& event, DirtSimStateMachine& dsm);
};

// File: src/Event.h

// Example event structures with static name() method
struct GetFPSCommand {
    static constexpr const char* name() { return "GetFPSCommand"; }
};

struct PauseCommand {
    static constexpr const char* name() { return "PauseCommand"; }
};

struct MouseDownEvent {
    int pixelX;
    int pixelY;
    static constexpr const char* name() { return "MouseDownEvent"; }
};

struct SetTimescaleCommand {
    double timescale;
    static constexpr const char* name() { return "SetTimescaleCommand"; }
};

// Event variant containing all event types
using Event = std::variant<
    // Immediate events
    SetGravityCommand,
    SetTimescaleCommand,
    SetElasticityCommand,
    ToggleDebugCommand,
    
    // Queued events - state transitions
    InitCompleteEvent,
    StartSimulationCommand,
    PauseCommand,
    ResumeCommand,
    ResetSimulationCommand,
    QuitApplicationCommand,
    OpenConfigCommand,
    
    // Queued events - simulation
    AdvanceSimulationCommand,
    SelectMaterialCommand,
    SaveWorldCommand,
    
    // Queued events - user input
    MouseDownEvent,
    MouseMoveEvent,
    MouseUpEvent,
    
    // Queued events - UI
    CaptureScreenshotCommand
>;
```

### 2. Event Classification System

```cpp
// File: src/EventTraits.h
template<typename T>
struct IsImmediateEvent : std::false_type {};

// Immediate event specializations
template<> struct IsImmediateEvent<SetGravityCommand> : std::true_type {};
template<> struct IsImmediateEvent<SetTimescaleCommand> : std::true_type {};
template<> struct IsImmediateEvent<SetElasticityCommand> : std::true_type {};
template<> struct IsImmediateEvent<ToggleDebugCommand> : std::true_type {};
```

### 3. Thread-Safe Shared State

```cpp
// File: src/SharedSimState.h

// New struct to hold simulation statistics
struct SimulationStats {
    uint32_t totalCells = 0;
    uint32_t activeCells = 0;
    uint32_t dirtCells = 0;
    uint32_t waterCells = 0;
    uint32_t wallCells = 0;
    float totalMass = 0.0f;
    float avgPressure = 0.0f;
    uint32_t stepCount = 0;
    std::chrono::steady_clock::time_point lastUpdate;
};

class SharedSimState {
    std::atomic<bool> shouldExit{false};
    std::atomic<bool> isPaused{false};
    std::atomic<uint32_t> currentStep{0};
    std::atomic<float> currentFPS{0.0f};
    
    mutable std::shared_mutex statsMutex;
    SimulationStats currentStats;
    
    mutable std::shared_mutex worldMutex;
    WorldSnapshot lastSnapshot;
    
public:
    // Atomic accessors
    bool getShouldExit() const { return shouldExit.load(); }
    void setShouldExit(bool value) { shouldExit.store(value); }
    
    bool getIsPaused() const { return isPaused.load(); }
    void setIsPaused(bool value) { isPaused.store(value); }
    
    // Protected complex data
    SimulationStats getStats() const {
        std::shared_lock lock(statsMutex);
        return currentStats;
    }
    
    void updateStats(const SimulationStats& stats) {
        std::unique_lock lock(statsMutex);
        currentStats = stats;
    }
};
```

### 4. Event Router

```cpp
// File: src/EventRouter.h
class EventRouter {
    DirtSimStateMachine& dsm;
    SharedSimState& sharedState;
    
public:
    void routeEvent(const Event& event) {
        std::visit([this](auto&& e) {
            using T = std::decay_t<decltype(e)>;
            
            if constexpr (IsImmediateEvent<T>::value) {
                // Process immediately on current thread
                processImmediate(e);
            } else {
                // Queue for simulation thread
                dsm.queueEvent(e);
            }
        }, event);
    }
    
private:
    void processImmediate(const auto& event) {
        LOG_DEBUG("Processing immediate event: {} on thread: {}", 
                  typeid(event).name(), std::this_thread::get_id());
        
        auto start = std::chrono::steady_clock::now();
        dsm.processImmediateEvent(event, sharedState);
        auto duration = std::chrono::steady_clock::now() - start;
        
        LOG_DEBUG("Immediate event processed in {} us", 
                  std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
    }
};
```

### 5. State Implementations

#### State Lifecycle Management

States can optionally define `onEnter` and `onExit` methods that will be called during state transitions. The state machine uses compile-time detection to check if these methods exist:

```cpp
// File: src/DirtSimStateMachine.cpp
template<typename State>
void callOnEnter(State& state, DirtSimStateMachine& dsm) {
    if constexpr (requires { state.onEnter(dsm); }) {
        state.onEnter(dsm);
    }
}

template<typename State>
void callOnExit(State& state, DirtSimStateMachine& dsm) {
    if constexpr (requires { state.onExit(dsm); }) {
        state.onExit(dsm);
    }
}

void DirtSimStateMachine::transitionTo(State::Any newState) {
    // Call onExit for current state
    std::visit([this](auto& state) {
        callOnExit(state, *this);
    }, fsmState);
    
    // Perform transition
    fsmState = std::move(newState);
    
    // Call onEnter for new state
    std::visit([this](auto& state) {
        callOnEnter(state, *this);
    }, fsmState);
}
```

This approach provides:
- **Optional lifecycle methods** - States only define them if needed
- **Zero runtime overhead** - No virtual calls or function pointers
- **Type safety** - Compile-time checking of method signatures
- **Clean syntax** - No inheritance required

#### State Examples

```cpp
// File: src/states/SimRunning.cpp
struct SimRunning {
    uint32_t stepCount = 0;
    
    void onEnter(DirtSimStateMachine& dsm) {
        spdlog::info("SimRunning: Creating SimulationManager");
        
        // Create SimulationManager for backend loop integration
        lv_obj_t* screen = lv_is_initialized() ? lv_scr_act() : nullptr;
        dsm.simulationManager = std::make_unique<SimulationManager>(
            WorldType::RulesB, 7, 7, screen);
        dsm.simulationManager->initialize();
        
        stepCount = dsm.getSharedState().getCurrentStep();
        spdlog::info("SimRunning: SimulationManager created, simulation ready");
    }
    
    void onExit(DirtSimStateMachine& dsm) {
        spdlog::info("SimRunning: Exiting state");
        // Note: SimulationManager preserved for SimPaused state
    }
    
    State::Any onEvent(const AdvanceSimulationCommand& cmd, DirtSimStateMachine& dsm) {
        if (dsm.simulationManager) {
            dsm.simulationManager->advanceTime(1.0/60.0);
            stepCount++;
            dsm.getSharedState().setCurrentStep(stepCount);
        }
        return *this;
    }
    
    State::Any onEvent(const PauseCommand& cmd, DirtSimStateMachine& dsm) {
        spdlog::info("SimRunning: Pausing at step {}", stepCount);
        return State::SimPaused{};
    }
    
    State::Any onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm) {
        dsm.getSharedState().setSelectedMaterial(cmd.material);
        if (dsm.simulationManager && dsm.simulationManager->getWorld()) {
            dsm.simulationManager->getWorld()->setSelectedMaterial(cmd.material);
        }
        return *this;
    }
};

// File: src/states/Startup.cpp
struct Startup {
    State::Any onEvent(const InitCompleteEvent& evt, DirtSimStateMachine& dsm) {
        // Create world based on config
        dsm.world = WorldFactory::createWorld(dsm.config.worldType);
        
        // Transition to main menu
        LOG_INFO("STATE_TRANSITION: Startup -> MainMenu");
        return State::MainMenu{};
    }
};

// File: src/states/MainMenu.cpp
struct MainMenu {
    // UI stored statically due to value semantics of states
    static std::unique_ptr<MainMenuUI> menuUI;
    
    void onEnter(DirtSimStateMachine& dsm) {
        spdlog::info("MainMenu: Creating UI");
        if (lv_is_initialized() && dsm.uiManager) {
            // UIManager provides container
            auto container = dsm.uiManager->createMenuContainer();
            menuUI = std::make_unique<MainMenuUI>(container);
        }
    }
    
    void onExit(DirtSimStateMachine& dsm) {
        spdlog::info("MainMenu: Cleaning up UI");
        menuUI.reset();
        if (dsm.uiManager) {
            dsm.uiManager->clearCurrentContainer();
        }
    }
    
    State::Any onEvent(const StartSimulationCommand& cmd, DirtSimStateMachine& dsm) {
        spdlog::info("MainMenu: Starting simulation");
        return State::SimRunning{};
    }
    
    State::Any onEvent(const SelectMaterialCommand& cmd, DirtSimStateMachine& dsm) {
        dsm.getSharedState().setSelectedMaterial(cmd.material);
        spdlog::debug("MainMenu: Selected material {}", static_cast<int>(cmd.material));
        return *this;
    }
};
```

### 6. Enhanced Logging
Make sure to log at the INFO level: state transitions, event queuing, immediate  event processing, event processing, world state at key times.

## Implementation Status

### ✅ Complete Implementation (2025-06-22)

The dual-path event system has been fully implemented and tested. All phases are complete:

#### Phase Completion Summary:
- **Phase 1: Foundation** ✅ - All event types, shared state, and core infrastructure
- **Phase 2: Event System** ✅ - Event processor, router, and LVGL integration  
- **Phase 3: State Machine** ✅ - All 11 states with lifecycle management
- **Phase 4: UI Management** ✅ - UIManager and state-owned UI components
- **Phase 5: Integration** ✅ - Backend loop integration with --event-system flag
- **Phase 6: Testing** ✅ - Comprehensive test suite with 100% passing tests

#### Final Architecture:
1. **Event Classification**: Clear separation between immediate (UI parameter updates) and queued events (state transitions)
2. **State Machine**: 11 states managing application flow with proper onEnter/onExit lifecycle
3. **Thread Safety**: SharedSimState provides lock-free atomics for simple data, mutexes for complex state
4. **UI Management**: Hybrid approach with state-owned components and UIManager for LVGL resources
5. **Backend Integration**: SimulationManager created by states, accessible to backend loop

#### Test Results:
- **Total Tests**: 277 (255 passing, 1 skipped, 21 failing in unrelated physics systems)
- **Event System Tests**: 100% passing
  - DirtSimStateMachineTest: 10/10 ✅
  - StateTests: 13/13 ✅
  - StateTransitionTests: 13/13 ✅
  - IntegrationTests: 11/11 ✅
  - EventClassification tests: All passing ✅

#### Key Implementation Decisions:
1. **Static UI Storage**: States use static storage for UI components due to std::variant value semantics
2. **Event Routing**: PauseCommand/ResumeCommand are queued (not immediate) to enable state transitions
3. **SelectMaterialCommand Handler**: Added to all relevant states (MainMenu, SimRunning, SimPaused)
4. **SimulationManager Lifecycle**: Created in SimRunning::onEnter, preserved through pause states

### Phase 7: Migration

#### 7.1: Event System Extensions
- [ ] Add missing 35+ events to Event.h (physics sliders, UI toggles)
- [ ] Extend EventRouter with new immediate event classifications
- [ ] Add event handlers to SimRunning/SimPaused states
- [x] **Fix Mouse Event Handlers** ✅ (Completed 2025-09-14):
  - [x] Add `MouseMoveEvent` handler to SimRunning state
  - [x] Add `MouseUpEvent` handler to SimRunning state
  - [x] Add interaction mode tracking to SimRunning state
  - [x] Update `MouseDownEvent` handler with unified behavior:
    - Always enters GRAB_MODE (single particle drag)
    - If cell has material: calls `startDragging()`
    - If cell is empty: calls `addMaterialAtPixel()` then `startDragging()`
  - [x] MouseMoveEvent: calls `updateDrag()` when in GRAB_MODE
  - [x] MouseUpEvent: calls `endDragging()` and resets mode
  - [x] Same handlers added to SimPaused state for consistency

#### 7.2: UI Callback Migration (Batch Approach)
**Batch 1: Core Controls (5 callbacks)**
- [ ] `pauseBtnEventCb` → PauseCommand/ResumeCommand
- [ ] `resetBtnEventCb` → ResetSimulationCommand
- [ ] `gravityBtnEventCb` → SetGravityCommand
- [ ] `timescaleSliderEventCb` → SetTimescaleCommand
- [ ] `elasticitySliderEventCb` → SetElasticityCommand

**Batch 2: Physics Parameters (10 callbacks)**
- [ ] `pressureScaleSliderEventCb` → SetPressureScaleCommand
- [ ] `pressureScaleWorldBSliderEventCb` → SetPressureScaleWorldBCommand
- [ ] `cohesionForceBtnEventCb` → ToggleCohesionForceCommand
- [ ] `cohesionForceStrengthSliderEventCb` → SetCohesionForceStrengthCommand
- [ ] `adhesionBtnEventCb` → ToggleAdhesionCommand
- [ ] `adhesionStrengthSliderEventCb` → SetAdhesionStrengthCommand
- [ ] `viscosityStrengthSliderEventCb` → SetViscosityStrengthCommand
- [ ] `frictionStrengthSliderEventCb` → SetFrictionStrengthCommand
- [ ] `comCohesionRangeSliderEventCb` → SetCOMCohesionRangeCommand
- [ ] `airResistanceSliderEventCb` → SetAirResistanceCommand

**Batch 3: Material & World Controls (8 callbacks)**
- [ ] `worldTypeButtonMatrixEventCb` → SwitchWorldTypeCommand
- [ ] Material picker callbacks → SelectMaterialCommand (already implemented)
- [ ] `cellSizeSliderEventCb` → SetCellSizeCommand
- [ ] `fragmentationSliderEventCb` → SetFragmentationCommand
- [ ] `pressureSystemDropdownEventCb` → SetPressureSystemCommand
- [ ] `forceBtnEventCb` → ToggleForceCommand
- [ ] `cohesionBtnEventCb` → ToggleCohesionCommand
- [ ] `debugBtnEventCb` → ToggleDebugCommand

**Batch 4: Advanced Features (10 callbacks)**
- [ ] `screenshotBtnEventCb` → CaptureScreenshotCommand
- [ ] `printAsciiBtnEventCb` → PrintAsciiDiagramCommand
- [ ] `quitBtnEventCb` → QuitApplicationCommand
- [ ] `backwardBtnEventCb` → StepBackwardCommand
- [ ] `forwardBtnEventCb` → StepForwardCommand
- [ ] `timeReversalToggleBtnEventCb` → ToggleTimeReversalCommand
- [ ] `leftThrowBtnEventCb` → ToggleLeftThrowCommand
- [ ] `rightThrowBtnEventCb` → ToggleRightThrowCommand
- [ ] `quadrantBtnEventCb` → ToggleQuadrantCommand
- [ ] `frameLimitBtnEventCb` → ToggleFrameLimitCommand

**Batch 5: Specialized Physics (10 callbacks)**
- [ ] `waterCohesionSliderEventCb` → SetWaterCohesionCommand
- [ ] `waterViscositySliderEventCb` → SetWaterViscosityCommand
- [ ] `waterPressureThresholdSliderEventCb` → SetWaterPressureThresholdCommand
- [ ] `waterBuoyancySliderEventCb` → SetWaterBuoyancyCommand
- [ ] `hydrostaticPressureToggleEventCb` → ToggleHydrostaticPressureCommand
- [ ] `dynamicPressureToggleEventCb` → ToggleDynamicPressureCommand
- [ ] `pressureDiffusionToggleEventCb` → TogglePressureDiffusionCommand
- [ ] `hydrostaticPressureStrengthSliderEventCb` → SetHydrostaticPressureStrengthCommand
- [ ] `dynamicPressureStrengthSliderEventCb` → SetDynamicPressureStrengthCommand
- [ ] `rainSliderEventCb` → SetRainRateCommand

#### 7.3: Default Path Switch
- [ ] Update main.cpp to default use_event_system = true
- [ ] Add legacy flag --traditional-mode for fallback
- [ ] Update Makefile run targets to use event system

#### 7.4: Validation & Cleanup
- [ ] Visual test suite with all 43 UI interactions
- [ ] Remove unused callback methods from SimulatorUI
- [ ] Performance benchmarks (event vs traditional)
- [ ] Create migration guide for existing code

#### Migration Component Status
**Core Files Requiring Updates:**
- [ ] `src/Event.h` - Add 35+ new event types
- [ ] `src/EventTraits.h` - Classify new events as immediate/queued
- [ ] `src/EventRouter.h` - Add immediate event processing methods
- [ ] `src/EventRouter.cpp` - Implement new immediate event handlers
- [ ] `src/states/SimRunning.cpp` - Add onEvent handlers for queued events
- [ ] `src/states/SimPaused.cpp` - Add onEvent handlers for applicable events
- [ ] `src/SimulatorUI.cpp` - Replace callback registrations with EventRouter calls
- [ ] `src/SimulatorUI.h` - Remove static callback method declarations
- [ ] `src/main.cpp` - Switch default to event system
- [ ] `Makefile` - Update run targets

#### Performance Monitoring
- [ ] Add metrics for event queue depth
- [ ] Monitor UIUpdateQueue drop rates
- [ ] Benchmark immediate vs push-based event latency
- [ ] Profile memory usage of event system vs traditional

#### Migration Safety
- [ ] Feature flag for gradual rollout: SPARKLE_DUCK_EVENT_SYSTEM=1
- [ ] Automated tests for UI callback → event conversion
- [ ] Rollback plan if performance regressions occur
- [ ] Compatibility layer during transition period

### Phase 8: Documentation
- [ ] Update code comments with thread safety notes
- [ ] Document state transition diagram
- [ ] Create event flow diagrams
- [ ] Update system architecture documentation


## UI Lifecycle Management Decision

### Chosen Approach: Hybrid State-Owned UI with Lightweight UIManager

After analyzing the current architecture and considering various approaches, we've decided on a hybrid approach that combines state ownership of UI components with a lightweight UIManager for LVGL resource management.

#### Key Design Principles:

1. **States Own Their UI Components**:
   - Each state that needs UI creates and destroys its own UI components
   - UI components are stored as members of the state (e.g., `std::unique_ptr<SimulatorUI>`)
   - States manage UI lifecycle through `onEnter()` and `onExit()` methods

2. **UIManager Handles LVGL Resources**:
   - Provides containers/screens for states to build their UI
   - Manages screen transitions and LVGL-specific cleanup
   - Does NOT own the business logic UI components

3. **Example Implementation Pattern**:
   ```cpp
   // States own their UI components
   struct SimRunning {
       std::unique_ptr<SimulatorUI> ui;
       
       void onEnter(DirtSimStateMachine& dsm) {
           // UIManager provides the container
           auto container = dsm.uiManager.getSimulationContainer();
           ui = std::make_unique<SimulatorUI>(container, dsm.world.get());
       }
       
       void onExit(DirtSimStateMachine& dsm) {
           ui.reset(); // State manages lifecycle
           dsm.uiManager.clearContainer();
       }
   };
   ```

#### Benefits of This Approach:
- **Cohesive**: States encapsulate both logic and UI
- **Clear Ownership**: No ambiguity about who manages what
- **Testable**: Can mock UIManager for unit tests
- **Extensible**: Easy to add new states with different UI needs
- **RAII-Compliant**: Automatic cleanup through smart pointers

#### Implementation Plan for Phase 4:
1. Create lightweight `UIManager` class
2. Update states to own their UI components
3. Convert LVGL callbacks to use EventRouter
4. Ensure proper cleanup in all state transitions

## Phase 5 Integration Complete! 🎉 (2025-06-22)

Today we successfully completed the integration of the event system with the backend run loop! Here's what was accomplished:

### Key Achievements

1. **Enhanced CLI with args library**:
   - Replaced old getopt-based argument parsing with modern C++ args library
   - Added clean, well-formatted help system
   - Maintained all existing command-line options

2. **State Machine Backend Integration**:
   - Modified `DirtSimStateMachine` to manage `SimulationManager` instances
   - Added `getSimulationManager()` method for backend loop access
   - States now control the lifecycle of simulation resources

3. **SimRunning State Refactoring**:
   - `SimRunning::onEnter()` now creates a `SimulationManager`
   - `SimRunning::onExit()` properly cleans up the manager
   - All event handlers updated to use `simulationManager` instead of direct world access

4. **Main.cpp Integration**:
   - Event system initialization: `Startup` → `MainMenu` → `SimRunning`
   - Backend loop now gets `SimulationManager` from the state machine
   - Clean fallback to traditional mode if event system fails

### Architecture Solution

The key insight was having the state machine manage SimulationManager instances:

```cpp
class DirtSimStateMachine {
    std::unique_ptr<SimulationManager> simulationManager;
    
public:
    SimulationManager* getSimulationManager() { 
        return simulationManager.get(); 
    }
};
```

This allows:
- Backend loop remains unchanged (still expects SimulationManager)
- States control when simulation resources are created/destroyed
- Clean separation between UI states and simulation states
- Future states can provide different managers or nullptr

### Testing Results

The `--event-system` flag now works correctly:
```bash
./build/bin/sparkle-duck --event-system -s 10
```

Output shows:
- ✅ "Starting with event-driven state machine"
- ✅ State transitions: Startup → MainMenu → SimRunning
- ✅ "SimRunning: Creating SimulationManager"
- ✅ "Using SimulationManager from event system (7x7 grid)"
- ✅ Simulation runs for specified steps
- ✅ Clean shutdown with proper resource cleanup

### Next Steps

With Phase 5 complete, the event system is now fully integrated! The next phases involve:
- Phase 6: Comprehensive testing of the event system
- Phase 7: Migration of remaining legacy code
- Phase 8: Documentation updates

The foundation is solid and ready for building more complex state-based features!

## Push-Based UI Update Integration (2025-06-22)

The dual-path event system has been enhanced with push-based UI updates to eliminate thread safety issues:

### Key Changes

1. **Dual-Path Routing Enhancement**: EventRouter now supports routing immediate events through the push-based system when enabled
   - `isPushCompatible()` method identifies migratable events
   - Feature flag `isPushUpdatesEnabled()` controls routing behavior
   - All 8 immediate event types can now be routed through state machine

2. **Thread Safety**: Immediate events no longer directly access SharedSimState from UI thread
   - UI updates flow through thread-safe UIUpdateQueue
   - Latest-update-wins semantics prevent backlog
   - 60fps LVGL timer consumes updates on UI thread

3. **Backward Compatibility**: Original immediate processing path remains for gradual migration
   - CLI flag `--push-updates` enables new system
   - Runtime toggle via `enablePushUpdates(bool)`
   - Easy rollback if issues arise

### Testing

Enable push-based updates with:
```bash
./build/bin/sparkle-duck --event-system --push-updates -s 100
# Or just:
./build/bin/sparkle-duck -p -s 100  # -p auto-enables event system
```

The dual-path system now provides a migration path from thread-unsafe immediate events to the fully thread-safe push-based architecture.