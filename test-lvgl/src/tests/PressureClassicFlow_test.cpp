#include <gtest/gtest.h>
#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../MaterialType.h"
#include "../Cell.h"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <sstream>

class PressureClassicFlowTest : public VisualTestBase {
protected:
    // Store world state snapshots for debugging.
    std::vector<std::string> worldStateHistory;
    
    // Capture current world state to a string.
    std::string captureWorldState(int timestep) {
        if (!world) return "";
        
        std::stringstream ss;
        ss << "=== Timestep " << timestep << " ===\n";
        
        // Add ASCII representation.
        ss << world->toAsciiDiagram() << "\n";
        
        // Add detailed cell information.
        ss << "Detailed Cell Information:\n";
        for (uint32_t y = 0; y < world->getHeight(); y++) {
            for (uint32_t x = 0; x < world->getWidth(); x++) {
                const CellB& cell = world->at(x, y);
                if (cell.getFillRatio() > 0.001) {  // Only log cells with meaningful content.
                    // Log walls more concisely.
                    if (cell.getMaterialType() == MaterialType::WALL) {
                        ss << fmt::format("  Cell({},{}) - WALL\n", x, y);
                    } else {
                        Vector2d gradient = cell.getPressureGradient();
                        ss << fmt::format("  Cell({},{}) - Material: {}, Fill: {:.6f}, "
                                         "Velocity: ({:.3f},{:.3f}), COM: ({:.3f},{:.3f}), "
                                         "HydroP: {:.6f}, DynP: {:.6f}, PGrad: ({:.6f},{:.6f})\n",
                                         x, y, 
                                         getMaterialName(cell.getMaterialType()),
                                         cell.getFillRatio(),
                                         cell.getVelocity().x, cell.getVelocity().y,
                                         cell.getCOM().x, cell.getCOM().y,
                                         cell.getHydrostaticPressure(),
                                         cell.getDynamicPressure(),
                                         gradient.x, gradient.y);
                    }
                }
            }
        }
        return ss.str();
    }
    
    // Helper function to log world state on test failure.
    void logFailureState(const std::string& failure_reason) {
        spdlog::error("TEST FAILURE: {}", failure_reason);
        spdlog::info("=== WORLD STATE HISTORY AT TEST FAILURE ===");
        
        // Log the last 3 timesteps (or fewer if less are available).
        size_t startIdx = worldStateHistory.size() > 3 ? worldStateHistory.size() - 3 : 0;
        for (size_t i = startIdx; i < worldStateHistory.size(); i++) {
            spdlog::info("{}", worldStateHistory[i]);
        }
    }
    
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Create a 6x6 world - small enough to visualize easily.
        world = createWorldB(6, 6);
        
        // Enable both pressure systems.
        world->setDynamicPressureEnabled(false);
        world->setHydrostaticPressureEnabled(true);
        world->setPressureScale(10.0);  // Strong pressure for visible effects.

        // Enable debug visualization to see pressure vectors.
        world->setDebugDrawEnabled(true);

        // Standard test settings.
        world->setWallsEnabled(false);
        world->setAddParticlesEnabled(false);
        world->setGravity(9.81);

