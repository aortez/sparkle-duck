# Phase 0 Analysis: Push-Based UI Update System

## 1. Simulation Flow Mapping

### 1.1 Where World Methods are Called

The simulation uses `advanceTime()` instead of `step()`:
- **World::advanceTime()** - src/World.cpp:71
- **WorldB::advanceTime()** - src/WorldB.cpp:103

### 1.2 Complete Simulation Loop

1. **Main Thread** (`DirtSimStateMachine::run()` - src/DirtSimStateMachine.cpp:42)
   - Runs in a loop with ~60 FPS timing (16ms sleep)
   - If in `SimRunning` state and not paused, queues `AdvanceSimulationCommand`

2. **Event Flow**:
   ```
   DirtSimStateMachine::run()
   └── queueEvent(AdvanceSimulationCommand) [line 48]
       └── EventProcessor::processEventsFromQueue()
           └── SimRunning::onEvent(AdvanceSimulationCommand)
               └── SimulationManager::advanceTime(1.0/60.0) [SimRunning.cpp:64]
                   └── WorldInterface::advanceTime()
                       ├── World::advanceTime() [RulesA]
                       └── WorldB::advanceTime() [RulesB]
   ```

3. **SimRunning State Handler** (src/states/SimRunning.cpp:57)
   - Receives `AdvanceSimulationCommand`
   - Calls `SimulationManager::advanceTime(1.0/60.0)`
   - Updates step count in SharedSimState
   - Updates statistics every 60 steps
   - Updates FPS (currently hardcoded to 60.0f)

4. **World Physics Processing**:
   - **WorldB::advanceTime()** includes:
     - Add particles (if enabled)
     - resolveForces()
     - processVelocityLimiting()
     - updateTransfers()
     - processMaterialMoves()
     - Pressure calculations (hydrostatic/dynamic)
   
   - **World::advanceTime()** includes:
     - Time reversal state saving
     - Physics step processing
     - Material transfers

### 1.3 State Machine States that Advance Simulation

States that can advance the simulation:

1. **SimRunning** (src/states/SimRunning.cpp)
   - Continuously advances simulation when not paused
   - Handles `AdvanceSimulationCommand` automatically via main loop
   - Updates SharedSimState with step count and stats every 60 steps
   - Updates FPS (currently hardcoded to 60.0f)

2. **SimPaused** (src/states/SimPaused.cpp)
   - Can advance simulation one step at a time (frame-by-frame debugging)
   - Handles `AdvanceSimulationCommand` for single-step advancement
   - Maintains previousState.stepCount
   - Stays in paused state after advancing

All State Machine States:
- **Startup** - Initial resource loading
- **MainMenu** - User can start simulation or access settings
- **SimRunning** - Active physics simulation (advances continuously)
- **SimPaused** - Paused simulation (can advance single steps)
- **UnitTesting** - Running automated tests
- **Benchmarking** - Performance benchmarking
- **Loading** - Loading saved simulation state
- **Saving** - Saving current simulation state
- **Config** - Configuration/settings
- **Demo** - Demo/tutorial mode
- **Shutdown** - Cleanup and exit

## 2. LVGL Integration

### 2.1 Display Backend Timer Handling

Each display backend has its own main loop that calls LVGL's timer handler:

1. **SDL Backend** (src/lib/display_backends/sdl.cpp)
   - Calls `lv_timer_handler()` in main loop (line 132)
   - Returns idle time for frame limiting
   - Processes final UI updates before screenshot (3x calls)

2. **X11 Backend** (src/lib/display_backends/x11.cpp)
   - Identical structure to SDL
   - Calls `lv_timer_handler()` in main loop (line 138)
   - Frame limiting based on idle time

3. **Wayland Backend** (src/lib/display_backends/wayland.cpp)
   - Uses `lv_wayland_timer_handler()` instead (line 150)
   - Different API but same purpose
   - Returns completion status instead of idle time

4. **FBDEV Backend** (src/lib/display_backends/fbdev.cpp)
   - Also uses `lv_timer_handler()`

### 2.2 Current UI Update Patterns

**UI Update Methods in SimulatorUI**:
- `updateFPSLabel(uint32_t fps)` - Updates FPS display
- `updateDebugButton()` - Updates debug button text based on Cell::debugDraw
- `updateMassLabel(double totalMass)` - Updates mass display

**Current Update Flow**:
1. **FPS Updates**:
   - Updated in `SimulatorLoop::processFrame()` every second
   - Calls `world->getUI()->updateFPSLabel(state.fps)`
   
