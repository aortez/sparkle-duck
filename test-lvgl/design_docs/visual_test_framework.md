# Visual Test Framework

## Overview

The Sparkle Duck visual test framework provides an interactive testing environment that bridges automated unit testing with visual validation of physics simulations. It allows developers to observe simulation behavior in real-time while maintaining the structure and reproducibility of automated tests.

## Architecture

### Core Components

**VisualTestCoordinator** - Global coordinator managing test lifecycle and UI synchronization
- Singleton pattern for centralized test management
- Thread-safe communication between test and UI threads
- Test state tracking and progression control

**VisualTestEnvironment** - GoogleTest environment for visual test configuration
- Integrates with GoogleTest framework
- Manages SPARKLE_DUCK_VISUAL_TESTS environment variable
- Handles display backend initialization

**VisualTestBase** - Base class for all visual tests
- Provides common test infrastructure
- Manages World/WorldB instance creation
- Handles test lifecycle methods

**TestUI** - Specialized LVGL interface for test visualization
- 400x400 pixel simulation display area
- Interactive control buttons (Start, Step, Step10, Next, Screenshot)
- Status label for test feedback

### Threading Model

The framework operates on a dual-thread architecture:
- **Main Thread**: Runs GoogleTest framework and test logic
- **UI Thread**: Manages LVGL rendering and Wayland event loop

Thread synchronization is achieved through:
- Atomic variables for button state tracking
- Condition variables for test progression blocking
- Message passing for status updates

## Test Flow

### Standard Test Progression

1. **Initialization**
   - Test creates World/WorldB instance with initial conditions
   - TestUI displays initial world state
   - Status shows "Press Start to begin"

2. **User Interaction**
   - **Start**: Starts/Restarts test execution
   - **Step**: Advances simulation by one timestep
   - **Step10**: Executes 10 simulation steps
   - **Next**: Skips to next test

3. **Test Execution**
   - Tests use `waitForStart()` to pause for user readiness
   - `stepSimulation(n)` advances physics by n timesteps
   - `waitForStep()` blocks until Step button pressed
   - Visual feedback updates after each operation
   - After pressing Step or Step10, if the test isn't complete yet, the framework allows the user to press Start, Step, or Step10 to continue the test in the appropriate manner.

4. **Validation**
   - Visual observation of physics behavior
   - Programmatic assertions on world state
   - Screenshot capture for documentation

### Framework Methods for Visual Tests

**showInitialState(world, description)** - Display initial world state with Start button only. Use this only for tests where step-by-step observation is not desired (e.g., simple pass/fail tests or continuous animations).

**showInitialStateWithStep(world, description)** - **RECOMMENDED**: Display initial state with Start/Step/Step10 buttons. This enables frame-by-frame analysis and is particularly valuable for debugging physics interactions, collision detection, and complex material behaviors. Most tests should use this method.

**stepSimulation(world, steps, description)** - Advance simulation with proper Step mode handling

**updateDisplay(world, status)** - Update the world display and status message

**pauseIfVisual(ms)** - Pause for specified milliseconds in visual mode only

**waitForNext()** - Wait for user to press Next button before proceeding to next test

**waitForRestartOrNext()** - Wait for Start (to restart) or Next button after test completion

**runRestartableTest(lambda)** - Wrap test logic in a restart loop, allowing repeated test execution

### Test Modes

**Headless Mode** (Default)
- Tests run without visual output
- No user interaction required
- Fast execution for CI/CD pipelines

**Visual Mode** (SPARKLE_DUCK_VISUAL_TESTS=1)
- Full visual display of simulation
- Interactive test control
- Manual progression through test steps

**Step Mode**
- Frame-by-frame simulation advancement
- Detailed observation of physics behavior
- Useful for debugging complex interactions

## Test Implementation Patterns

### Unified Simulation Loop (Recommended)

**NEW APPROACH**: Use `runSimulationLoop()` to eliminate code duplication between visual and non-visual modes:

