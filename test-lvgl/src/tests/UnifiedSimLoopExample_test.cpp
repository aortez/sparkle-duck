/**
 * @file UnifiedSimLoopExample_test.cpp
 * @brief Reference guide demonstrating best practices for the visual test framework
 * 
 * This file shows how to write tests that work seamlessly in both visual and
 * non-visual modes without code duplication, using the new runSimulationLoop pattern.
 */

#include <gtest/gtest.h>
#include "visual_test_runner.h"
#include "../World.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

/**
 * Example test class demonstrating the unified simulation loop pattern.
 * Update me when the pattern changes!
 * 
 * Key pattern: Your test class should:
 * 1. Inherit from VisualTestBase
 * 2. Create your world in SetUp()
 * 3. Override getWorldInterface() to return your world
 */
class UnifiedSimLoopExampleTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // PATTERN: Create your world using the framework helpers.
        // These apply universal defaults (adhesion off, cohesion off, etc.).
        world = createWorldB(5, 5);
        world->setGravity(9.81);
    }
    
    // PATTERN: Override this to provide world interface for the unified loop.
    // This allows runSimulationLoop to work with your world.
    World* getWorldInterface() override {
        return world.get();
    }
    
    std::unique_ptr<World> world;
};

/**
 * Example 1: Simple state tracking test (WITH RESTART - NEW STANDARD)
 * Shows the basic pattern for tracking physics state over time
 * Now includes restart functionality as the default pattern
 */
TEST_F(UnifiedSimLoopExampleTest, SimpleFallingWaterTracking) {
    spdlog::info("[EXAMPLE] Demonstrating unified simulation loop pattern with restart");
    
    // PATTERN: Use runRestartableTest as the standard wrapper.
    runRestartableTest([this]() {
        // PATTERN: Clear world state at the beginning (for restarts).
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        // PATTERN: Setup initial conditions.
        world->addMaterialAtCell(2, 0, MaterialType::WATER, 1.0);
        
        // PATTERN: Show initial state to user (works in both modes).
        showInitialState(world.get(), "Water falling demonstration");
        
        // PATTERN: Log initial state.
        logWorldState(world.get(), "Initial: Water at top");

        // PATTERN: Declare state tracking variables OUTSIDE the loop.
        // These will be captured by the lambda.
        std::vector<double> yPositions;
        std::vector<double> velocities;
        double lowestY = 0.0;
        bool hitBottom = false;
    
    // ============================================================
    // OLD WAY (DON'T DO THIS):
    // ============================================================
    // if (visual_mode_) {
    //     for (int step = 0; step < 30; step++) {
    //         // Find water cell.
    //         Cell* waterCell = findWaterCell();
    //         
    //         // Track state.
    //         yPositions.push_back(waterCell->y);
    //         velocities.push_back(waterCell->getVelocity().y);
    //         
    //         // Update display.
    //         std::stringstream ss;
    //         ss << "Step " << step << ": Y=" << waterCell->y;
    //         updateDisplay(world.get(), ss.str());
    //         
    //         // Step simulation.
    //         stepSimulation(world.get(), 1);
    //         
    //         // Check stopping condition.
    //         if (waterCell->y >= 4) {
    //             hitBottom = true;
    //             break;
    //         }
    //     }
    // } else {
    //     for (int step = 0; step < 30; step++) {
    //         // DUPLICATE: Same exact logic without display.
    //         Cell* waterCell = findWaterCell();
    //         yPositions.push_back(waterCell->y);
    //         velocities.push_back(waterCell->getVelocity().y);
    //         world->advanceTime(0.016);
    //         if (waterCell->y >= 4) {
    //             hitBottom = true;
    //             break;
    //         }
    //     }
    // }
    
        // ============================================================
        // NEW WAY (DO THIS INSTEAD):
        // ============================================================
        runSimulationLoop(30,  // max steps.
            [&](int step) {    // lambda captures local variables by reference.
            // PATTERN: Test logic goes here - runs identically in both modes.
            
            // Find water cell (it moves as it falls).
            Cell* waterCell = nullptr;
            for (uint32_t y = 0; y < world->getHeight(); y++) {
                for (uint32_t x = 0; x < world->getWidth(); x++) {
                    if (world->at(x, y).material_type == MaterialType::WATER &&
                        world->at(x, y).fill_ratio > 0.5) {
                        waterCell = &world->at(x, y);
                        lowestY = y;
                        break;
                    }
                }
                if (waterCell) break;
            }
            
            if (waterCell) {
                // Track state - this happens in BOTH modes.
                yPositions.push_back(lowestY);
                velocities.push_back(waterCell->getVelocity().y);
                
                // PATTERN: Optional visual-only display.
                // Only do this if you need custom display beyond the description.
                if (visual_mode_) {
                    std::stringstream ss;
                    ss << "Step " << step + 1 << "\n";
                    ss << "Water at Y=" << lowestY << "\n";
                    ss << "Velocity: " << waterCell->getVelocity().y;
                    updateDisplay(world.get(), ss.str());
                }
                
                // Check stopping condition.
                if (lowestY >= world->getHeight() - 1) {
                    hitBottom = true;
                    spdlog::info("Water hit bottom at step {}", step);
                }
            }
            
                // PATTERN: log the world state.
                logWorldState(world.get(), "Water falling");

                // NOTE: Physics advancement is handled by runSimulationLoop!
                // Don't call world->advanceTime() or stepSimulation() here.
            },
            "Water falling test",      // Description shown in visual mode.
            [&]() { return hitBottom; } // Optional: early stop condition.
        );
    
        // PATTERN: Verify results after the loop.
        // This runs in both visual and non-visual modes.
        EXPECT_TRUE(hitBottom) << "Water should reach the bottom";
        EXPECT_GT(velocities.back(), 0.0) << "Water should have downward velocity";
        
        // PATTERN: Use waitForRestartOrNext() for restart capability.
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
        
        spdlog::info("✅ Example test completed - water fell from Y=0 to Y={}", lowestY);
    }); // End of runRestartableTest.
}

