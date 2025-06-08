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

#include "../World.h"
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
};

// Base class for all visual tests
class VisualTestBase : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    
    // Helper to create a world with or without UI
    std::unique_ptr<World> createWorld(uint32_t width, uint32_t height);
    
    // Helper method to run visual simulation if enabled, otherwise just advance time
    void runSimulation(World* world, int steps = 60, const std::string& description = "");
    
    // Test state
    bool visual_mode_ = false;
    std::unique_ptr<TestUI> ui_;
    std::string current_test_name_;
};

// Utility functions for tests
bool isVisualModeEnabled();

// Function to get visual test coordinator (for backward compatibility)
VisualTestCoordinator& getVisualTestCoordinator(); 
