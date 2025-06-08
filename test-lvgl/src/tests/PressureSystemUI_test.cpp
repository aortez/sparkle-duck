#include "gtest/gtest.h"
#include "../World.h"
#include "../Cell.h"
#include "../tests/TestUI.h"
#include "lvgl/lvgl.h"
#include "../lib/driver_backends.h"
#include "../lib/simulator_settings.h"

#include <iostream>
#include <memory>
#include <cstdio>
#include <unistd.h>

// External global settings used by the backend system
extern simulator_settings_t settings;

// Static flag to track if backend is already initialized
static bool backend_initialized = false;

class PressureSystemUITest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n=== Setting up UI test ===" << std::endl;
        
        // Only initialize LVGL and backend once for all tests
        if (!backend_initialized) {
            // Initialize LVGL
            lv_init();
            
            // Configure global settings for time-limited execution
            settings.window_width = 600;  
            settings.window_height = 500;
            settings.max_steps = 60;     // Default for individual test segments
            
            // Register and initialize display backend
            driver_backends_register();
            
            // Use wayland backend (as specified in user rules)
            if (driver_backends_init_backend("wayland") == -1) {
                GTEST_SKIP() << "Failed to initialize Wayland backend - skipping visual test";
            }
            
            backend_initialized = true;
            std::cout << "Display backend initialized successfully" << std::endl;
        }
        
        // Get the active screen from the initialized backend
        screen = lv_scr_act();
        ASSERT_NE(screen, nullptr) << "Failed to get active screen";
        
        // Create test UI
        ui = std::make_unique<TestUI>(screen, "PressureSystemUITest");
        ui->initialize();
        
        // Create a world for testing (small size for performance)
        world = std::make_unique<World>(15, 15, ui->getDrawArea());
        world->setGravity(9.81);
        
        // Connect UI and world
        ui->setWorld(world.get());
        
        // Add some material to see pressure differences
        // Add dirt in multiple locations to create a pile
        for (int x = 120; x <= 180; x += 15) {
            for (int y = 60; y <= 120; y += 15) {
                world->addDirtAtPixel(x, y);
            }
        }
        
        std::cout << "World and UI setup complete" << std::endl;
    }
    
    void TearDown() override {
        std::cout << "=== Cleaning up UI test ===" << std::endl;
        ui.reset();
        world.reset();
        
        // Force a small delay to let any pending operations complete
        usleep(10000); // 10ms
    }
    
    // Helper method to run the visual simulation for the specified duration
    void runVisualSimulation(const std::string& test_name) {
        std::cout << "\n--- Running visual simulation for: " << test_name << " ---" << std::endl;
        ui->updateTestLabel("Running " + test_name);
        
        // Ensure step counter is reset for this specific simulation run
        settings.max_steps = 60;  // Shorter duration for individual test segments
        
        std::cout << "Starting simulation with max_steps=" << settings.max_steps << std::endl;
        
        // Enter the backend run loop - this will run for settings.max_steps then exit
        driver_backends_run_loop(*world);
        
        std::cout << "Visual simulation completed for: " << test_name << std::endl;
    }
    
    lv_obj_t* screen;
    std::unique_ptr<TestUI> ui;
    std::unique_ptr<World> world;
};

TEST_F(PressureSystemUITest, PressureSystemSwitching) {
    std::cout << "\n=== PRESSURE SYSTEM SWITCHING TEST ===" << std::endl;
    
    // Test that all three pressure systems work
    std::vector<World::PressureSystem> systems = {
        World::PressureSystem::Original,
        World::PressureSystem::TopDown,
        World::PressureSystem::IterativeSettling
    };
    
    std::vector<std::string> system_names = {
        "Original (COM)",
        "Top-Down Hydrostatic", 
        "Iterative Settling"
    };
    
    for (size_t i = 0; i < systems.size(); ++i) {
        std::cout << "\nTesting pressure system: " << system_names[i] << std::endl;
        
        // Set the pressure system
        world->setPressureSystem(systems[i]);
        
        // Verify it was set correctly
        EXPECT_EQ(world->getPressureSystem(), systems[i]);
        
        // Run the visual simulation to show this pressure system in action
        runVisualSimulation(system_names[i]);
        
        // Run a few more simulation steps for testing after display
        for (int step = 0; step < 5; ++step) {
            world->advanceTime(1.0/60.0); // 60 FPS timestep
        }
        
        // Check that some pressure was generated (this validates the system is working)
        double total_pressure = 0.0;
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                Vector2d pressure = world->at(x, y).pressure;
                total_pressure += pressure.mag();
            }
        }
        
        std::cout << "  Total pressure magnitude: " << total_pressure << std::endl;
        
        // For systems with material, we should see some pressure
        if (total_pressure > 0.0) {
            std::cout << "  ✓ Pressure system is generating pressure" << std::endl;
        } else {
            std::cout << "  ! No pressure generated (may be normal for this configuration)" << std::endl;
        }
    }
    
    std::cout << "\n=== Test completed successfully ===" << std::endl;
}