/**
 * Example 2: Complex state tracking with multiple cells
 * Shows how to track relationships between cells
 */
TEST_F(UnifiedSimLoopExampleTest, PressureTrackingExample) {
    spdlog::info("[EXAMPLE] Pressure tracking with unified loop");
    
    // Setup scenario that might generate pressure.
    world->addMaterialAtCell(1, 1, MaterialType::WATER, 0.9);
    world->addMaterialAtCell(2, 1, MaterialType::WATER, 0.9);
    
    Cell& cell1 = world->at(1, 1);
    Cell& cell2 = world->at(2, 1);
    
    // Give them opposing velocities for collision.
    cell1.velocity = Vector2d{2.0, 0.0};
    cell2.velocity = Vector2d{-2.0, 0.0};
    
    showInitialState(world.get(), "Two water cells colliding");
    
    // PATTERN: State tracking variables.
    double maxPressure = 0.0;
    int pressureDetectedStep = -1;
    std::vector<double> pressureHistory;
    
    // PATTERN: Simple loop when you don't need custom display.
    runSimulationLoop(20, [&](int step) {
        double p1 = cell1.getHydrostaticPressure() + cell1.getDynamicPressure();
        double p2 = cell2.getHydrostaticPressure() + cell2.getDynamicPressure();
        double currentMax = std::max(p1, p2);
        
        pressureHistory.push_back(currentMax);
        
        // Track maximum and detection.
        if (currentMax > maxPressure) {
            maxPressure = currentMax;
            if (pressureDetectedStep < 0 && maxPressure > 0.001) {
                pressureDetectedStep = step;
                spdlog::info("Pressure first detected at step {}", step);
            }
        }
        
        // PATTERN: Periodic logging (works in both modes).
        if (step % 5 == 0) {
            spdlog::debug("Step {}: pressures = ({:.6f}, {:.6f})", step, p1, p2);
        }
    },
    "Collision pressure test"  // This description is shown in visual mode.
    // No early stop condition in this example.
    );
    
    // Verify and report results.
    spdlog::info("Maximum pressure observed: {:.6f}", maxPressure);
    if (pressureDetectedStep >= 0) {
        spdlog::info("Pressure detection latency: {} steps", pressureDetectedStep);
    }
    spdlog::info("✅ Pressure tracking example completed");
}

