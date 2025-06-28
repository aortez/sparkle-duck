#include "../World.h"
#include "../WorldSetup.h"
#include "TestUI.h"

#include <gtest/gtest.h>
#include <memory>
#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

class WaterPressure180Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a medium-sized world for better visualization.
        width = 10;
        height = 10;
        createWorld();
        
        // Set up water physics parameters for testing.
        world->setWaterPressureThreshold(0.001); // Low threshold to trigger pressure effects.
        Cell::setCohesionStrength(0.1);
        Cell::setViscosityFactor(0.1);
        
        // Disable fragmentation for cleaner testing.
        World::DIRT_FRAGMENTATION_FACTOR = 0.0;
        
        // Try to initialize UI - will be null if not available.
        initializeUI();
    }

    void createWorld() {
        world = std::make_unique<World>(width, height, nullptr); // No draw area for testing.
        world->setAddParticlesEnabled(false);
        world->setGravity(9.81); // Keep gravity for realistic behavior.
    }
    
    void initializeUI() {
        // For now, just set ui to nullptr to avoid LVGL dependency issues.
        // In the future, this could try to create UI if display is available.
        ui = nullptr;
        std::cout << "Running in headless mode (no UI visualization)" << std::endl;
    }

    void updateStatus(const std::string& status) {
        std::cout << "[TEST STATUS] " << status << std::endl;
        
        // If UI is available in the future, update it here.
        if (ui) {
            // ui->updateTestLabel(status);
            // if (world) world->draw();
        }
        
        // Small delay for reading console output.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    void runSimulationSteps(int steps, const std::string& description) {
        updateStatus(description + " - Starting " + std::to_string(steps) + " steps");
        
        for (int step = 0; step < steps; step++) {
            world->advanceTime(0.016); // 16ms timestep.
            
            // Print progress every 10 steps.
            if (step % 10 == 0 && step > 0) {
                updateStatus(description + " - Step " + std::to_string(step) + "/" + std::to_string(steps));
            }
        }
        
        updateStatus(description + " - Completed");
    }
    
    void printWorldState(const std::string& title) {
        std::cout << "\n=== " << title << " ===" << std::endl;
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                const Cell& cell = world->at(x, y);
                char symbol = '.';
                if (cell.dirt > 0.1) symbol = '#'; // Dirt.
                else if (cell.water > 0.1) symbol = '~'; // Water.
                std::cout << symbol;
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    void TearDown() override {
        // Restore default values.
        World::DIRT_FRAGMENTATION_FACTOR = 0.1;
        world.reset();
        ui.reset();
    }

    std::unique_ptr<World> world;
    std::unique_ptr<TestUI> ui;
    uint32_t width;
    uint32_t height;
};

TEST_F(WaterPressure180Test, WaterPressureDeflectionBasic) {
    spdlog::info("Starting WaterPressure180Test::WaterPressureDeflectionBasic test");
    updateStatus("Setting up basic water pressure test");
    
    // Create a scenario where water has pressure but direct path is blocked.
    // Place water in center with some velocity and pressure.
    const int centerX = width / 2;
    const int centerY = height / 2;
    
    // Clear the world first.
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            world->at(x, y).dirt = 0.0;
            world->at(x, y).water = 0.0;
            world->at(x, y).com = Vector2d(0.0, 0.0);
            world->at(x, y).v = Vector2d(0.0, 0.0);
        }
    }
    
    // Place water in center with downward velocity.
    world->at(centerX, centerY).water = 1.0;
    world->at(centerX, centerY).v = Vector2d(0.0, 2.0); // Strong downward velocity.
    world->at(centerX, centerY).com = Vector2d(0.0, 0.4); // COM deflected downward.
    
    // Block the direct downward path with dirt.
    world->at(centerX, centerY + 1).dirt = 1.0;
    
    // Also block some side paths to force 180-degree arc logic.
    world->at(centerX - 1, centerY + 1).dirt = 1.0; // Block lower-left diagonal.
    
    updateStatus("Water placed with blocked downward path");
    printWorldState("Initial Setup");
    
    std::cout << "=== Initial Water Pressure 180° Test ===" << std::endl;
    std::cout << "Water at (" << centerX << "," << centerY << ") with blocked downward path" << std::endl;
    std::cout << "Expected: Water should find alternative path within 180° downward arc" << std::endl;
    
    // Store initial state.
    double initialWater __attribute__((unused)) = world->at(centerX, centerY).water;
    Vector2d initialVelocity __attribute__((unused)) = world->at(centerX, centerY).v;
    
    // Run simulation with status updates.
    bool waterMoved = false;
    Vector2d finalPosition = Vector2d(centerX, centerY);
    
    runSimulationSteps(20, "Building up pressure");
    printWorldState("After Pressure Buildup");
    
    for (int step = 0; step < 100; step++) {
        world->advanceTime(0.016); // 16ms timestep.
        
        // Update status every 10 steps.
        if (step % 10 == 0) {
            updateStatus("Running simulation - step " + std::to_string(step));
        }
        
        // Check for water movement to adjacent cells.
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                
                int nx = centerX + dx;
                int ny = centerY + dy;
                
                if (nx >= 0 && nx < (int)width && ny >= 0 && ny < (int)height) {
                    if (world->at(nx, ny).water > 0.1) {
                        waterMoved = true;
                        finalPosition = Vector2d(nx, ny);
                        
                        std::cout << "Step " << step << ": Water moved to (" << nx << "," << ny << ")" << std::endl;
                        std::cout << "  Direction vector: (" << (nx - centerX) << "," << (ny - centerY) << ")" << std::endl;
                        
                        // Check if movement follows 180-degree downward arc rules.
                        Vector2d moveDirection = Vector2d(nx - centerX, ny - centerY).normalize();
                        Vector2d gravityDirection = Vector2d(0.0, 1.0);
                        double gravityAlignment = moveDirection.dot(gravityDirection);
                        
                        std::cout << "  Gravity alignment: " << gravityAlignment << std::endl;
                        
                        // Water should prefer movement that has some downward component.
                        // or at least not strongly upward (within 180° arc).
                        EXPECT_GE(gravityAlignment, -0.2) << "Water moved too far upward, outside 180° arc";
                        
                        updateStatus("Water moved! Analyzing results...");
                        printWorldState("After Water Movement");
                        break;
                    }
                }
            }
            if (waterMoved) break;
        }
        if (waterMoved) break;
    }
    
    updateStatus("Test completed - checking results");
    printWorldState("Final State");
    
    // Verify that water found an alternative path.
    EXPECT_TRUE(waterMoved) << "Water should have moved to an alternative cell when direct path blocked";
    
    std::cout << "=== Test Results ===" << std::endl;
    std::cout << "Water moved: " << (waterMoved ? "YES" : "NO") << std::endl;
    if (waterMoved) {
        std::cout << "Final position: (" << finalPosition.x << "," << finalPosition.y << ")" << std::endl;
    }
}

