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

## Writing Visual Tests

### Basic Test Structure

```cpp
class MyVisualTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        world = createTestWorld();
    }
    
    void runTest() {
        // Do any custom set up work here.
        ...
        
        // Show initial state:
        updateDisplay();
        waitForStart();
        
        // Run simulation
        for (int i = 0; i < 100; i++) {
            stepSimulation(1);
            updateDisplay();
            
            // Optional: wait for manual step
            if (isStepMode()) {
                waitForStep();
            }
            
            // Validate/log results
            // ... 
        }
        
        // Validate/log results
        // ...
    }
};

TEST_F(MyVisualTest, WaterFlowsDownhill) {
    runTest();
}
```

### Restartable Tests

Tests can be made restartable for repeated observation:

```cpp
void runTest() override {
    while (true) {
        resetWorld();
        updateDisplay();
        waitForStart();
        
        performTestScenario();
        
        if (!waitForRestart()) {
            break;
        }
    }
}
```

## Future Enhancements
- Custom test scenario scripting
- Video recording of test runs
- Network-based remote test control
- Integration with WebRTC test driver

## Best Practices

1. **Keep Tests Focused** - Each test should validate one specific behavior
2. **Use Descriptive Names** - Test names should clearly indicate what's being tested
3. **Provide Visual Context** - Add status messages explaining test progression
4. **Enable Restart** - Allow repeated observation of interesting behaviors
5. **Document Expected Results** - Comments should explain what correct behavior looks like
6. **Minimize Test Duration** - Keep interactive tests short for better developer experience
7. **Consider Composibility/Re-useability**

The visual test framework transforms physics validation from abstract assertions into observable phenomena, making it easier to develop, debug, and demonstrate the simulation's capabilities.