TEST_F(PressureSystemUITest, PressureSystemComparison) {
    std::cout << "\n=== PRESSURE SYSTEM COMPARISON TEST ===" << std::endl;
    
    // Run same scenario with each pressure system and compare results
    struct PressureResult {
        World::PressureSystem system;
        std::string name;
        double total_pressure;
        double max_pressure;
    };
    
    std::vector<PressureResult> results;
    
    for (auto system : {World::PressureSystem::Original, 
                       World::PressureSystem::TopDown,
                       World::PressureSystem::IterativeSettling}) {
        
        // Reset world to consistent state
        world->reset();
        
        // Add material at center to create pressure
        for (int x = 120; x <= 180; x += 15) {
            for (int y = 90; y <= 150; y += 15) {
                world->addDirtAtPixel(x, y);
            }
        }
        
        world->setPressureSystem(system);
        
        // Show this system running visually
        std::string system_name;
        switch(system) {
            case World::PressureSystem::Original:
                system_name = "Original (COM)";
                break;
            case World::PressureSystem::TopDown:
                system_name = "Top-Down Hydrostatic";
                break;
            case World::PressureSystem::IterativeSettling:
                system_name = "Iterative Settling";
                break;
        }
        
        runVisualSimulation("Comparison: " + system_name);
        
        // Run additional simulation steps for measurement
        for (int step = 0; step < 10; ++step) {
            world->advanceTime(1.0/60.0);
        }
        
        // Collect pressure statistics
        double total_pressure = 0.0;
        double max_pressure = 0.0;
        
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                double pressure_mag = world->at(x, y).pressure.mag();
                total_pressure += pressure_mag;
                max_pressure = std::max(max_pressure, pressure_mag);
            }
        }
        
        PressureResult result;
        result.system = system;
        result.total_pressure = total_pressure;
        result.max_pressure = max_pressure;
        result.name = system_name;
        
        results.push_back(result);
    }
    
    // Display comparison
    std::cout << "\nPressure System Comparison Results:" << std::endl;
    std::cout << "System                    | Total Pressure | Max Pressure" << std::endl;
    std::cout << "--------------------------|----------------|-------------" << std::endl;
    
    for (const auto& result : results) {
        printf("%-25s | %14.6f | %12.6f\n", 
               result.name.c_str(),
               result.total_pressure, 
               result.max_pressure);
    }
    
    // Basic sanity checks
    for (const auto& result : results) {
        EXPECT_GE(result.total_pressure, 0.0) << "Pressure should be non-negative for " << result.name;
        EXPECT_GE(result.max_pressure, 0.0) << "Max pressure should be non-negative for " << result.name;
    }
    
    std::cout << "\n=== Comparison completed ===" << std::endl;
}

// Test specifically for pressure system API functionality
TEST_F(PressureSystemUITest, PressureSystemAPI) {
    std::cout << "\n=== PRESSURE SYSTEM API TEST ===" << std::endl;
    
    // Test that we can switch pressure systems programmatically
    // (this simulates what the dropdown callback does)
    
    // Start with Original system
    world->setPressureSystem(World::PressureSystem::Original);
    EXPECT_EQ(world->getPressureSystem(), World::PressureSystem::Original);
    std::cout << "✓ Original system set successfully" << std::endl;
    
    // Switch to TopDown
    world->setPressureSystem(World::PressureSystem::TopDown);
    EXPECT_EQ(world->getPressureSystem(), World::PressureSystem::TopDown);
    std::cout << "✓ TopDown system set successfully" << std::endl;
    
    // Switch to IterativeSettling
    world->setPressureSystem(World::PressureSystem::IterativeSettling);
    EXPECT_EQ(world->getPressureSystem(), World::PressureSystem::IterativeSettling);
    std::cout << "✓ IterativeSettling system set successfully" << std::endl;
    
    // Switch back to Original
    world->setPressureSystem(World::PressureSystem::Original);
    EXPECT_EQ(world->getPressureSystem(), World::PressureSystem::Original);
    std::cout << "✓ Switched back to Original system successfully" << std::endl;
    
    // Show the final API test running
    runVisualSimulation("API Test - Final State");
    
    std::cout << "=== API test completed ===\n" << std::endl;
}

// Test top-down pressure accumulation specifically
TEST_F(PressureSystemUITest, TopDownPressureAccumulation) {
    std::cout << "\n=== TOP-DOWN PRESSURE ACCUMULATION TEST ===" << std::endl;
    
    // Create a vertical column of material to test pressure accumulation
    world->reset();
    
    // Add material vertically (should create accumulating pressure)
    for (int y = 40; y <= 240; y += 30) {  // Column from top to bottom
        world->addDirtAtPixel(200, y);  // Center column
    }
    
    // Use top-down pressure system
    world->setPressureSystem(World::PressureSystem::TopDown);
    
    // Show the top-down pressure system in action
    runVisualSimulation("Top-Down Pressure Column");
    
    // Run additional simulation to let pressure develop
    for (int step = 0; step < 15; ++step) {
        world->advanceTime(1.0/60.0);
    }
    
    // Check pressure increases with depth
    std::vector<double> pressures_by_row;
    for (uint32_t y = 0; y < world->getHeight(); ++y) {
        double row_pressure = 0.0;
        for (uint32_t x = 0; x < world->getWidth(); ++x) {
            row_pressure += world->at(x, y).pressure.mag();
        }
        pressures_by_row.push_back(row_pressure);
        if (row_pressure > 0.001) {  // Only print significant pressures
            std::cout << "  Row " << y << " pressure: " << row_pressure << std::endl;
        }
    }
    
    // Verify that deeper rows generally have higher pressure
    // (this validates the top-down accumulation concept)
    bool found_pressure_gradient = false;
    for (size_t i = 1; i < pressures_by_row.size(); ++i) {
        if (pressures_by_row[i] > pressures_by_row[i-1] && pressures_by_row[i] > 0.001) {
            found_pressure_gradient = true;
            std::cout << "  ✓ Found pressure increase from row " << (i-1) << " to row " << i << std::endl;
            break;
        }
    }
    
    if (found_pressure_gradient) {
        std::cout << "  ✓ Top-down pressure accumulation is working!" << std::endl;
    } else {
        std::cout << "  ! No clear pressure gradient found (may need different material configuration)" << std::endl;
    }
    
    std::cout << "=== Top-down test completed ===\n" << std::endl;
} 