TEST_F(WaterPressure180Test, WaterPressureVsDirectionPreference) {
    spdlog::info("Starting WaterPressure180Test::WaterPressureVsDirectionPreference test");
    updateStatus("Testing water pressure vs velocity direction preference");
    
    // Create a scenario where water has both pressure and velocity.
    // but they point in different directions within the 180° arc.
    const int centerX = width / 2;
    const int centerY = height / 2;
    
    // Clear the world.
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            world->at(x, y).dirt = 0.0;
            world->at(x, y).water = 0.0;
            world->at(x, y).com = Vector2d(0.0, 0.0);
            world->at(x, y).v = Vector2d(0.0, 0.0);
        }
    }
    
    // Place water with rightward velocity but downward pressure (COM deflection).
    world->at(centerX, centerY).water = 1.0;
    world->at(centerX, centerY).v = Vector2d(2.0, 0.0); // Strong rightward velocity.
    world->at(centerX, centerY).com = Vector2d(0.0, 0.5); // COM deflected downward (creates downward pressure).
    
    // Block direct right movement.
    world->at(centerX + 1, centerY).dirt = 1.0;
    
    // Block direct downward movement.
    world->at(centerX, centerY + 1).dirt = 1.0;
    
    // Leave diagonal down-right open - this should be preferred as it balances.
    // both velocity preference (right) and pressure direction (down).
    
    updateStatus("Water with conflicting velocity and pressure directions");
    
    std::cout << "=== Velocity vs Pressure Direction Test ===" << std::endl;
    std::cout << "Water velocity: RIGHT, Pressure: DOWN" << std::endl;
    std::cout << "Expected: Should prefer down-right diagonal (compromise direction)" << std::endl;
    
    bool waterMoved = false;
    Vector2d finalDirection;
    
    runSimulationSteps(20, "Building up conflicting forces");
    
    for (int step = 0; step < 100; step++) {
        world->advanceTime(0.016);
        
        // Update status every 10 steps.
        if (step % 10 == 0) {
            updateStatus("Running direction preference test - step " + std::to_string(step));
        }
        
        // Check for water movement.
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                
                int nx = centerX + dx;
                int ny = centerY + dy;
                
                if (nx >= 0 && nx < (int)width && ny >= 0 && ny < (int)height) {
                    if (world->at(nx, ny).water > 0.1) {
                        waterMoved = true;
                        finalDirection = Vector2d(dx, dy);
                        
                        std::cout << "Step " << step << ": Water moved to (" << nx << "," << ny << ")" << std::endl;
                        std::cout << "  Direction: (" << dx << "," << dy << ")" << std::endl;
                        
                        // Analyze the chosen direction.
                        Vector2d velocityDirection = Vector2d(2.0, 0.0).normalize(); // Original velocity direction.
                        Vector2d pressureDirection = Vector2d(0.0, 1.0).normalize(); // Pressure from COM deflection.
                        Vector2d moveDirection = finalDirection.normalize();
                        
                        double velocityAlignment = moveDirection.dot(velocityDirection);
                        double pressureAlignment = moveDirection.dot(pressureDirection);
                        
                        std::cout << "  Velocity alignment: " << velocityAlignment << std::endl;
                        std::cout << "  Pressure alignment: " << pressureAlignment << std::endl;
                        
                        // The chosen direction should consider both velocity and pressure.
                        // For down-right diagonal: velocity alignment ≈ 0.7, pressure alignment ≈ 0.7.
                        if (dx == 1 && dy == 1) {
                            std::cout << "  ✓ Chose down-right diagonal (good compromise)" << std::endl;
                        }
                        
                        updateStatus("Water found compromise direction!");
                        break;
                    }
                }
            }
            if (waterMoved) break;
        }
        if (waterMoved) break;
    }
    
    updateStatus("Direction preference test completed");
    
    EXPECT_TRUE(waterMoved) << "Water should have found an alternative path";
    
    std::cout << "=== Direction Preference Results ===" << std::endl;
    std::cout << "Water moved: " << (waterMoved ? "YES" : "NO") << std::endl;
    if (waterMoved) {
        std::cout << "Chosen direction: (" << finalDirection.x << "," << finalDirection.y << ")" << std::endl;
    }
}

