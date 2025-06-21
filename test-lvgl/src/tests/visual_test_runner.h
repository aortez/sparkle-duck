#pragma once

#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <cstdio>
#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <future>
#include <sstream>

#include "../World.h"
#include "../WorldB.h"
#include "../Cell.h"
#include "TestUI.h"
#include "lvgl/lvgl.h"
#include "../lib/driver_backends.h"
#include "../lib/simulator_settings.h"

// Forward declaration
class VisualTestCoordinator;

// Global visual test coordinator
class VisualTestCoordinator {
public:
    static VisualTestCoordinator& getInstance();
    
    bool initializeVisualMode();
    bool isVisualModeEnabled() const;
    void finalCleanup();
    
    // Threading support for continuous Wayland loop
    void startEventLoop();
    void stopEventLoop();
    
    // Thread-safe LVGL task execution
    void postTask(std::function<void()> task);
    void postTaskSync(std::function<void()> task);

private:
    VisualTestCoordinator() = default;
    ~VisualTestCoordinator();
    
    // Visual mode state
    bool visual_initialized_ = false;
    bool visual_mode_enabled_ = false;
    lv_obj_t* main_screen_ = nullptr;
    
    // Threading state
    std::atomic<bool> event_loop_running_{false};
    std::atomic<bool> should_stop_loop_{false};
    std::unique_ptr<std::thread> event_thread_;
    
    // LVGL Task Queue for thread-safe operations
    std::mutex task_queue_mutex_;
    std::condition_variable task_queue_cv_;
    std::vector<std::function<void()>> task_queue_;
    
    // Event loop function
    void eventLoopFunction();
};

// Global test environment for proper visual mode coordination
class VisualTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override;
    void TearDown() override;
    
    // Universal test configuration settings
    static bool isDebugLoggingEnabled() { return debug_logging_enabled_; }
    static bool isAdhesionDisabledByDefault() { return adhesion_disabled_by_default_; }
    static bool isCohesionDisabledByDefault() { return cohesion_disabled_by_default_; }
    static bool isPressureDisabledByDefault() { return pressure_disabled_by_default_; }
    static bool isAsciiLoggingEnabled() { return ascii_logging_enabled_; }
    
private:
    // Universal test configuration flags
    static bool debug_logging_enabled_;
    static bool adhesion_disabled_by_default_;
    static bool cohesion_disabled_by_default_;
    static bool pressure_disabled_by_default_;
    static bool ascii_logging_enabled_;
};