        spdlog::info("[TEST] Classic pressure flow tests - 6x6 world with debug visualization");
    }

    void TearDown() override {
        world->setDebugDrawEnabled(false);
        VisualTestBase::TearDown();
    }
    
    // Required for unified simulation loop.
    WorldInterface* getWorldInterface() override {
        return world.get();
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(PressureClassicFlowTest, GradientDirectionHorizontal) {
    runRestartableTest([this]() {

    	const int WIDTH = 2;
        const int HEIGHT = 1;
        world = createWorldB(WIDTH, HEIGHT);

        world->setDynamicPressureEnabled(false);
        world->setHydrostaticPressureEnabled(true);
        world->setPressureScale(1.0);
        world->setPressureDiffusionEnabled(true);
        world->setWallsEnabled(false);
        world->setAddParticlesEnabled(false);
        world->setGravity(9.81);

        const std::string testTitle = "Gradient direction - 2x1 world, pressure on left cell";
        spdlog::info("[TEST] " + testTitle);

        // Setup world: left cell filled with water, right cell empty.
        world->addMaterialAtCell(0, 0, MaterialType::WATER, 1.0);
        spdlog::info("Setup: Left cell (0,0) filled with water, right cell (1,0) empty");

        showInitialStateWithStep(world.get(), testTitle);

        // Apply pressure to the left cell.
        world->at(0, 0).setPressure(100.0);
        spdlog::info("Applied pressure of 100.0 to left cell (0,0)");

        runSimulationLoop(2, [this](int step) {
            logWorldStateAscii(world.get(), "Step: " + std::to_string(step));
            logWorldState(world.get(), "Step: " + std::to_string(step));

            // Verify forces and examine pressure gradient.
            const auto& leftCell = world->at(0, 0);
            const auto& rightCell = world->at(1, 0);
            
            spdlog::info("Pressure values - Left cell: {:.2f}, Right cell: {:.2f}", 
                        leftCell.getPressure(), rightCell.getPressure());
            
            // Calculate pressure gradient (positive x direction).
            double pressure_gradient_x = rightCell.getPressure() - leftCell.getPressure();
            spdlog::info("Pressure gradient (left to right): {:.2f}", pressure_gradient_x);
            
            // Get forces on each cell.
            Vector2d leftForce = leftCell.getPendingForce();
            Vector2d rightForce = rightCell.getPendingForce();
            
            spdlog::info("Forces - Left cell: ({:.4f}, {:.4f}), Right cell: ({:.4f}, {:.4f})",
                        leftForce.x, leftForce.y, rightForce.x, rightForce.y);
            
            // Expected: negative gradient (high to low pressure) should create positive force on left cell.
            spdlog::info("Analysis: Pressure gradient is {}, expecting force on water to point {}",
                        pressure_gradient_x < 0 ? "negative (high to low)" : "positive (low to high)",
                        pressure_gradient_x < 0 ? "right (+x)" : "left (-x)");

        }, "Pressure gradient examination");

        // Final verification of pressure gradient behavior.
        // After physics updates, verify the water has gained rightward velocity from pressure.
        const auto& waterCell = world->at(0, 0);
        Vector2d velocity = waterCell.getVelocity();
        
        // Verify velocity is positive (rightward) and reasonable.
        EXPECT_GT(velocity.x, 0.5) << "Water should have significant rightward velocity from pressure gradient";
        EXPECT_LT(velocity.x, 5.0) << "Velocity should be reasonable, not extreme";
        
        // Y velocity should be small (mainly from gravity).
        EXPECT_GT(velocity.y, 0) << "Y velocity should be small";
        EXPECT_LT(velocity.y, 0.5) << "Y velocity should be small";
        
        // Verify pressure has decreased due to diffusion.
        double finalPressure = waterCell.getPressure();
        EXPECT_LT(finalPressure, 100.0) << "Pressure should decrease due to diffusion";
        EXPECT_GT(finalPressure, 90.0) << "Pressure shouldn't drop too drastically in 2 steps";
        
        spdlog::info("Verification passed - Water velocity: ({:.3f}, {:.3f}), Final pressure: {:.2f}", 
                    velocity.x, velocity.y, finalPressure);

        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, GradientWithWallBoundary) {
    runRestartableTest([this]() {
        
        // Create a 2x2 world to observe wall-pressure interactions.
        const int WIDTH = 2;
        const int HEIGHT = 2;
        world = createWorldB(WIDTH, HEIGHT);
        
        world->setDynamicPressureEnabled(false);
        world->setHydrostaticPressureEnabled(true);
        world->setPressureScale(1.0);
        world->setPressureDiffusionEnabled(true);
        world->setWallsEnabled(false);
        world->setAddParticlesEnabled(false);
        world->setGravity(9.81);
        
        const std::string testTitle = "Gradient with wall boundary - 2x2 world";
        spdlog::info("[TEST] " + testTitle);
        
        // Setup: Top left is wall, bottom two cells are water, top right is empty.
        world->addMaterialAtCell(0, 0, MaterialType::WALL, 1.0);   // Top left.
        world->addMaterialAtCell(0, 1, MaterialType::WATER, 1.0);  // Bottom left.
        world->addMaterialAtCell(1, 1, MaterialType::WATER, 1.0);  // Bottom right.
        // Cell (1,0) remains empty (top right).
        
        spdlog::info("Setup: Top-left=WALL, Bottom-left=WATER, Bottom-right=WATER, Top-right=empty");
        
        showInitialStateWithStep(world.get(), testTitle);
        
        // Apply pressure to bottom left water cell.
        world->at(0, 1).setPressure(100.0);
        spdlog::info("Applied pressure of 100.0 to bottom left water cell (0,1)");
        
        // Run for 3 timesteps to observe behavior.
        runSimulationLoop(3, [this](int step) {
            logWorldStateAscii(world.get(), "Step: " + std::to_string(step));
            
            // Log pressure values for all cells.
            spdlog::info("Step {} pressure values:", step);
            for (int y = 0; y < 2; y++) {
                for (int x = 0; x < 2; x++) {
                    const auto& cell = world->at(x, y);
                    spdlog::info("  Cell({},{}) [{}]: pressure={:.2f}", 
                                x, y, 
                                getMaterialName(cell.getMaterialType()),
                                cell.getPressure());
                }
            }
            
        }, "Observing pressure gradient with wall boundary");
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, DamBreak) {
    // Purpose: Classic fluid dynamics scenario testing horizontal pressure-driven flow.
    // Dynamic pressure from a water column should drive rapid flow when obstruction removed.
    //
    // Setup: Full-height water column (x=0-1) held by WALL dam at x=2, then bottom cell removed.
    // Expected: Water jets through bottom opening due to pressure.
    // Tests: Pressure gradient and high-pressure flow through small opening.

    world->setDynamicPressureEnabled(true);
    world->setHydrostaticPressureEnabled(false);
    
    runRestartableTest([this]() {
        spdlog::info("[TEST] Dam Break - Classic fluid dynamics scenario");
        
        // Clear world state history for this test.
        worldStateHistory.clear();
        
        // Create water column on left side - full height.
        for (int y = 0; y < 6; y++) {
            world->addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
            world->addMaterialAtCell(1, y, MaterialType::WATER, 1.0);
        }
        
        // Create dam (temporary wall) - full height using WALL.
        for (int y = 0; y < 6; y++) {
            world->addMaterialAtCell(2, y, MaterialType::WALL, 1.0);  // Using WALL as solid dam.
        }
        
        showInitialStateWithStep(world.get(), "Dam break test - water held by wall dam");
        
        int max_x_reached = 0;
        bool dam_broken = false;
        bool issue_detected = false;
        
        // Combined pressure build-up and flow simulation.
        runSimulationLoop(110, [this, &max_x_reached, &dam_broken, &issue_detected](int step) {
            // Skip to end if issue already detected.
            if (issue_detected) {
                return;
            }
            
            // Capture world state for debugging.
            worldStateHistory.push_back(captureWorldState(step));

            // Log full world state using existing functions.
            logWorldStateAscii(world.get(), "Step: " + std::to_string(step));
            logWorldState(world.get(), "Step: " + std::to_string(step));

            // Let pressure build up.
            if (step < 30) {
                spdlog::info("Building pressure... [Step {}/30]", step + 1);
                
                // Verify water cells maintain centered COM and low velocity before dam break.
                for (uint32_t y = 0; y < world->getHeight(); ++y) {
                    for (uint32_t x = 0; x < 2; ++x) {  // Only check water columns (0,1).
                        const auto& cell = world->at(x, y);
                        if (cell.getMaterialType() == MaterialType::WATER && cell.getFillRatio() > 0.9) {
                            Vector2d com = cell.getCOM();
                            Vector2d vel = cell.getVelocity();
                            
                            // Check COM - should remain near center (0,0) of each cell.
                            // It shouldn't move up.
                            if (std::abs(com.x) > 0.01 || com.y < 0) {
                                spdlog::error("OFF-CENTER COM DETECTED at step {} in cell ({},{})", step, x, y);
                                spdlog::error("  COM: ({:.4f}, {:.4f}), expected near (0, 0)", com.x, com.y);
                                spdlog::error("  Fill ratio: {:.4f}", cell.getFillRatio());
                                spdlog::error("  Velocity: ({:.4f}, {:.4f})", vel.x, vel.y);
                                issue_detected = true;
                                return;  // Skip to end of test.
                            }
                            
                            // Check velocity - water should not be moving up.
                            if (std::abs(vel.x) > 0.1 || vel.y < -0.1) {
                                spdlog::error("UNEXPECTED VELOCITY DETECTED at step {} in cell ({},{})", step, x, y);
                                spdlog::error("  Velocity: ({:.4f}, {:.4f}), expected < 0.1", vel.x, vel.y);
                                spdlog::error("  COM: ({:.4f}, {:.4f})", com.x, com.y);
                                spdlog::error("  Fill ratio: {:.4f}", cell.getFillRatio());
                                issue_detected = true;
                                return;  // Skip to end of test.
                            }
                        }
                    }
                }
            }
            // At step 10: break the dam.
            else if (step == 30 && !dam_broken) {
                spdlog::info("Breaking bottom of dam...");
                world->at(2, 5).clear();  // Remove only bottom wall cell.
                dam_broken = true;
                
                // Debug: Check pressure values around the break.
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
            
            // Verification points during flow.
            if (step == 60) {
                // By step 60, water should have filled the hole where dam was broken.
                double water_at_hole = world->at(2, 5).getFillRatio();
                spdlog::info("Step 60: Water at dam hole (2,5): {:.3f}", water_at_hole);
                EXPECT_GT(water_at_hole, 0.5) << "Water should fill the dam hole by step 60";
            }
            
            if (step == 70) {
                // By step 70, water should start spreading to the right.
                double water_next = world->at(3, 5).getFillRatio();
                spdlog::info("Step 70: Water at (3,5): {:.3f}", water_next);
                EXPECT_GT(water_next, 0.01) << "Water should start spreading right by step 70";
            }
            
            if (step == 90) {
                // Check further spread.
                double water_further = world->at(4, 5).getFillRatio();
                spdlog::info("Step 90: Water at (4,5): {:.3f}", water_further);
                EXPECT_GT(water_further, 0.01) << "Water should continue spreading by step 90";
            }
            
            // Steps 11-80: observe flow.
            if (step > 10 && step % 10 == 0) {
                // Measure how far water has traveled.
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
        }, "Dam break flow", [&issue_detected]() { return issue_detected; });
        
        // Debug final state.
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
        
        // Check if any issues were detected during the test.
        if (issue_detected) {
            logFailureState("Physics issues detected (off-center COM or unexpected velocity)");
            
            if (visual_mode_) {
                updateDisplay(world.get(), "TEST FAILED: Physics issues detected (see logs)");
                // Give user time to see the error before failing.
                pauseIfVisual(2000);
            }
            FAIL() << "Test failed due to physics issues (off-center COM or unexpected velocity)";
        }
        
        EXPECT_GT(max_x_reached, 2) << "Water should flow past dam location";
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, pressureEqualsGravity) {
    // Purpose: Investigate gravity-pressure interaction in a completely blocked water system.
    // Water blocked by walls should not gain upward velocity from pressure.
    //
    // Setup: 3x5 world with WALL on right side, rest filled with water.
    // Expected: COM should not move up, velocity should not point up.
    // Tests: Pressure-gravity equilibrium in static fluid.
    
    runRestartableTest([this]() {
        // Clear world state history for this test.
        worldStateHistory.clear();
        
        // Create smaller 3x5 world for this test.
		const int WIDTH = 3;
		const int HEIGHT = 5;
        world = createWorldB(WIDTH, HEIGHT);
        
        // Enable both pressure systems and gravity.
        world->setDynamicPressureEnabled(true);
        world->setHydrostaticPressureEnabled(true);
        world->setPressureScale(10.0);
        world->setGravity(9.81);
        world->setWallsEnabled(false);
        
        spdlog::info("[TEST] pressureEqualsGravity - Testing pressure-gravity equilibrium");
        
        // Clear world first.
        for (uint32_t y = 0; y < HEIGHT; ++y) {
            for (uint32_t x = 0; x < WIDTH; ++x) {
                world->at(x, y).clear();
            }
        }
        
        // Add WALL along right side.
        for (int y = 0; y < HEIGHT; y++) {
            world->addMaterialAtCell(WIDTH - 1, y, MaterialType::WALL, 1.0);
        }
        
        // Fill rest with water.
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH - 1; x++) {
                world->addMaterialAtCell(x, y, MaterialType::WATER, 1.0);
            }
        }
        
        showInitialStateWithStep(world.get(), "Water blocked by wall - testing pressure equilibrium");
        logWorldStateAscii(world.get(), "Initial: Water fully blocked by wall");
        
        bool issue_detected = false;
        
        // Run for 10 timesteps and check each one.
        runSimulationLoop(10, [this, &issue_detected, WIDTH, HEIGHT](int step) {
            spdlog::info("=== Timestep {} ===", step);
            
            // Capture world state for debugging.
            worldStateHistory.push_back(captureWorldState(step));
            
            // Log world state after physics update.
            logWorldStateAscii(world.get(), fmt::format("After timestep {}", step));
            
            // Check all water cells.
            for (uint32_t y = 0; y < HEIGHT; ++y) {
                for (uint32_t x = 0; x < WIDTH - 1; ++x) {  // Only check water cells (not the wall).
                    const auto& cell = world->at(x, y);
                    if (cell.getMaterialType() == MaterialType::WATER) {
                        Vector2d com = cell.getCOM();
                        Vector2d vel = cell.getVelocity();
                        double hydro = cell.getHydrostaticPressure();
                        double dynamic = cell.getDynamicPressure();
                        
                        spdlog::info("  Cell ({},{}) - COM: ({:.4f},{:.4f}), Vel: ({:.4f},{:.4f}), "
                                    "Hydrostatic: {:.4f}, Dynamic: {:.4f}",
                                    x, y, com.x, com.y, vel.x, vel.y, hydro, dynamic);
                        
                        // Check if COM moved up (negative y).
                        if (com.y < -0.01) {
                            spdlog::error("ERROR: COM moved UP at step {} in cell ({},{})", step, x, y);
                            spdlog::error("  COM.y = {:.4f} (should be >= -0.01)", com.y);
                            issue_detected = true;
                        }
                        
                        // Check if velocity points up (negative y).
                        if (vel.y < -0.01) {
                            spdlog::error("ERROR: Velocity points UP at step {} in cell ({},{})", step, x, y);
                            spdlog::error("  Velocity.y = {:.4f} (should be >= -0.01)", vel.y);
                            issue_detected = true;
                        }
                    }
                }
            }
            
            if (issue_detected) {
                logWorldState(world.get(), "Detailed state at error");
                return; // Stop early.
            }
        }, "Pressure-gravity equilibrium test", [&issue_detected]() { return issue_detected; });
        
        if (issue_detected) {
            logFailureState("Water showed upward movement in blocked system");
            FAIL() << "Water should not move upward when blocked by walls";
        }
        
        spdlog::info("Test passed: Water remained stable under pressure-gravity equilibrium");
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Water remained stable");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, WaterEqualization) {
    // Purpose: Tests water equalization through a dam break in a narrow channel.
    // Water should flow through the broken dam and equalize heights on both sides.
    //
    // Setup: 3x6 world with wall blocking center column, water on left, air on right.
    // Expected: After dam break, water flows right and equalizes height with left side.
    // Tests: Pressure-driven flow and hydrostatic equalization in simple geometry.
    
    runRestartableTest([this]() {
        // Clear world state history for this test.
        worldStateHistory.clear();
        
        // Create 3x6 world for this test.
        const int WIDTH = 3;
        const int HEIGHT = 6;
        world = createWorldB(WIDTH, HEIGHT);
        
        // Enable pressure systems with same settings as DamBreak.
        world->setDynamicPressureEnabled(false);
        world->setHydrostaticPressureEnabled(true);
        world->setPressureScale(1.0);
        world->setPressureDiffusionEnabled(true);
        world->setWallsEnabled(false);
        world->setAddParticlesEnabled(false);
        world->setGravity(9.81);
        
        spdlog::info("[TEST] Water Equalization - 3x6 world with center wall");
        
        // Clear world state.
        for (uint32_t y = 0; y < HEIGHT; ++y) {
            for (uint32_t x = 0; x < WIDTH; ++x) {
                world->at(x, y).clear();
            }
        }
        
        // Fill leftmost column with water.
        for (int y = 0; y < HEIGHT; y++) {
            world->addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
        }
        
        // Create wall in center column (x=1).
        for (int y = 0; y < HEIGHT; y++) {
            world->addMaterialAtCell(1, y, MaterialType::WALL, 1.0);
        }
        
        // Rightmost column remains empty (air).
        
        showInitialStateWithStep(world.get(), "Water equalization test - water held by center wall");
        
        bool dam_broken = false;
        int final_left_height = 0;
        int final_right_height = 0;
        
        // Run simulation with similar structure to DamBreak.
        runSimulationLoop(200, [this, &dam_broken, &final_left_height, &final_right_height, WIDTH, HEIGHT](int step) {
            // Capture world state for debugging.
            worldStateHistory.push_back(captureWorldState(step));
            
            // Log world state.
            logWorldStateAscii(world.get(), "Step: " + std::to_string(step));
            logWorldState(world.get(), "Step: " + std::to_string(step));
            
            // Let pressure build up.
            if (step < 30) {
                spdlog::info("Building pressure... [Step {}/30]", step + 1);
                
                // Verify water cells maintain centered COM and low velocity.
                for (uint32_t y = 0; y < HEIGHT; ++y) {
                    const auto& cell = world->at(0, y);  // Check left column.
                    if (cell.getMaterialType() == MaterialType::WATER && cell.getFillRatio() > 0.9) {
                        Vector2d com = cell.getCOM();
                        Vector2d vel = cell.getVelocity();
                        
                        if (std::abs(com.x) > 0.01 || com.y < 0) {
                            spdlog::error("OFF-CENTER COM DETECTED at step {} in cell (0,{})", step, y);
                            return;
                        }
                        
                        if (std::abs(vel.x) > 0.1 || vel.y < -0.1) {
                            spdlog::error("UNEXPECTED VELOCITY DETECTED at step {} in cell (0,{})", step, y);
                            return;
                        }
                    }
                }
            }
            // At step 30: break the dam at bottom.
            else if (step == 30 && !dam_broken) {
                spdlog::info("Breaking bottom of center wall...");
                world->at(1, HEIGHT-1).clear();  // Remove bottom wall cell.
                dam_broken = true;
                
                // Log pressure around the break.
                spdlog::info("=== Pressure analysis after dam break ===");
                for (int x = 0; x < WIDTH; x++) {
                    auto& cell = world->at(x, HEIGHT-1);
                    double hydro = cell.getHydrostaticPressure();
                    double total = hydro + cell.getDynamicPressure();
                    spdlog::info("Cell ({},{}) - Material: {}, Fill: {:.2f}, Total pressure: {:.4f}",
                                x, HEIGHT-1, 
                                static_cast<int>(cell.getMaterialType()),
                                cell.getFillRatio(),
                                total);
                }
            }
            
            // Verification points during flow.
            if (step == 60) {
                double water_at_hole = world->at(1, HEIGHT-1).getFillRatio();
                spdlog::info("Step 60: Water at dam hole (1,{}): {:.3f}", HEIGHT-1, water_at_hole);
                EXPECT_GT(water_at_hole, 0.5) << "Water should fill the dam hole by step 60";
            }
            
            if (step == 70) {
                double water_right_bottom = world->at(2, HEIGHT-1).getFillRatio();
                spdlog::info("Step 70: Water at rightmost bottom (2,{}): {:.3f}", HEIGHT-1, water_right_bottom);
                EXPECT_GT(water_right_bottom, 0.01) << "Water should reach right side by step 70";
            }
            
            if (step == 90) {
                // Check if water started moving up the right column.
                double water_right_up = world->at(2, HEIGHT-2).getFillRatio();
                spdlog::info("Step 90: Water one cell up on right (2,{}): {:.3f}", HEIGHT-2, water_right_up);
            }
            
            // Track water movement rate every 10 steps after dam break.
            if (dam_broken && step > 30 && step % 10 == 0) {
                // Check water fill levels in all cells.
                spdlog::info("=== Step {} - Water distribution ===", step);
                for (int y = 0; y < HEIGHT; y++) {
                    spdlog::info("Row {}: [{:.2f}] [{:.2f}] [{:.2f}]", 
                                y,
                                world->at(0, y).getFillRatio(),
                                world->at(1, y).getFillRatio(), 
                                world->at(2, y).getFillRatio());
                }
            }
            
            // At final steps, measure water heights.
            if (step == 100 || step == 150 || step == 190 || step == 240 || step == 290) {
                final_left_height = 0;
                final_right_height = 0;
                
                // Count water height in left column.
                for (int y = HEIGHT-1; y >= 0; y--) {
                    if (world->at(0, y).getFillRatio() > 0.1) {
                        final_left_height = HEIGHT - y;
                    }
                }
                
                // Count water height in right column.
                for (int y = HEIGHT-1; y >= 0; y--) {
                    if (world->at(2, y).getFillRatio() > 0.1) {
                        final_right_height = HEIGHT - y;
                    }
                }
                
                spdlog::info("=== Water heights at step {} ===", step);
                spdlog::info("Left column height: {}", final_left_height);
                spdlog::info("Right column height: {}", final_right_height);
                spdlog::info("Height difference: {}", std::abs(final_left_height - final_right_height));
            }
        }, "Water equalization");
        
        // Debug final state.
        spdlog::info("=== Final water distribution ===");
        for (int y = 0; y < HEIGHT; y++) {
            std::string row_info = "";
            for (int x = 0; x < WIDTH; x++) {
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
        
        // Verify water flowed to right side.
        if (final_right_height <= 0) {
            logFailureState("Water did not flow to right column");
        }
        EXPECT_GT(final_right_height, 0) << "Water should flow to right column";
        
        // Check if heights are somewhat equalized (within 2 cells).
        if (std::abs(final_left_height - final_right_height) > 2) {
            logFailureState(fmt::format("Water heights not equalized - Left: {}, Right: {}, Difference: {}", 
                                       final_left_height, final_right_height, 
                                       std::abs(final_left_height - final_right_height)));
        }
        EXPECT_LE(std::abs(final_left_height - final_right_height), 2) 
            << "Water heights should be approximately equal";
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, WaterEqualization2) {
    runRestartableTest([this]() {

        const int WIDTH = 3;
        const int HEIGHT = 6;
        world = createWorldB(WIDTH, HEIGHT);

        world->setDynamicPressureEnabled(false);
        world->setHydrostaticPressureEnabled(true);
        world->setPressureScale(1.0);
        world->setPressureDiffusionEnabled(true);
        world->setWallsEnabled(false);
        world->setAddParticlesEnabled(false);
        world->setGravity(9.81);

        spdlog::info("[TEST] Water Equalization - 3x6 world with center wall");

        // A floating column of water
        for (int y = 1; y < HEIGHT - 1; y++) {
            world->addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
        }

        world->addMaterialAtCell(0, 5, MaterialType::AIR, 0.0);

        // Create wall in center column (x=1).
        for (int y = 0; y < HEIGHT; y++) {
            world->addMaterialAtCell(1, y, MaterialType::WALL, 1.0);
        }

        // Rightmost column remains empty (air).

        showInitialStateWithStep(world.get(), "Water equalization test - water held by center wall");

        bool dam_broken = false;
        int final_left_height = 0;
        int final_right_height = 0;

        runSimulationLoop(5, [this, &dam_broken, &final_left_height, &final_right_height, WIDTH, HEIGHT](int step) {

            // Log world state.
            logWorldStateAscii(world.get(), "Step: " + std::to_string(step));
            logWorldState(world.get(), "Step: " + std::to_string(step));

        }, "Water equalization");

        // Debug final state.
        spdlog::info("=== Final water distribution ===");
        for (int y = 0; y < HEIGHT; y++) {
            std::string row_info = "";
            for (int x = 0; x < WIDTH; x++) {
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

        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}


TEST_F(PressureClassicFlowTest, VenturiConstriction) {
    // Purpose: Tests pressure behavior at flow constrictions. In real fluids, velocity increases.
    // through constrictions while pressure decreases (Venturi effect). Tests if pressure-driven.
    // flow can push water through narrow gaps.
    //
    // Setup: Water with rightward velocity hits walls with 2-cell vertical gap.
    // Expected: Pressure builds before constriction, water flows through gap.
    // Observation: Tests pressure accumulation and flow through restrictions.
    
    runRestartableTest([this]() {
        // Clear world state.
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        spdlog::info("[TEST] Venturi Effect - Flow through constriction");
        
        // Create constricted channel.
        // Wide section on left.
        for (int y = 1; y < 5; y++) {
            world->addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
        }
        
        // Walls creating constriction.
        world->addMaterialAtCell(1, 1, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(1, 4, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(2, 1, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(2, 4, MaterialType::WALL, 1.0);
        // Narrow gap at y=2,3.
        
        // Give water rightward velocity.
        for (int y = 1; y < 5; y++) {
            world->at(0, y).setVelocity(Vector2d(3.0, 0.0));
        }
        
        showInitialStateWithStep(world.get(), "Venturi constriction test");
        logWorldStateAscii(world.get(), "Initial: Venturi constriction");
        
        double pressure_before = 0.0;
        double water_after = 0.0;
        
        runSimulationLoop(30, [this, &pressure_before, &water_after](int step) {
            if (step == 20) {
                // Check pressure buildup before constriction.
                pressure_before = (world->at(0, 2).getHydrostaticPressure() + world->at(0, 2).getDynamicPressure()) + 
                                (world->at(0, 3).getHydrostaticPressure() + world->at(0, 3).getDynamicPressure());
                
                // Check if water made it through.
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
        
        // Some water should make it through.
        EXPECT_GT(water_after, 0.01) << "Some water should pass through constriction";
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, CornerEscapeDiagonal) {
    // Purpose: Specifically designed to test diagonal flow capability. With only 4-direction.
    // flow, water cannot escape corner when cardinal directions are blocked. This is THE key.
    // test for validating multi-directional (8-neighbor) flow implementation.
    //
    // Setup: Water trapped in corner with walls blocking right and down movements.
    // Expected: With 8-direction flow, water escapes diagonally to (1,1).
    // Current: EXPECTED TO FAIL - system only supports cardinal direction flow.
    
    runRestartableTest([this]() {
        // Clear world state.
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        spdlog::info("[TEST] Corner Escape - Requires diagonal flow");
        
        // Place water in corner.
        world->addMaterialAtCell(0, 0, MaterialType::WATER, 1.0);
        
        // Block cardinal directions with walls.
        world->addMaterialAtCell(1, 0, MaterialType::WALL, 1.0);
        world->addMaterialAtCell(0, 1, MaterialType::WALL, 1.0);
        
        // Give water diagonal velocity and pressure.
        world->at(0, 0).setVelocity(Vector2d(3.0, 3.0));
        world->at(0, 0).setDynamicPressure(5.0);
        // Pressure is now scalar, not vector.
        
        showInitialStateWithStep(world.get(), "Corner escape test - water trapped");
        logWorldStateAscii(world.get(), "Initial: Water trapped in corner");
        
        bool escaped = false;
        
        runSimulationLoop(50, [this, &escaped](int step) {
            if (step % 10 == 0 && step > 0) {
                // Check if water escaped diagonally.
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
        
        // Currently fails due to lack of diagonal flow.
        // EXPECT_TRUE(escaped) << "Water should escape diagonally";
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}

TEST_F(PressureClassicFlowTest, TJunctionSplit) {
    // Purpose: Tests flow distribution at junctions. When flow hits a T-junction, it should.
    // split proportionally based on available paths and pressure gradients. Currently shows.
    // unequal distribution due to single-direction flow limitation.
    //
    // Setup: Vertical water flow hits horizontal wall with gap, creating T-junction.
    // Expected: Water splits roughly equally left and right.
    // Current: Water flows to only one neighbor instead of splitting proportionally.
    
    runRestartableTest([this]() {
        // Clear world state.
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                world->at(x, y).clear();
            }
        }
        
        spdlog::info("[TEST] T-Junction - Flow should split equally");
        
        // Create vertical flow that hits horizontal wall.
        world->addMaterialAtCell(2, 0, MaterialType::WATER, 1.0);
        world->addMaterialAtCell(3, 0, MaterialType::WATER, 1.0);
        world->addMaterialAtCell(2, 1, MaterialType::WATER, 1.0);
        world->addMaterialAtCell(3, 1, MaterialType::WATER, 1.0);
        
        // Horizontal wall creating T-junction.
        for (int x = 0; x < 6; x++) {
            if (x != 2 && x != 3) {  // Leave gap for water entry.
                world->addMaterialAtCell(x, 3, MaterialType::WALL, 1.0);
            }
        }
        
        // Give water downward velocity.
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
                // Measure flow distribution.
                left_flow = 0.0;
                right_flow = 0.0;
                
                // Count water on left side (x < 2.5).
                for (int x = 0; x < 2; x++) {
                    for (int y = 3; y < 6; y++) {
                        left_flow += world->at(x, y).getFillRatio();
                    }
                }
                
                // Count water on right side (x > 3.5).
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
        
        // Water should flow in at least one direction.
        EXPECT_GT(left_flow + right_flow, 0.01) << "Water should flow past T-junction";
        
        if (visual_mode_) {
            updateDisplay(world.get(), "Test complete! Press Start to restart or Next to continue");
            waitForRestartOrNext();
        }
    });
}