/**
 * Example 3: Stage-based progression
 * Shows how to track multiple stages/checkpoints in a test
 */
TEST_F(UnifiedSimLoopExampleTest, StageProgressionExample) {
    spdlog::info("[EXAMPLE] Stage-based test progression");
    
    // Setup: Water on left side of wall with hole.
    world->addMaterialAtCell(0, 2, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(1, 0, MaterialType::WALL, 1.0);
    world->addMaterialAtCell(1, 1, MaterialType::WALL, 1.0);
    // (1,2) is empty - the hole.
    world->addMaterialAtCell(1, 3, MaterialType::WALL, 1.0);
    world->addMaterialAtCell(1, 4, MaterialType::WALL, 1.0);
    
    showInitialState(world.get(), "Water flowing through hole in wall");
    
    // PATTERN: Track stages with descriptive names.
    struct Stage {
        std::string name;
        bool completed = false;
        int completedAtStep = -1;
    };
    
    Stage stages[] = {
        {"Water starts moving", false, -1},
        {"Water reaches hole", false, -1},
        {"Water passes through hole", false, -1},
        {"Water spreads on other side", false, -1}
    };
    
    runSimulationLoop(50, [&](int step) {
        // Check stage conditions.
        if (!stages[0].completed) {
            // Check if water has velocity.
            for (uint32_t y = 0; y < world->getHeight(); y++) {
                for (uint32_t x = 0; x < world->getWidth(); x++) {
                    auto& cell = world->at(x, y);
                    if (cell.material_type == MaterialType::WATER &&
                        cell.velocity.magnitude() > 0.1) {
                        stages[0].completed = true;
                        stages[0].completedAtStep = step;
                        spdlog::info("Stage 1 complete at step {}: {}", 
                                    step, stages[0].name);
                        break;
                    }
                }
            }
        }
        
        if (!stages[1].completed) {
            // Check if water at hole position.
            if (world->at(1, 2).material_type == MaterialType::WATER) {
                stages[1].completed = true;
                stages[1].completedAtStep = step;
                spdlog::info("Stage 2 complete at step {}: {}", 
                            step, stages[1].name);
            }
        }
        
        if (!stages[2].completed) {
            // Check if water past the wall (x > 1).
            for (uint32_t x = 2; x < world->getWidth(); x++) {
                for (uint32_t y = 0; y < world->getHeight(); y++) {
                    if (world->at(x, y).material_type == MaterialType::WATER) {
                        stages[2].completed = true;
                        stages[2].completedAtStep = step;
                        spdlog::info("Stage 3 complete at step {}: {}", 
                                    step, stages[2].name);
                        goto stage3_done;  // Break nested loop.
                    }
                }
            }
            stage3_done:;
        }
        
        // PATTERN: Build status for visual mode.
        if (visual_mode_) {
            std::stringstream ss;
            ss << "Step " << step + 1 << " - Stage Progress:\n";
            for (const auto& stage : stages) {
                ss << (stage.completed ? "✓ " : "○ ") << stage.name;
                if (stage.completed) {
                    ss << " (step " << stage.completedAtStep << ")";
                }
                ss << "\n";
            }
            updateDisplay(world.get(), ss.str());
        }
    },
    "Stage progression test",
    [&]() {
        // PATTERN: Stop early if all stages complete.
        return std::all_of(std::begin(stages), std::end(stages),
                          [](const Stage& s) { return s.completed; });
    });
    
    // Report results.
    spdlog::info("Stage progression results:");
    for (const auto& stage : stages) {
        if (stage.completed) {
            spdlog::info("  ✓ {} - completed at step {}", 
                        stage.name, stage.completedAtStep);
        } else {
            spdlog::info("  ✗ {} - not completed", stage.name);
        }
    }
}

/**
 * Example 4: Restartable test
 * Shows how to make a test that can be restarted after completion
 */
TEST_F(UnifiedSimLoopExampleTest, RestartableTestExample) {
    spdlog::info("[EXAMPLE] Demonstrating restartable test pattern");
    
    // PATTERN: Use runRestartableTest to enable restart functionality.
    runRestartableTest([this]() {
        // PATTERN: Clear world state at the beginning of each run.
        // This ensures a clean state for restarts.
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        // Setup initial conditions.
        world->addMaterialAtCell(2, 0, MaterialType::SAND, 1.0);
        
        // PATTERN: showInitialState works correctly within runRestartableTest.
        // It won't disable restart when already in a restart loop.
        showInitialState(world.get(), "Sand falling test - restartable");
        
        // Run the simulation.
        bool hitBottom = false;
        runSimulationLoop(30, [&](int step) {
            // Find sand.
            for (uint32_t y = 0; y < world->getHeight(); ++y) {
                for (uint32_t x = 0; x < world->getWidth(); ++x) {
                    if (world->at(x, y).material_type == MaterialType::SAND &&
                        world->at(x, y).fill_ratio > 0.5) {
                        if (y >= world->getHeight() - 1) {
                            hitBottom = true;
                        }
                        break;
                    }
                }
            }
            
            // PATTERN: Log world state every step.
            logWorldState(world.get(), fmt::format("Step {}: Sand falling", step));
        }, "Sand falling");
        
        // PATTERN: Use waitForRestartOrNext() instead of waitForNext().
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
        
        spdlog::info("✅ Restartable test iteration completed");
    });
}

// ============================================================
// SUMMARY OF BEST PRACTICES (UPDATED WITH RESTART AS STANDARD):
// ============================================================
// 
// 1. ALWAYS USE runRestartableTest() as the outer wrapper.
//    - This is now the standard pattern for all visual tests.
//    - Enables test restart functionality automatically.
//    - Clear world state at the beginning of the lambda.
// 
// 2. USE runSimulationLoop() inside runRestartableTest().
//    - Eliminates visual/non-visual code duplication.
//    - Pass a lambda that captures your state variables.
//    - Physics advancement is handled automatically.
// 
// 3. DECLARE state tracking variables BEFORE the simulation loop.
//    - Capture them by reference [&] in the lambda.
//    - They'll be accessible after the loop for assertions.
// 
// 4. PUT test logic in the lambda that works for BOTH modes.
//    - Don't duplicate code for visual vs non-visual.
//    - The framework handles the differences.
// 
// 5. USE visual_mode_ ONLY for optional visual enhancements.
//    - Custom status displays.
//    - Additional visual feedback.
//    - Not required - the description parameter often suffices.
// 
// 6. DON'T call world->advanceTime() or stepSimulation() in the lambda.
//    - The framework handles this based on the mode.
// 
// 7. USE the optional early stop condition when appropriate.
//    - Return true when the test should end early.
//    - Useful for "wait until X happens" tests.
// 
// 8. END WITH waitForRestartOrNext() in visual mode.
//    - Use this instead of waitForNext().
//    - Allows users to restart the test or continue to next.
// 
// 9. VERIFY results after the loop.
//    - Use EXPECT_* macros as normal.
//    - Log summary information.
// 
// 10. KEEP the lambda focused on one timestep.
//     - Don't try to do multiple steps inside the lambda.
//     - Let the framework handle the loop.
// 
// ============================================================