```cpp
class MyTest : public VisualTestBase {
    WorldInterface* getWorldInterface() override {
        return world.get();  // Required for unified loop
    }
    std::unique_ptr<WorldB> world;
};

TEST_F(MyTest, ExampleTest) {
    // Setup
    world->addMaterialAtCell(1, 1, MaterialType::WATER, 1.0);
    showInitialState(world.get(), "Water falling test");
    
    // State tracking variables
    std::vector<double> velocities;
    bool hitBottom = false;
    
    // Single loop for both modes - NO DUPLICATION
    runSimulationLoop(30, [&](int step) {
        // Test logic runs identically in both modes
        auto& cell = world->at(1, 1);
        velocities.push_back(cell.getVelocity().y);
        
        if (cell.getPosition().y > 8) {
            hitBottom = true;
        }
        
        // Optional: visual-only display
        if (visual_mode_) {
            updateDisplay(world.get(), "Step " + std::to_string(step));
        }
    },
    "Testing water falling",           // Description for visual mode
    [&]() { return hitBottom; }        // Optional early stop condition
    );
    
    // Verify results
    EXPECT_TRUE(hitBottom);
}
```

**Benefits**:
- Single test logic for both modes
- Framework handles visual/non-visual differences
- Lambda captures state variables cleanly

**Reference**: See `src/tests/UnifiedSimLoopExample_test.cpp` for comprehensive examples and best practices.

### Legacy Pattern (Avoid for New Tests)

The old pattern required duplicated code paths:

```cpp
if (visual_mode_) {
    // Visual mode: Use framework methods for user interaction
    showInitialStateWithStep(world.get(), "Test description");
    
    for (int step = 0; step < maxSteps; ++step) {
        updateDisplay(world.get(), "Status message");
        stepSimulation(world.get(), 1, "Step description");
    }
    
    waitForNext();
} else {
    // Non-visual mode: Direct simulation advancement
    for (int step = 0; step < maxSteps; ++step) {
        world->advanceTime(0.016);
    }
}
```

**Problems with legacy pattern**:
- Code duplication between visual/non-visual branches
- Maintenance burden when updating test logic
- Risk of visual/non-visual modes testing different behavior

### Step Mode Support (Recommended Default)

Step mode should be the default choice for most tests as it provides maximum flexibility for debugging:

```cpp
// RECOMMENDED: Use Step mode by default for better observability
showInitialStateWithStep(world.get(), "Initial state description");

// Use stepSimulation() instead of direct advanceTime() in visual mode
stepSimulation(world.get(), 1, "What this step tests");

// Update display with current status
updateDisplay(world.get(), "Current state: velocity=" + std::to_string(vel));

// Final test summary before next test
updateDisplay(world.get(), "Test complete: all assertions passed");
waitForNext();
```

## Writing Visual Tests

### Modern Test Structure (Standard Pattern with Restart)

Using the unified simulation loop pattern with restart functionality as the standard:

```cpp
class MyVisualTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        world = createWorldB(10, 10);
        
        // Apply test-specific settings
        world->setAirResistanceEnabled(false);
        world->setGravity(9.81);
    }
    
    // Required for unified simulation loop
    WorldInterface* getWorldInterface() override {
        return world.get();
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(MyVisualTest, WaterFlowsDownhill) {
    // STANDARD PATTERN: Use runRestartableTest as the outer wrapper
    runRestartableTest([this]() {
        // REQUIRED: Clear world state for clean restarts
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        // Setup initial conditions
        world->addMaterialAtCell(5, 1, MaterialType::WATER, 1.0);
        
        // Show initial state - use Step mode for better debugging
        showInitialStateWithStep(world.get(), "Water flows downhill test");
        
        // State tracking
        std::vector<double> waterPositions;
        bool reachedBottom = false;
        
        // Single loop handles both visual and non-visual modes
        runSimulationLoop(20, [&](int step) {
            // Find water cell (it moves as it falls)
            for (uint32_t y = 0; y < world->getHeight(); y++) {
                for (uint32_t x = 0; x < world->getWidth(); x++) {
                    auto& cell = world->at(x, y);
                    if (cell.getMaterialType() == MaterialType::WATER && 
                        cell.getFillRatio() > 0.5) {
                        waterPositions.push_back(y);
                        
                        if (y >= world->getHeight() - 2) {
                            reachedBottom = true;
                        }
                        
                        // Optional custom status for visual mode
                        if (visual_mode_) {
                            std::stringstream ss;
                            ss << "Water at Y=" << y << ", velocity=" 
                               << cell.getVelocity().y;
                            updateDisplay(world.get(), ss.str());
                        }
                        return; // Found water, done for this step
                    }
                }
            }
            
            // Log world state each step
            logWorldState(world.get(), fmt::format("Step {}: Water falling", step));
        },
        "Observing water flow",        // Basic description
        [&]() { return reachedBottom; } // Stop early when water reaches bottom
        );
        
        // Verify results (works in both modes)
        EXPECT_TRUE(reachedBottom) << "Water should reach bottom";
        EXPECT_GT(waterPositions.size(), 5) << "Water should move multiple times";
        
        // STANDARD: End with waitForRestartOrNext() instead of waitForNext()
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    }); // End of runRestartableTest
}
```