2. **Mass Updates**:
   - World classes call `ui_->updateMassLabel()` directly during physics processing
   
3. **Debug/Force/Cohesion Toggles**:
   - Processed as immediate events in EventRouter
   - Modify SharedSimState or global flags (Cell::debugDraw)
   - UI is NOT automatically updated - relies on polling or next redraw

## 3. Current Immediate Events and Dependencies

### 3.1 Immediate Events (from EventTraits.h)

1. **GetFPSCommand**
   - Reads: `sharedState_.getCurrentFPS()`
   - No UI update (just logs)

2. **GetSimStatsCommand**
   - Reads: `sharedState_.getStats()`
   - No UI update (just logs)

3. **ToggleDebugCommand**
   - Modifies: `Cell::debugDraw` (global static)
   - No UI update callback

4. **PrintAsciiDiagramCommand**
   - Reads: `sharedState_.getCurrentWorld()`
   - Calls: `world->toAsciiDiagram()`
   - Output to log only

5. **ToggleForceCommand**
   - Reads: `sharedState_.getPhysicsParams()`
   - Modifies: `params.forceVisualizationEnabled`
   - Writes: `sharedState_.updatePhysicsParams(params)`

6. **ToggleCohesionCommand**
   - Reads: `sharedState_.getPhysicsParams()`
   - Modifies: `params.cohesionEnabled`
   - Writes: `sharedState_.updatePhysicsParams(params)`

7. **ToggleAdhesionCommand**
   - Reads: `sharedState_.getPhysicsParams()`
   - Modifies: `params.adhesionEnabled`, `Cell::adhesionDrawEnabled`
   - Writes: `sharedState_.updatePhysicsParams(params)`
   - Calls: `world->setAdhesionEnabled()` if world exists

8. **ToggleTimeHistoryCommand**
   - Reads: `sharedState_.getPhysicsParams()`
   - Modifies: `params.timeHistoryEnabled`
   - Writes: `sharedState_.updatePhysicsParams(params)`

### 3.2 SharedSimState Access Patterns

**Thread Safety Issues**:
- Immediate events run on UI thread
- Simulation thread also accesses SharedSimState
- No synchronization for physics params updates
- Direct world pointer access (`getCurrentWorld()`) is dangerous

## 4. Migration Inventory

### 4.1 EventTraits Specializations to Remove

All in src/EventTraits.h:
- `IsImmediateEvent<GetFPSCommand>` (line 27)
- `IsImmediateEvent<GetSimStatsCommand>` (line 32)
- `IsImmediateEvent<ToggleDebugCommand>` (line 38)
- `IsImmediateEvent<PrintAsciiDiagramCommand>` (line 44)
- `IsImmediateEvent<ToggleForceCommand>` (line 50)
- `IsImmediateEvent<ToggleCohesionCommand>` (line 56)
- `IsImmediateEvent<ToggleAdhesionCommand>` (line 62)
- `IsImmediateEvent<ToggleTimeHistoryCommand>` (line 68)

### 4.2 processImmediateEvent Implementations

All in src/EventRouter.cpp:
- `processImmediateEvent(const GetFPSCommand&)` (line 6)
- `processImmediateEvent(const GetSimStatsCommand&)` (line 17)
- `processImmediateEvent(const PauseCommand&)` (line 32)
- `processImmediateEvent(const ResumeCommand&)` (line 44)
- `processImmediateEvent(const ToggleDebugCommand&)` (line 56)
- `processImmediateEvent(const PrintAsciiDiagramCommand&)` (line 68)
- `processImmediateEvent(const ToggleForceCommand&)` (line 81)
- `processImmediateEvent(const ToggleCohesionCommand&)` (line 93)
- `processImmediateEvent(const ToggleAdhesionCommand&)` (line 104)
- `processImmediateEvent(const ToggleTimeHistoryCommand&)` (line 124)

### 4.3 Test Coverage Gaps

Need to check:
- Tests for immediate event processing
- Tests for UI update mechanisms
- Thread safety tests for SharedSimState access

## Phase 0 Completion Note

**Status**: COMPLETED (2025-06-22)

This analysis provided the foundation for implementing the push-based UI update system. All identified issues have been addressed:

1. **Thread Safety**: Implemented UIUpdateQueue with mutex protection
2. **Immediate Events**: All 8 types now routable through push system via dual-path support
3. **State Updates**: Push points added after simulation advancement and state transitions
4. **Test Coverage**: 65 tests added across all components

The system is now ready for Phase 4: real-world testing and migration.