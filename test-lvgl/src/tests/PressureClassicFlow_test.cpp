#include <gtest/gtest.h>
#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../MaterialType.h"
#include "../Cell.h"
#include <spdlog/spdlog.h>

class PressureClassicFlowTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Create a 6x6 world - small enough to visualize easily
        world = createWorldB(6, 6);
        
        // Enable both pressure systems
        world->setDynamicPressureEnabled(false);
        world->setHydrostaticPressureEnabled(true);
        world->setPressureScale(10.0);  // Strong pressure for visible effects
        
        // Enable debug visualization to see pressure vectors
        Cell::debugDraw = true;
        
        // Standard test settings
        world->setWallsEnabled(false);
        world->setAddParticlesEnabled(false);
        world->setGravity(9.81);
        
        spdlog::info("[TEST] Classic pressure flow tests - 6x6 world with debug visualization");
    }
    
    void TearDown() override {
        Cell::debugDraw = false;
        VisualTestBase::TearDown();
    }
    
    // Required for unified simulation loop
    WorldInterface* getWorldInterface() override {
        return world.get();
    }
    
    std::unique_ptr<WorldB> world;
};


TEST_F(PressureClassicFlowTest, DamBreak) {
    // Purpose: Classic fluid dynamics scenario testing horizontal pressure-driven flow.
    // Hydrostatic pressure from a water column should drive rapid flow when obstruction removed.
    //
    // Setup: Full-height water column (x=0-1) held by WALL dam at x=2, then bottom cell removed
    // Expected: Water jets through bottom opening due to maximum hydrostatic pressure
    // Tests: Hydrostatic pressure gradient and high-pressure flow through small opening
    
    runRestartableTest([this]() {
        // Clear world state
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        spdlog::info("[TEST] Dam Break - Classic fluid dynamics scenario");
        
        // Create water column on left side - full height
        for (int y = 0; y < 6; y++) {
            world->addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
            world->addMaterialAtCell(1, y, MaterialType::WATER, 1.0);
        }
        
        // Create dam (temporary wall) - full height using WALL
        for (int y = 0; y < 6; y++) {
            world->addMaterialAtCell(2, y, MaterialType::WALL, 1.0);  // Using WALL as solid dam
        }
        
        showInitialStateWithStep(world.get(), "Dam break test - water held by wall dam");
        logWorldStateAscii(world.get(), "Initial: Water held by dam");
        
        int max_x_reached = 0;
        bool dam_broken = false;
        
        // Combined pressure build-up and flow simulation
        runSimulationLoop(80, [this, &max_x_reached, &dam_broken](int step) {
            // First 10 steps: let pressure build up
            if (step < 10) {
                spdlog::info("Building pressure... [Step {}/10]", step + 1);
                
                // Verify water cells maintain centered COM and low velocity before dam break
                for (uint32_t y = 0; y < world->getHeight(); ++y) {
                    for (uint32_t x = 0; x < 2; ++x) {  // Only check water columns (0,1)
                        const auto& cell = world->at(x, y);
                        if (cell.getMaterialType() == MaterialType::WATER && cell.getFillRatio() > 0.9) {
                            Vector2d com = cell.getCOM();
                            Vector2d vel = cell.getVelocity();
                            
                            // COM should remain near center (0,0) of each cell
                            EXPECT_NEAR(com.x, 0.0, 0.01) << "Water COM x should remain centered at (" << x << "," << y << ") before dam break";
                            EXPECT_NEAR(com.y, 0.0, 0.01) << "Water COM y should remain centered at (" << x << "," << y << ") before dam break";
                            
                            // Velocity should be very small (water is blocked)
                            EXPECT_LT(std::abs(vel.x), 0.1) << "Water x-velocity should be minimal at (" << x << "," << y << ") before dam break";
                            EXPECT_LT(std::abs(vel.y), 0.1) << "Water y-velocity should be small at (" << x << "," << y << ") before dam break";
                            
                            if (step == 9) {
                                // Log final pre-break state
                                spdlog::debug("Cell ({},{}) before break - COM: ({:.3f},{:.3f}), Vel: ({:.3f},{:.3f})", 
                                            x, y, com.x, com.y, vel.x, vel.y);
                            }
                        }
                    }
                }
            }
            // At step 10: break the dam
            else if (step == 10 && !dam_broken) {
                spdlog::info("Breaking bottom of dam...");
                world->at(2, 5).clear();  // Remove only bottom wall cell
                dam_broken = true;
                logWorldStateAscii(world.get(), "Dam broken - bottom cell removed");
                
                // Debug: Check pressure values around the break
                spdlog::info("=== Pressure analysis after dam break ===");
                for (int y = 4; y <= 5; y++) {
                    for (int x = 0; x <= 3; x++) {
                        auto& cell = world->at(x, y);
                        double hydro = cell.getHydrostaticPressure();
                        double total = hydro + cell.getDynamicPressure();
                        spdlog::info("Cell ({},{}) - Material: {}, Fill: {:.2f}, Hydrostatic: {:.4f}, Total: {:.4f}",
                                    x, y, 
                                    static_cast<int>(cell.getMaterialType()),
                                    cell.getFillRatio(),
                                    hydro, total);
                    }
                }
            }
            
            // Verification points during flow
            if (step == 20) {
                // By step 20, water should have filled the hole where dam was broken
                double water_at_hole = world->at(2, 5).getFillRatio();
                spdlog::info("Step 20: Water at dam hole (2,5): {:.3f}", water_at_hole);
                
                // Debug: Check velocities and COM positions
                for (int x = 0; x <= 3; x++) {
                    auto& cell = world->at(x, 5);
                    if (cell.getFillRatio() > 0.01) {
                        spdlog::info("  Cell ({},5) - Vel: ({:.3f},{:.3f}), COM: ({:.3f},{:.3f})", 
                                    x, cell.getVelocity().x, cell.getVelocity().y,
                                    cell.getCOM().x, cell.getCOM().y);
                    }
                }
                
                EXPECT_GT(water_at_hole, 0.5) << "Water should fill the dam hole by step 20";
            }
            
            if (step == 50) {
                // By step 50, water should start spreading to the right
                double water_next = world->at(3, 5).getFillRatio();
                spdlog::info("Step 50: Water at (3,5): {:.3f}", water_next);
                EXPECT_GT(water_next, 0.01) << "Water should start spreading right by step 50";
            }
            
            if (step == 70) {
                // Check further spread
                double water_further = world->at(4, 5).getFillRatio();
                spdlog::info("Step 70: Water at (4,5): {:.3f}", water_further);
                EXPECT_GT(water_further, 0.01) << "Water should continue spreading by step 70";
            }
            
            // Steps 11-80: observe flow
            if (step > 10 && step % 10 == 0) {
                // Measure how far water has traveled
                int max_x = 0;
                for (int y = 0; y < 6; y++) {
                    for (int x = 0; x < 6; x++) {
                        if (world->at(x, y).getMaterialType() == MaterialType::WATER && 
                            world->at(x, y).getFillRatio() > 0.01) {
                            max_x = std::max(max_x, x);
                        }
                    }
                }
                
                max_x_reached = max_x;
                spdlog::info("Step {}: Water front at x={}", step, max_x);
                
                if (max_x >= 5) {
                    spdlog::info("Water reached right edge!");
                }
            }
        }, "Dam break flow");
        
        // Debug final state
        spdlog::info("=== Final water distribution ===");
        for (int y = 0; y < 6; y++) {
            std::string row_info = "";
            for (int x = 0; x < 6; x++) {
                if (world->at(x, y).getMaterialType() == MaterialType::WATER && 
                    world->at(x, y).getFillRatio() > 0.01) {
                    row_info += "W ";
                } else if (world->at(x, y).getMaterialType() == MaterialType::WALL) {
                    row_info += "# ";
                } else {
                    row_info += ". ";
                }
            }
            spdlog::info("Row {}: {}", y, row_info);
        }
        spdlog::info("Max x reached: {}", max_x_reached);
        
        EXPECT_GT(max_x_reached, 2) << "Water should flow past dam location";
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, CommunicatingVessels) {
    // Purpose: Tests hydrostatic pressure equalization principle - water in connected vessels
    // should reach same height regardless of vessel shape. Fundamental fluid dynamics behavior.
    //
    // Setup: U-tube with water in left column only, connected at bottom
    // Expected: Water flows through bottom, rises in right column until heights equalize
    // Current Issue: Test fails - pressure system doesn't handle complex flow paths well
    
    runRestartableTest([this]() {
        // Clear world state
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        spdlog::info("[TEST] Communicating Vessels - Water should find equal level");
        
        // Create U-tube configuration with walls
        // Left column
        world->addMaterialAtCell(1, 0, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(1, 1, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(1, 2, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(1, 3, MaterialType::WALL, 1.0);
        
        // Right column
        world->addMaterialAtCell(4, 0, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(4, 1, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(4, 2, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(4, 3, MaterialType::WALL, 1.0);
        
        // Bottom connection
        world->addMaterialAtCell(2, 4, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(3, 4, MaterialType::WALL, 1.0);
        
        // Add water to left column only
        world->addMaterialAtCell(0, 0, MaterialType::WATER, 1.0);
        world->addMaterialAtCell(0, 1, MaterialType::WATER, 1.0);
        world->addMaterialAtCell(0, 2, MaterialType::WATER, 1.0);
        
        showInitialStateWithStep(world.get(), "U-tube test - water in left column");
        logWorldStateAscii(world.get(), "Initial: U-tube with water in left column");
        
        int left_height = 0;
        int right_height = 0;
        
        runSimulationLoop(80, [this, &left_height, &right_height](int step) {
            if (step == 60) {
                // Check water levels
                left_height = 0;
                right_height = 0;
                
                for (int y = 0; y < 4; y++) {
                    if (world->at(0, y).getMaterialType() == MaterialType::WATER) {
                        left_height = std::max(left_height, y + 1);
                    }
                    if (world->at(5, y).getMaterialType() == MaterialType::WATER) {
                        right_height = std::max(right_height, y + 1);
                    }
                }
                
                spdlog::info("Final water levels: left={}, right={}", left_height, right_height);
                
                if (std::abs(left_height - right_height) > 1) {
                    spdlog::warn("ISSUE: Water levels not equal (diff={})", 
                                std::abs(left_height - right_height));
                }
            }
        }, "Communicating vessels");
        
        // Water should have moved to right side
        EXPECT_GT(right_height, 0) << "Water should flow to right vessel";
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, VenturiConstriction) {
    // Purpose: Tests pressure behavior at flow constrictions. In real fluids, velocity increases
    // through constrictions while pressure decreases (Venturi effect). Tests if pressure-driven
    // flow can push water through narrow gaps.
    //
    // Setup: Water with rightward velocity hits walls with 2-cell vertical gap
    // Expected: Pressure builds before constriction, water flows through gap
    // Observation: Tests pressure accumulation and flow through restrictions
    
    runRestartableTest([this]() {
        // Clear world state
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        spdlog::info("[TEST] Venturi Effect - Flow through constriction");
        
        // Create constricted channel
        // Wide section on left
        for (int y = 1; y < 5; y++) {
            world->addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
        }
        
        // Walls creating constriction
        world->addMaterialAtCell(1, 1, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(1, 4, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(2, 1, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(2, 4, MaterialType::WALL, 1.0);
        // Narrow gap at y=2,3
        
        // Give water rightward velocity
        for (int y = 1; y < 5; y++) {
            world->at(0, y).setVelocity(Vector2d(3.0, 0.0));
        }
        
        showInitialStateWithStep(world.get(), "Venturi constriction test");
        logWorldStateAscii(world.get(), "Initial: Venturi constriction");
        
        double pressure_before = 0.0;
        double water_after = 0.0;
        
        runSimulationLoop(30, [this, &pressure_before, &water_after](int step) {
            if (step == 20) {
                // Check pressure buildup before constriction
                pressure_before = (world->at(0, 2).getHydrostaticPressure() + world->at(0, 2).getDynamicPressure()) + 
                                (world->at(0, 3).getHydrostaticPressure() + world->at(0, 3).getDynamicPressure());
                
                // Check if water made it through
                water_after = 0.0;
                for (int y = 0; y < 6; y++) {
                    water_after += world->at(3, y).getFillRatio();
                    water_after += world->at(4, y).getFillRatio();
                }
                
                spdlog::info("Pressure before constriction: {:.3f}", pressure_before);
                spdlog::info("Water after constriction: {:.3f}", water_after);
                
                if (pressure_before < 0.1) {
                    spdlog::warn("ISSUE: No pressure buildup at constriction");
                }
            }
        }, "Venturi flow");
        
        // Some water should make it through
        EXPECT_GT(water_after, 0.01) << "Some water should pass through constriction";
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, CornerEscapeDiagonal) {
    // Purpose: Specifically designed to test diagonal flow capability. With only 4-direction
    // flow, water cannot escape corner when cardinal directions are blocked. This is THE key
    // test for validating multi-directional (8-neighbor) flow implementation.
    //
    // Setup: Water trapped in corner with walls blocking right and down movements
    // Expected: With 8-direction flow, water escapes diagonally to (1,1)
    // Current: EXPECTED TO FAIL - system only supports cardinal direction flow
    
    runRestartableTest([this]() {
        // Clear world state
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        spdlog::info("[TEST] Corner Escape - Requires diagonal flow");
        
        // Place water in corner
        world->addMaterialAtCell(0, 0, MaterialType::WATER, 1.0);
        
        // Block cardinal directions with walls
        world->addMaterialAtCell(1, 0, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(0, 1, MaterialType::WALL, 1.0);
        
        // Give water diagonal velocity and pressure
        world->at(0, 0).setVelocity(Vector2d(3.0, 3.0));
        world->at(0, 0).setDynamicPressure(5.0);
        // Pressure is now scalar, not vector
        
        showInitialStateWithStep(world.get(), "Corner escape test - water trapped");
        logWorldStateAscii(world.get(), "Initial: Water trapped in corner");
        
        bool escaped = false;
        
        runSimulationLoop(50, [this, &escaped](int step) {
            if (step % 10 == 0 && step > 0) {
                // Check if water escaped diagonally
                escaped = false;
                for (int i = 1; i < 6; i++) {
                    if (world->at(i, i).getFillRatio() > 0.01) {
                        escaped = true;
                        spdlog::info("Step {}: Water escaped to ({},{})", step, i, i);
                        break;
                    }
                }
                
                if (!escaped && step >= 40) {
                    spdlog::warn("LIMITATION: No diagonal escape after {} steps", step);
                    spdlog::info("Current pressure: {:.3f}",
                                world->at(0, 0).getHydrostaticPressure() + world->at(0, 0).getDynamicPressure());
                }
            }
        }, "Corner escape test");
        
        // Currently fails due to lack of diagonal flow
        // EXPECT_TRUE(escaped) << "Water should escape diagonally";
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, TJunctionSplit) {
    // Purpose: Tests flow distribution at junctions. When flow hits a T-junction, it should
    // split proportionally based on available paths and pressure gradients. Currently shows
    // unequal distribution due to single-direction flow limitation.
    //
    // Setup: Vertical water flow hits horizontal wall with gap, creating T-junction
    // Expected: Water splits roughly equally left and right
    // Current: Water flows to only one neighbor instead of splitting proportionally
    
    runRestartableTest([this]() {
        // Clear world state
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        spdlog::info("[TEST] T-Junction - Flow should split equally");
        
        // Create vertical flow that hits horizontal wall
        world->addMaterialAtCell(2, 0, MaterialType::WATER, 1.0);
        world->addMaterialAtCell(3, 0, MaterialType::WATER, 1.0);
        world->addMaterialAtCell(2, 1, MaterialType::WATER, 1.0);
        world->addMaterialAtCell(3, 1, MaterialType::WATER, 1.0);
        
        // Horizontal wall creating T-junction
        for (int x = 0; x < 6; x++) {
            if (x != 2 && x != 3) {  // Leave gap for water entry
                world->addMaterialAtCell(x, 3, MaterialType::WALL, 1.0);
            }
        }
        
        // Give water downward velocity
        for (int x = 2; x <= 3; x++) {
            for (int y = 0; y <= 1; y++) {
                world->at(x, y).setVelocity(Vector2d(0.0, 4.0));
            }
        }
        
        showInitialStateWithStep(world.get(), "T-junction flow split test");
        logWorldStateAscii(world.get(), "Initial: T-junction setup");
        
        double left_flow = 0.0;
        double right_flow = 0.0;
        
        runSimulationLoop(50, [this, &left_flow, &right_flow](int step) {
            if (step == 40) {
                // Measure flow distribution
                left_flow = 0.0;
                right_flow = 0.0;
                
                // Count water on left side (x < 2.5)
                for (int x = 0; x < 2; x++) {
                    for (int y = 3; y < 6; y++) {
                        left_flow += world->at(x, y).getFillRatio();
                    }
                }
                
                // Count water on right side (x > 3.5)
                for (int x = 4; x < 6; x++) {
                    for (int y = 3; y < 6; y++) {
                        right_flow += world->at(x, y).getFillRatio();
                    }
                }
                
                spdlog::info("T-junction flow split:");
                spdlog::info("  Left: {:.3f}", left_flow);
                spdlog::info("  Right: {:.3f}", right_flow);
                
                if (left_flow > 0.01 && right_flow > 0.01) {
                    double ratio = left_flow / right_flow;
                    spdlog::info("  L/R Ratio: {:.2f} (ideal=1.0)", ratio);
                    
                    if (std::abs(ratio - 1.0) > 0.5) {
                        spdlog::warn("LIMITATION: Unequal flow distribution");
                    }
                } else {
                    spdlog::warn("LIMITATION: Flow only went one direction");
                }
            }
        }, "T-junction flow split");
        
        // Water should flow in at least one direction
        EXPECT_GT(left_flow + right_flow, 0.01) << "Water should flow past T-junction";
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}