TEST_F(WaterPressure180Test, WaterPressureArcLimits) {
    spdlog::info("Starting WaterPressure180Test::WaterPressureArcLimits test");
    updateStatus("Testing 180-degree arc limits");
    
    // Test that water truly respects the 180-degree downward arc.
    // by creating scenarios where upward movement would be the shortest path.
    // but should be rejected.
    
    const int centerX = width / 2;
    const int centerY = height / 2;
    
    // Clear the world.
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            world->at(x, y).dirt = 0.0;
            world->at(x, y).water = 0.0;
            world->at(x, y).com = Vector2d(0.0, 0.0);
            world->at(x, y).v = Vector2d(0.0, 0.0);
        }
    }
    
    // Place water with pressure.
    world->at(centerX, centerY).water = 1.0;
    world->at(centerX, centerY).v = Vector2d(0.0, 1.0); // Downward velocity.
    world->at(centerX, centerY).com = Vector2d(0.0, 0.4); // Downward pressure.
    
    // Block all downward and sideways paths, leave only upward paths.
    world->at(centerX, centerY + 1).dirt = 1.0;     // Down.
    world->at(centerX - 1, centerY).dirt = 1.0;     // Left.
    world->at(centerX + 1, centerY).dirt = 1.0;     // Right.
    world->at(centerX - 1, centerY + 1).dirt = 1.0; // Down-left.
    world->at(centerX + 1, centerY + 1).dirt = 1.0; // Down-right.
    
    // Leave upward paths open (these should be rejected by 180° rule).
    // world->at(centerX, centerY - 1) is open (directly up).
    // world->at(centerX - 1, centerY - 1) is open (up-left).
    // world->at(centerX + 1, centerY - 1) is open (up-right).
    
    updateStatus("Water surrounded - only upward paths available");
    
    std::cout << "=== 180-Degree Arc Limit Test ===" << std::endl;
    std::cout << "All downward/sideways paths blocked, only upward paths available" << std::endl;
    std::cout << "Expected: Water should NOT move upward (outside 180° arc)" << std::endl;
    
    bool waterMovedUpward = false;
    bool waterMovedAtAll = false;
    
    runSimulationSteps(20, "Building pressure - all downward paths blocked");
    
    for (int step = 0; step < 100; step++) {
        world->advanceTime(0.016);
        
        // Update status every 10 steps.
        if (step % 10 == 0) {
            updateStatus("Testing arc limits - step " + std::to_string(step));
        }
        
        // Check for any water movement.
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                
                int nx = centerX + dx;
                int ny = centerY + dy;
                
                if (nx >= 0 && nx < (int)width && ny >= 0 && ny < (int)height) {
                    if (world->at(nx, ny).water > 0.1) {
                        waterMovedAtAll = true;
                        
                        if (dy < 0) { // Moved upward.
                            waterMovedUpward = true;
                            std::cout << "Step " << step << ": ✗ Water moved upward to (" << nx << "," << ny << ")" << std::endl;
                            updateStatus("ERROR: Water moved upward!");
                        } else {
                            std::cout << "Step " << step << ": Water moved to (" << nx << "," << ny << ")" << std::endl;
                            updateStatus("Water moved within allowed arc");
                        }
                        break;
                    }
                }
            }
            if (waterMovedAtAll) break;
        }
        if (waterMovedAtAll) break;
    }
    
    updateStatus("Arc limit test completed");
    
    // Water should not move upward due to 180° arc restriction.
    EXPECT_FALSE(waterMovedUpward) << "Water should not move upward (outside 180° downward arc)";
    
    std::cout << "=== Arc Limit Results ===" << std::endl;
    std::cout << "Water moved at all: " << (waterMovedAtAll ? "YES" : "NO") << std::endl;
    std::cout << "Water moved upward: " << (waterMovedUpward ? "YES" : "NO") << std::endl;
    
    if (!waterMovedUpward && !waterMovedAtAll) {
        std::cout << "✓ Correctly refused to move upward - 180° arc respected" << std::endl;
    }
} 