// Base class for all visual tests
class VisualTestBase : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    // Helper to create a world with or without UI
    std::unique_ptr<World> createWorld(uint32_t width, uint32_t height);
    
    // Helper to create a WorldB with universal defaults applied
    std::unique_ptr<WorldB> createWorldB(uint32_t width, uint32_t height);
    
    // Helper method to run visual simulation if enabled, otherwise just advance time
    void runSimulation(World* world, int steps = 60, const std::string& description = "");
    
    // Manual test progression methods for visual mode
    void waitForStart();          // Block until Start button is pressed (visual mode only)
    void waitForNext();           // Block until Next button is pressed (visual mode only)
    void pauseIfVisual(int milliseconds = 500); // Pause in visual mode, no-op otherwise
    
    // Enhanced start/step functionality
    enum class TestAction {
        START,  // User pressed Start - run continuously
        STEP,   // User pressed Step - run step by step
        NEXT    // User pressed Next - skip to next test
    };
    TestAction waitForStartOrStep();  // Block until Start or Step button is pressed
    
    // Step-by-step simulation control for manual testing
    void stepSimulation(World* world, int steps = 1);  // Advance N simulation steps
    TestAction waitForStep();     // Block until Step, Start (continue), or Next is pressed
    
    // Enhanced visual test helpers
    void updateDisplay(WorldInterface* world, const std::string& status = "");  // Update display with optional status
    void updateDisplayNoDelay(WorldInterface* world, const std::string& status = "");  // Update display without built-in delay
    void showInitialState(WorldInterface* world, const std::string& description);  // Show initial state and wait for start
    void showInitialStateWithStep(WorldInterface* world, const std::string& description);  // Show initial state with Start/Step choice
    void stepSimulation(WorldInterface* world, int steps, const std::string& stepDescription);  // Enhanced stepping with status
    void runContinuousSimulation(WorldInterface* world, int steps, const std::string& description);  // Run with rendering every frame
    
    // Test restart functionality
    void enableTestRestart() { restart_enabled_ = true; }
    void disableTestRestart() { restart_enabled_ = false; }
    bool isRestartEnabled() const { return restart_enabled_; }
    bool shouldRestartTest() const { return restart_requested_; }
    void clearRestartRequest() { restart_requested_ = false; }
    
    // Helper method for restartable test loops
    template<typename TestLogic>
    void runRestartableTest(TestLogic test_logic) {
        enableTestRestart();
        do {
            clearRestartRequest();
            waitForStart();
            // Always run the test logic when Start is pressed
            test_logic();
        } while (shouldRestartTest() && visual_mode_);
        disableTestRestart();
    }
    
    // Auto-scaling methods for visual tests
    void setAutoScaling(bool enabled) { auto_scaling_enabled_ = enabled; }
    bool isAutoScalingEnabled() const { return auto_scaling_enabled_; }
    void scaleDrawingAreaForWorld(uint32_t world_width, uint32_t world_height);
    void restoreOriginalCellSize();
    
    // Universal test configuration helpers
    void applyUniversalPhysicsDefaults(WorldB* world);
    void logWorldStateAscii(const WorldB* world, const std::string& description = "");
    void logWorldStateAscii(const World* world, const std::string& description = "");
    
    // Test setup completion - call this after setting up initial test conditions
    // Usage pattern: 1) createWorldB(), 2) setup materials/conditions, 3) logInitialTestState()
    void logInitialTestState(const WorldB* world, const std::string& test_description = "");
    void logInitialTestState(const World* world, const std::string& test_description = "");
    void logInitialTestState(const WorldInterface* world, const std::string& test_description = "");
    
    // Log current world state including material positions, velocities, and total mass
    void logWorldState(const WorldB* world, const std::string& context = "");
    
    // New unified simulation loop that eliminates visual/non-visual duplication
    // RecorderFunc should be a callable that takes an int timestep parameter
    // Example usage:
    //   runSimulationLoop(30, [&](int step) { 
    //       pressureHistory.push_back(cell.getPressure());
    //   }, "Testing pressure");
    template<typename RecorderFunc>
    void runSimulationLoop(int maxSteps, RecorderFunc recorder, 
                          const std::string& description = "",
                          std::function<bool()> earlyStopCondition = nullptr) {
        for (int step = 0; step < maxSteps; ++step) {
            // Let the recorder capture state/perform test logic
            recorder(step);
            
            // Handle display and stepping based on mode
            if (visual_mode_) {
                // Build status display if description provided
                if (!description.empty()) {
                    std::stringstream ss;
                    ss << description << " [Step " << (step + 1) << "/" << maxSteps << "]";
                    updateDisplay(getWorldInterface(), ss.str());
                }
                
                // Use existing step simulation which handles Step/Start modes
                stepSimulation(getWorldInterface(), 1, description);
            } else {
                // Non-visual mode: just advance time
                getWorldInterface()->advanceTime(0.016);
            }
            
            // Check early stop condition if provided
            if (earlyStopCondition && earlyStopCondition()) {
                break;
            }
        }
    }
    
    // Virtual method for derived tests to provide their world interface
    // Tests using WorldB should override this to return world.get()
    virtual WorldInterface* getWorldInterface() {
        return nullptr; // Base implementation - tests must override
    }

    // Test state
    bool visual_mode_ = false;
    std::unique_ptr<TestUI> ui_;
    std::string current_test_name_;
    
    // Auto-scaling state
    bool auto_scaling_enabled_ = true;  // Enable by default
    int original_cell_size_ = -1;       // Store original size for restoration
    
    // Test restart state
    bool restart_enabled_ = false;      // Enable restart functionality
    bool restart_requested_ = false;    // Set when user requests restart
};

// Utility functions for tests
bool isVisualModeEnabled();

// Function to get visual test coordinator (for backward compatibility)
VisualTestCoordinator& getVisualTestCoordinator(); 