### Legacy Test Structure (For Reference Only)

The old pattern with duplicated code:

```cpp
// DON'T USE THIS PATTERN FOR NEW TESTS
TEST_F(MyVisualTest, LegacyPattern) {
    world->addMaterialAtCell(5, 1, MaterialType::WATER, 1.0);
    showInitialStateWithStep(world.get(), "Water flows downhill test");
    
    const int maxSteps = 20;
    
    if (visual_mode_) {
        // Visual mode code...
        for (int step = 0; step < maxSteps; ++step) {
            // Duplicate logic here
            updateDisplay(world.get(), "Step " + std::to_string(step));
            stepSimulation(world.get(), 1, "Observing water flow");
        }
        waitForNext();
    } else {
        // Non-visual mode code...
        for (int step = 0; step < maxSteps; ++step) {
            // Same logic duplicated here
            world->advanceTime(0.016);
        }
    }
}
```

### Restartable Tests (Now Standard)

All visual tests should use the `runRestartableTest()` pattern for improved user experience:

```cpp
TEST_F(MyTest, RestartableExample) {
    // This is now the STANDARD pattern for all visual tests
    runRestartableTest([this]() {
        // REQUIRED: Clear world state at the beginning
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        // Setup test scenario
        world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
        showInitialState(world.get(), "Water test - restartable");
        
        // Run test logic with unified simulation loop
        runSimulationLoop(30, [&](int step) {
            // Test logic here
            logWorldState(world.get(), fmt::format("Step {}", step));
        }, "Running simulation");
        
        // REQUIRED: Use waitForRestartOrNext() instead of waitForNext()
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}
```

**Key benefits of restart functionality:**
- Users can repeat tests without exiting the test suite
- Helpful for debugging intermittent issues
- Allows multiple observations of the same scenario
- The Start button automatically becomes "Restart" after completion

## Future Enhancements
- Custom test scenario scripting
- Video recording of test runs
- Network-based remote test control
- Integration with WebRTC test driver

## Best Practices

### Code Structure (Updated for Restart as Standard)
1. **Always Use runRestartableTest()** - This is now the standard outer wrapper for all visual tests
2. **Clear World State** - Always clear world state at the beginning of the runRestartableTest lambda
3. **Use runSimulationLoop() Inside** - Eliminate visual/non-visual duplication within runRestartableTest
4. **Override getWorldInterface()** - Required for unified simulation loop to work
5. **Capture State with Lambdas** - Use `[&]` to capture local variables by reference
6. **Don't Call Physics Methods in Lambda** - Let the framework handle `advanceTime()` and `stepSimulation()`
7. **Default to Step Mode** - Use `showInitialStateWithStep()` unless your test specifically doesn't benefit from step-by-step observation
8. **End with waitForRestartOrNext()** - Replace `waitForNext()` with `waitForRestartOrNext()` in visual mode

### Test Design
9. **Keep Tests Focused** - Each test should validate one specific behavior
10. **Use Descriptive Names** - Test names should clearly indicate what's being tested
11. **Minimize Test Duration** - Keep interactive tests short for better developer experience
12. **Document Expected Results** - Comments should explain what correct behavior looks like
13. **Log World State** - Use `logWorldState()` in simulation loops to aid debugging

### Visual Interaction
14. **Provide Visual Context** - Add status messages explaining test progression
15. **Use Optional Visual Enhancements** - Add `if (visual_mode_)` only for extra visual feedback
16. **Support Early Termination** - Use early stop conditions for "wait until X happens" tests
17. **Restart is Standard** - All tests now support restart functionality by default

### Reference Implementation
**See `src/tests/UnifiedSimLoopExample_test.cpp`** for comprehensive examples of:
- Simple state tracking
- Multi-cell relationships  
- Stage-based progression
- Performance measurement
- Best practice patterns

The visual test framework transforms physics validation from abstract assertions into observable phenomena, making it easier to develop, debug, and demonstrate the simulation's capabilities.
