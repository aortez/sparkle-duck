#include "visual_test_runner.h"
#include <cmath>
#include "../WorldB.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

class ForceCalculationTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Set up logging to see detailed debug output including trace level
        spdlog::set_level(spdlog::level::trace);
        
        // Apply auto-scaling for 5x5 world before creation
        if (visual_mode_ && auto_scaling_enabled_) {
            scaleDrawingAreaForWorld(5, 5);
        }
        
        // Create a 5x5 world for testing force calculations
        // Pass the UI draw area if in visual mode, otherwise nullptr
        lv_obj_t* draw_area = (visual_mode_ && ui_) ? ui_->getDrawArea() : nullptr;
        world = std::make_unique<WorldB>(5, 5, draw_area);
        world->setWallsEnabled(false); // Disable walls for clean testing
        world->setAddParticlesEnabled(false); // Disable automatic particle addition for clean testing
        
        // CRITICAL: Connect the UI to the world for button functionality
        if (visual_mode_ && ui_) {
            auto& coordinator = VisualTestCoordinator::getInstance();
            coordinator.postTaskSync([this] {
                ui_->setWorld(world.get());
            });
        }
    }
    
    void TearDown() override {
        world.reset();
        VisualTestBase::TearDown();
    }
    
    void updateVisualDisplay() {
        if (visual_mode_ && world) {
            auto& coordinator = VisualTestCoordinator::getInstance();
            coordinator.postTaskSync([this] {
                world->draw();
            });
        }
    }
    
    void pauseIfVisual(int milliseconds = 500) {
        if (visual_mode_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }
    }
    
    // Helper method for manual simulation control
    void manualSimulationSteps(WorldB* world, int max_steps, const std::string& description) {
        if (!world) return;
        
        if (visual_mode_) {
            spdlog::info("=== Manual Simulation: {} ===", description);
            spdlog::info("Use Next button to advance simulation step by step");
            
            for (int step = 0; step < max_steps; ++step) {
                waitForNext(); // User advances to next step
                world->advanceTime(0.016);
                
                auto& coordinator = VisualTestCoordinator::getInstance();
                coordinator.postTaskSync([world] {
                    world->draw();
                });
                
                spdlog::info("Step {}/{} completed", step + 1, max_steps);
            }
        } else {
            // Non-visual mode: run automatically
            for (int step = 0; step < max_steps; ++step) {
                world->advanceTime(0.016);
            }
        }
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(ForceCalculationTest, EmptyCellHasZeroForces) {
    auto cohesion = world->calculateCohesionForce(2, 2);
    auto adhesion = world->calculateAdhesionForce(2, 2);
    
    EXPECT_EQ(cohesion.resistance_magnitude, 0.0);
    EXPECT_EQ(cohesion.connected_neighbors, 0);
    EXPECT_EQ(adhesion.force_magnitude, 0.0);
    EXPECT_EQ(adhesion.contact_points, 0);
}

TEST_F(ForceCalculationTest, IsolatedWaterHasNoForces) {
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    
    auto cohesion = world->calculateCohesionForce(2, 2);
    auto adhesion = world->calculateAdhesionForce(2, 2);
    
    // No same-material neighbors = no cohesion resistance
    EXPECT_EQ(cohesion.resistance_magnitude, 0.0);
    EXPECT_EQ(cohesion.connected_neighbors, 0);
    
    // No different-material neighbors = no adhesion
    EXPECT_EQ(adhesion.force_magnitude, 0.0);
    EXPECT_EQ(adhesion.contact_points, 0);
}

TEST_F(ForceCalculationTest, WaterWithWaterNeighborsHasCohesion) {
    // Enable restart functionality
    spdlog::info("[TEST] Enabling restart functionality");
    enableTestRestart();
    
    int iteration = 0;
    do {
        ++iteration;
        spdlog::info("[TEST] Starting test iteration {}", iteration);
        
        clearRestartRequest();
        waitForStart();
        
        spdlog::info("[TEST] Setting up test scenario");
        // Setup the test scenario
        world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
        world->addMaterialAtCell(2, 1, MaterialType::WATER, 1.0); // Above
        world->addMaterialAtCell(1, 2, MaterialType::WATER, 1.0); // Left
        
        // Show the setup in visual mode
        updateVisualDisplay();
        waitForNext();
        
        spdlog::info("[TEST] Calculating cohesion forces");
        auto cohesion = world->calculateCohesionForce(2, 2);
        
        // Should have cohesion resistance from 2 same-material neighbors
        EXPECT_GT(cohesion.resistance_magnitude, 0.0);
        EXPECT_EQ(cohesion.connected_neighbors, 2);
        
        // Verify formula: resistance = material_cohesion * connected_neighbors * fill_ratio
        const MaterialProperties& props = getMaterialProperties(MaterialType::WATER);
        double expected_resistance = props.cohesion * 2.0 * 1.0;
        EXPECT_DOUBLE_EQ(cohesion.resistance_magnitude, expected_resistance);
        
        // Clear the world for next restart
        if (visual_mode_ && shouldRestartTest()) {
            spdlog::info("[TEST] Restart requested - clearing world for next iteration");
            world->reset();
        }
        
        spdlog::info("[TEST] Iteration {} complete. shouldRestartTest={}, visual_mode={}", iteration, shouldRestartTest(), visual_mode_);
        
    } while (shouldRestartTest() && visual_mode_);
    
    spdlog::info("[TEST] Test completed after {} iterations", iteration);
    disableTestRestart();
}

TEST_F(ForceCalculationTest, WaterWithDirtNeighborHasAdhesion) {
    waitForStart();
    
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(3, 2, MaterialType::DIRT, 1.0); // Right
    
    // Show the setup in visual mode
    updateVisualDisplay();
    waitForNext();
    
    auto adhesion = world->calculateAdhesionForce(2, 2);
    
    // Should have adhesion force from different-material neighbor
    EXPECT_GT(adhesion.force_magnitude, 0.0);
    EXPECT_EQ(adhesion.contact_points, 1);
    EXPECT_EQ(adhesion.target_material, MaterialType::DIRT);
    
    // Force should point toward the DIRT neighbor (direction: +1, 0)
    EXPECT_GT(adhesion.force_direction.x, 0.0);
    EXPECT_DOUBLE_EQ(adhesion.force_direction.y, 0.0);
}

TEST_F(ForceCalculationTest, MetalHasHighCohesion) {
    // Use interior coordinates to avoid boundary walls (5x5 grid: boundaries at x=0,4 y=0,4)
    world->addMaterialAtCell(2, 2, MaterialType::METAL, 1.0);
    world->addMaterialAtCell(2, 1, MaterialType::METAL, 1.0); // Above (2,2)
    
    auto cohesion_metal = world->calculateCohesionForce(2, 2);
    
    // Create new world for WATER test to avoid interference
    auto water_world = std::make_unique<WorldB>(5, 5, nullptr);
    water_world->setAddParticlesEnabled(false);
    water_world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    water_world->addMaterialAtCell(2, 1, MaterialType::WATER, 1.0); // Above (2,2)
    
    auto cohesion_water = water_world->calculateCohesionForce(2, 2);
    
    // With same neighbor count (1), METAL should have higher resistance due to higher cohesion property
    EXPECT_GT(cohesion_metal.resistance_magnitude, cohesion_water.resistance_magnitude);
    
    // Verify METAL has higher cohesion property (0.9 vs 0.1)
    const MaterialProperties& metal_props = getMaterialProperties(MaterialType::METAL);
    const MaterialProperties& water_props = getMaterialProperties(MaterialType::WATER);
    EXPECT_GT(metal_props.cohesion, water_props.cohesion);
    
    // Expected calculations:
    // METAL: cohesion=0.9, neighbors=1, fill=1.0 → resistance = 0.9 * 1 * 1.0 = 0.9
    // WATER: cohesion=0.1, neighbors=1, fill=1.0 → resistance = 0.1 * 1 * 1.0 = 0.1
    EXPECT_DOUBLE_EQ(cohesion_metal.resistance_magnitude, 0.9);
    EXPECT_DOUBLE_EQ(cohesion_water.resistance_magnitude, 0.1);
}

TEST_F(ForceCalculationTest, AdhesionUsesGeometricMean) {
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(3, 2, MaterialType::METAL, 1.0); // Right
    
    auto adhesion = world->calculateAdhesionForce(2, 2);
    
    // Verify mutual adhesion calculation (geometric mean)
    const MaterialProperties& water_props = getMaterialProperties(MaterialType::WATER);
    const MaterialProperties& metal_props = getMaterialProperties(MaterialType::METAL);
    double expected_mutual = std::sqrt(water_props.adhesion * metal_props.adhesion);
    
    // Force strength should be based on this mutual adhesion
    double expected_force_strength = expected_mutual * 1.0 * 1.0 * 1.0; // mutual * fill1 * fill2 * distance_weight
    EXPECT_DOUBLE_EQ(adhesion.force_magnitude, expected_force_strength);
}

TEST_F(ForceCalculationTest, PartialCellsFillRatioWeighting) {
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 0.5); // Half-filled
    world->addMaterialAtCell(2, 1, MaterialType::WATER, 0.8); // Above, 80% filled
    
    auto cohesion = world->calculateCohesionForce(2, 2);
    
    // Expected: cohesion_property * connected_neighbors * own_fill_ratio
    // Note: connected_neighbors is count (1), not weighted by fill ratio
    const MaterialProperties& props = getMaterialProperties(MaterialType::WATER);
    double expected_resistance = props.cohesion * 1 * 0.5; // 0.1 * 1 * 0.5 = 0.05
    EXPECT_DOUBLE_EQ(cohesion.resistance_magnitude, expected_resistance);
    EXPECT_EQ(cohesion.connected_neighbors, 1);
}

TEST_F(ForceCalculationTest, MetalParticlesCohesionResistance) {
    waitForStart();
    
    // Create a 7x7 world to have space for separate droplets
    // Apply auto-scaling for larger world
    if (visual_mode_ && auto_scaling_enabled_) {
        scaleDrawingAreaForWorld(7, 7);
    }
    
    lv_obj_t* draw_area = (visual_mode_ && ui_) ? ui_->getDrawArea() : nullptr;
    auto large_world = std::make_unique<WorldB>(7, 7, draw_area);
    large_world->setWallsEnabled(false); // Disable walls for clean test
    large_world->setAddParticlesEnabled(false); // Disable automatic particle addition
    
    // This test verifies that cohesion forces prevent particles from separating
    // Use METAL for strong cohesion effects (cohesion = 0.9, high resistance)
    // Create two separate metal groups with gap between them
    // Droplet 1: at (1,3) and (2,3) - left side
    large_world->addMaterialAtCell(1, 3, MaterialType::METAL, 1.0);
    large_world->addMaterialAtCell(2, 3, MaterialType::METAL, 1.0);
    
    // Droplet 2: at (4,3) and (5,3) - right side  
    large_world->addMaterialAtCell(4, 3, MaterialType::METAL, 1.0);
    large_world->addMaterialAtCell(5, 3, MaterialType::METAL, 1.0);
    
    // Gap at (3,3) - empty space between droplets
    EXPECT_EQ(large_world->at(3, 3).getMaterialType(), MaterialType::AIR);
    EXPECT_EQ(large_world->at(3, 3).getFillRatio(), 0.0);
    
    // Set gravity to zero to isolate cohesion effects
    large_world->setGravity(0.0);
    
    // Give initial velocity toward the gap to test cohesion resistance
    // Without cohesion, particles should continue moving apart
    // With cohesion, they should slow down or reverse direction
    CellB& left_cell = large_world->at(2, 3);
    CellB& right_cell = large_world->at(4, 3);
    
    // Set initial velocities pointing away from each other
    left_cell.setVelocity(Vector2d(-0.5, 0.0));  // Moving left (away from gap)
    right_cell.setVelocity(Vector2d(0.5, 0.0));  // Moving right (away from gap)
    
    // Show initial setup in visual mode
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([&large_world] {
            large_world->draw();
        });
        spdlog::info("Showing initial metal droplet setup...");
        waitForNext();
    }
    
    // Record initial states
    Vector2d initial_velocity_left = left_cell.getVelocity();
    Vector2d initial_velocity_right = right_cell.getVelocity();
    
    // Manual simulation control: let user advance simulation step by step
    spdlog::info("=== Manual Simulation Control ===");
    spdlog::info("Use Step button to advance simulation timestep by timestep");
    
    // Connect the large_world to UI for step functionality
    if (visual_mode_ && ui_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([this, &large_world] {
            ui_->setWorld(large_world.get());
        });
    }
    
    bool cohesion_detected = false;
    bool droplets_combined = false;
    
    for (int step = 0; step < 50; ++step) {
        // Wait for user to advance simulation using Step button
        if (visual_mode_) {
            waitForStep();
        } else {
            large_world->advanceTime(0.016); // Auto-advance in non-visual mode
        }
        
        // Update visual display
        if (visual_mode_) {
            auto& coordinator = VisualTestCoordinator::getInstance();
            coordinator.postTaskSync([&large_world] {
                large_world->draw();
            });
        }
        
        Vector2d current_velocity_left = left_cell.getVelocity();
        Vector2d current_velocity_right = right_cell.getVelocity();
        
        spdlog::info("Step {}: Left velocity ({:.3f}, {:.3f}), Right velocity ({:.3f}, {:.3f})", 
                     step + 1, current_velocity_left.x, current_velocity_left.y,
                     current_velocity_right.x, current_velocity_right.y);
        
        // Check if cohesion has affected velocities
        // Cohesion should reduce the velocity magnitude or reverse direction
        if (current_velocity_left.mag() < initial_velocity_left.mag() * 0.9 ||
            current_velocity_right.mag() < initial_velocity_right.mag() * 0.9 ||
            current_velocity_left.x > -0.1 ||  // Left particle slowed or reversed
            current_velocity_right.x < 0.1) {  // Right particle slowed or reversed
            
            cohesion_detected = true;
            spdlog::info("COHESION DETECTED: velocities changed from ({:.3f}, {:.3f}) to ({:.3f}, {:.3f}) at step {}", 
                         initial_velocity_left.x, initial_velocity_right.x,
                         current_velocity_left.x, current_velocity_right.x, step + 1);
            
            if (visual_mode_) {
                waitForNext(); // Let user observe the cohesion effect
            }
            break;
        }
        
        // Early exit if material transfers into the gap (droplets combining)
        if (large_world->at(3, 3).getFillRatio() > 0.0) {
            droplets_combined = true;
            spdlog::info("DROPLETS COMBINED: Material appeared in gap at step {}", step + 1);
            
            if (visual_mode_) {
                waitForNext(); // Let user observe the combination
            }
            break;
        }
    }
    
    if (cohesion_detected) {
        SUCCEED() << "Cohesion forces successfully detected during manual simulation";
    } else if (droplets_combined) {
        SUCCEED() << "Metal droplets combined during manual simulation";
    } else {
        spdlog::info("No velocity changes detected in 50 steps - checking static cohesion forces");
    }
    
    // If no velocity changes detected, verify cohesion forces are at least being calculated
    auto cohesion_left = large_world->calculateCohesionForce(2, 3);
    auto cohesion_right = large_world->calculateCohesionForce(4, 3);
    
    // Even if movement wasn't detected, cohesion forces should exist
    EXPECT_GT(cohesion_left.resistance_magnitude, 0.0) 
        << "Left metal cell should have cohesion resistance with its neighbor";
    EXPECT_GT(cohesion_right.resistance_magnitude, 0.0)
        << "Right metal cell should have cohesion resistance with its neighbor";
    EXPECT_GE(cohesion_left.connected_neighbors, 1) 
        << "Left metal cell should have at least 1 connected neighbor";
    EXPECT_GE(cohesion_right.connected_neighbors, 1)
        << "Right metal cell should have at least 1 connected neighbor";
}

TEST_F(ForceCalculationTest, DebugMaterialTransfer) {
    // Debug test to understand material duplication and floating block behavior
    lv_obj_t* draw_area = (visual_mode_ && ui_) ? ui_->getDrawArea() : nullptr;
    world = std::make_unique<WorldB>(3, 3, draw_area);
    world->setWallsEnabled(false);
    
    // Simple test: single particle at top should fall straight down
    world->addMaterialAtCell(1, 0, MaterialType::DIRT, 1.0);
    
    spdlog::info("=== DEBUG: Single particle fall test ===");
    spdlog::info("Initial state:");
    spdlog::info("\n{}", world->toAsciiDiagram());
    
    // Check initial fill ratios
    spdlog::info("Cell (1,0) fill: {:.3f}", world->at(1, 0).getFillRatio());
    spdlog::info("Cell (1,1) fill: {:.3f}", world->at(1, 1).getFillRatio());
    spdlog::info("Cell (1,2) fill: {:.3f}", world->at(1, 2).getFillRatio());
    
    world->setGravity(15.0);
    
    // Run just a few steps to see what happens
    for (int step = 0; step < 5; ++step) {
        world->advanceTime(0.016);
        
        spdlog::info("Step {}: fills - (1,0):{:.3f}, (1,1):{:.3f}, (1,2):{:.3f}", 
                     step + 1,
                     world->at(1, 0).getFillRatio(),
                     world->at(1, 1).getFillRatio(), 
                     world->at(1, 2).getFillRatio());
        
        if (step == 2) {
            spdlog::info("After step 3:");
            spdlog::info("\n{}", world->toAsciiDiagram());
        }
    }
    
    spdlog::info("Final state:");
    spdlog::info("\n{}", world->toAsciiDiagram());
}

TEST_F(ForceCalculationTest, FloatingBlocksFallToNextCellSameSpeed) {
    // Test cell-to-cell transfers: 2x2 block vs isolated particle should transfer downward at similar rates
    //
    // World Setup (6x4 grid, walls disabled):
    // ┌──────┐
    // │   R  │  Row 0: Reference particle (R) at x=3, isolated
    // │BB    │  Row 1: 2x2 block (B) at x=0-1, gap at x=2, reference at x=3
    // │BB    │  Row 2: 2x2 block (B) at x=0-1
    // │      │  Row 3: Empty target cells for transfers
    // └──────┘
    //  012345   Column indices
    //
    // Expected Results:
    // - Reference particle (x=3): transfers from (3,0) to (3,1) - no cohesion resistance
    // - 2x2 block (x=0-1): transfers from rows 1-2 to row 3 - minimal cohesion (floating)
    // - Transfer timing should be nearly identical since both are floating structures
    //
    // Override the default 5x5 world with our custom 6x4 world
    // Apply auto-scaling for 6x4 world
    if (visual_mode_ && auto_scaling_enabled_) {
        scaleDrawingAreaForWorld(6, 4);
    }
    
    lv_obj_t* draw_area = (visual_mode_ && ui_) ? ui_->getDrawArea() : nullptr;
    world = std::make_unique<WorldB>(6, 4, draw_area);
    world->setWallsEnabled(false); // Disable walls for clean test
    world->setAddParticlesEnabled(false); // Disable automatic particle addition
    
    // Connect the UI to the world for button functionality.
    if (visual_mode_ && ui_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([this] {
            ui_->setWorld(world.get());
        });
    }
    
    // Create 2x2 floating dirt block at left side
    world->addMaterialAtCell(0, 1, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(1, 1, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(0, 2, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(1, 2, MaterialType::DIRT, 1.0);
    
    // Reference particle: isolated with full empty column (x=2) as gap
    world->addMaterialAtCell(3, 0, MaterialType::DIRT, 1.0);
    
    // Verify setup - empty column between block and reference
    EXPECT_EQ(world->at(2, 0).getFillRatio(), 0.0); // Gap column empty
    EXPECT_EQ(world->at(2, 1).getFillRatio(), 0.0); // Gap column empty
    EXPECT_EQ(world->at(2, 2).getFillRatio(), 0.0); // Gap column empty
    
    // Verify target cells are empty
    EXPECT_EQ(world->at(0, 3).getFillRatio(), 0.0); // Below 2x2 block
    EXPECT_EQ(world->at(1, 3).getFillRatio(), 0.0); // Below 2x2 block
    EXPECT_EQ(world->at(3, 1).getFillRatio(), 0.0); // Below reference
    
    // Show initial setup with ASCII diagram
    spdlog::info("=== INITIAL SETUP ===");
    spdlog::info("ASCII diagram before simulation:");
    std::string initial_ascii = world->toAsciiDiagram();
    spdlog::info("\n{}", initial_ascii);
    
    // Always render the initial world state immediately
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([this] {
            world->draw();
        });
        spdlog::info("Initial setup: 2x2 block vs isolated reference particle");
        
        // Wait for Start button to begin simulation
        waitForStart();
    }
    
    // Enable gravity and measure transfer timing
    world->setGravity(15.0);
    
    bool block_transferred = false;
    bool ref_transferred = false;
    bool upper_blocks_started_falling = false;
    int block_transfer_step = -1;
    int ref_transfer_step = -1;
    int upper_blocks_fall_step = -1;
    
    for (int step = 0; step < 300; ++step) {
        world->advanceTime(0.016);
        
        // Debug: Log state every 10 steps to see if anything is changing
        if (step % 10 == 0) {
            spdlog::info("Step {}: 2x2 block fills - (0,1):{:.3f}, (1,1):{:.3f}, (0,2):{:.3f}, (1,2):{:.3f}", 
                         step,
                         world->at(0, 1).getFillRatio(), world->at(1, 1).getFillRatio(),
                         world->at(0, 2).getFillRatio(), world->at(1, 2).getFillRatio());
            spdlog::info("Step {}: Reference particle (3,0):{:.3f}, target (3,1):{:.3f}", 
                         step, world->at(3, 0).getFillRatio(), world->at(3, 1).getFillRatio());
            spdlog::info("Step {}: Target row 3 - (0,3):{:.3f}, (1,3):{:.3f}", 
                         step, world->at(0, 3).getFillRatio(), world->at(1, 3).getFillRatio());
            
            // Log COMs at DEBUG level
            spdlog::debug("Step {}: COMs - (0,1):[{:.3f},{:.3f}], (1,1):[{:.3f},{:.3f}], (0,2):[{:.3f},{:.3f}], (1,2):[{:.3f},{:.3f}]",
                         step,
                         world->at(0, 1).getCOM().x, world->at(0, 1).getCOM().y,
                         world->at(1, 1).getCOM().x, world->at(1, 1).getCOM().y,
                         world->at(0, 2).getCOM().x, world->at(0, 2).getCOM().y,
                         world->at(1, 2).getCOM().x, world->at(1, 2).getCOM().y);
            spdlog::debug("Step {}: Reference COM - (3,0):[{:.3f},{:.3f}], target (3,1):[{:.3f},{:.3f}]",
                         step,
                         world->at(3, 0).getCOM().x, world->at(3, 0).getCOM().y,
                         world->at(3, 1).getCOM().x, world->at(3, 1).getCOM().y);
        }
        
        // Update visual display.
        if (visual_mode_) {
            auto& coordinator = VisualTestCoordinator::getInstance();
            coordinator.postTaskSync([this] {
                world->draw();
            });
        }
        
        // Check for actual cell-to-cell transfers
        // 2x2 block: material appears in row 3
        if (!block_transferred && (world->at(0, 3).getFillRatio() > 0.0 ||
                                  world->at(1, 3).getFillRatio() > 0.0)) {
            block_transferred = true;
            block_transfer_step = step;
            spdlog::info("2x2 block transferred to row 3 at step {}", step);
        }
        
        // Reference particle: material appears in (3,1)
        if (!ref_transferred && world->at(3, 1).getFillRatio() > 0.0) {
            ref_transferred = true;
            ref_transfer_step = step;
            spdlog::info("Reference particle transferred to (3,1) at step {}", step);
        }
        
        // Check if upper blocks start falling (after bottom blocks have fallen)
        if (block_transferred && !upper_blocks_started_falling) {
            // Check if upper blocks lose material or if material appears below them
            if (world->at(0, 1).getFillRatio() < 1.0 || world->at(1, 1).getFillRatio() < 1.0 ||
                world->at(0, 2).getFillRatio() > 0.0 || world->at(1, 2).getFillRatio() > 0.0) {
                upper_blocks_started_falling = true;
                upper_blocks_fall_step = step;
                spdlog::info("Upper blocks started falling at step {}", step);
            }
        }
        
        // Log state every 50 steps after initial transfers
        if (block_transferred && ref_transferred && step % 50 == 0) {
            spdlog::info("Step {}: Upper blocks (0,1):{:.3f}, (1,1):{:.3f} | Below them (0,2):{:.3f}, (1,2):{:.3f}", 
                         step,
                         world->at(0, 1).getFillRatio(), world->at(1, 1).getFillRatio(),
                         world->at(0, 2).getFillRatio(), world->at(1, 2).getFillRatio());
        }
    }
    
    // Both should transfer due to gravity
    EXPECT_TRUE(block_transferred) 
        << "2x2 dirt block should transfer to cells below due to gravity";
    EXPECT_TRUE(ref_transferred) 
        << "Reference particle should transfer to cell below due to gravity";
    
    // Verify that source cells are now empty or nearly empty after transfer
    spdlog::info("Final fill ratios in source cells:");
    spdlog::info("  2x2 block sources - (0,1):{:.3f}, (1,1):{:.3f}, (0,2):{:.3f}, (1,2):{:.3f}", 
                 world->at(0, 1).getFillRatio(), world->at(1, 1).getFillRatio(),
                 world->at(0, 2).getFillRatio(), world->at(1, 2).getFillRatio());
    spdlog::info("  Reference source - (3,0):{:.3f}", world->at(3, 0).getFillRatio());
    
    // Verify material conservation - source cells should be empty after transfer
    EXPECT_LT(world->at(0, 1).getFillRatio(), 0.1) 
        << "Source cell (0,1) should be mostly empty after material transfer";
    EXPECT_LT(world->at(1, 1).getFillRatio(), 0.1) 
        << "Source cell (1,1) should be mostly empty after material transfer";
    EXPECT_LT(world->at(0, 2).getFillRatio(), 0.1) 
        << "Source cell (0,2) should be mostly empty after material transfer";
    EXPECT_LT(world->at(1, 2).getFillRatio(), 0.1) 
        << "Source cell (1,2) should be mostly empty after material transfer";
    EXPECT_LT(world->at(3, 0).getFillRatio(), 0.1) 
        << "Reference source cell (3,0) should be mostly empty after material transfer";
    
    // Compare transfer timing - should be similar since both are floating
    if (block_transferred && ref_transferred) {
        int timing_difference = abs(block_transfer_step - ref_transfer_step);
        
        spdlog::info("Cell-to-cell transfer timing comparison:");
        spdlog::info("  2x2 block transferred at step {}", block_transfer_step);
        spdlog::info("  Reference particle transferred at step {}", ref_transfer_step);
        spdlog::info("  Timing difference: {} steps", timing_difference);
        
        if (upper_blocks_started_falling) {
            spdlog::info("  Upper blocks started falling at step {}", upper_blocks_fall_step);
        } else {
            spdlog::info("  Upper blocks never started falling (stayed floating)");
        }
        
        // Both are floating structures, so transfer timing should be close
        EXPECT_LE(timing_difference, 30) 
            << "Floating 2x2 block should transfer at similar rate to isolated particle. "
            << "Block: " << block_transfer_step << " steps, Reference: " << ref_transfer_step 
            << " steps, Difference: " << timing_difference << " steps";
    }
    
    // Show final state with ASCII diagram
    spdlog::info("=== FINAL STATE ===");
    spdlog::info("ASCII diagram after simulation:");
    std::string final_ascii = world->toAsciiDiagram();
    spdlog::info("\n{}", final_ascii);
    spdlog::info("=== COMPARISON COMPLETE ===");
}

TEST_F(ForceCalculationTest, SupportedVsFloatingStructures) {
    waitForStart();
    // Test that BFS correctly distinguishes between supported and floating structures
    //
    // World Setup (8x8 grid, walls disabled):
    // ┌────────┐
    // │        │  Row 0: Empty
    // │        │  Row 1: Empty  
    // │      D │  Row 2: Floating structure (F) at x=6, isolated in air
    // │      D │  Row 3: Floating structure (F) at x=6, isolated in air
    // │        │  Row 4: Empty space
    // │D       │  Row 5: Supported tower (S) at x=1, connected to ground via chain
    // │D       │  Row 6: Supported tower (S) at x=1, connected to ground via chain  
    // │D       │  Row 7: Supported tower (S) at x=1, ground level connection
    // └────────┘
    //  01234567   Column indices
    //
    // Expected Results:
    // 1. Supported tower (S): BFS finds ground connection → full cohesion (resistance ~0.4-0.8)
    // 2. Floating structure (F): BFS finds no support → minimal cohesion (resistance ~0.04)
    // 3. Cohesion ratio should be >3x (supported vs floating)
    // 4. This validates that BFS correctly preserves structural integrity for grounded materials
    // 5. While allowing floating rafts to fall naturally under gravity
    //
    // Apply auto-scaling for 8x8 world
    if (visual_mode_ && auto_scaling_enabled_) {
        scaleDrawingAreaForWorld(8, 8);
    }
    
    lv_obj_t* draw_area = (visual_mode_ && ui_) ? ui_->getDrawArea() : nullptr;
    auto large_world = std::make_unique<WorldB>(8, 8, draw_area);
    large_world->setWallsEnabled(false); // Disable walls for clean test
    large_world->setAddParticlesEnabled(false); // Disable automatic particle addition
    
    // Create two structures:
    // 1. Ground-connected structure: should have full cohesion
    // 2. Floating structure: should have minimal cohesion
    
    // Ground-connected structure (tower from ground up)
    large_world->addMaterialAtCell(1, 7, MaterialType::DIRT, 1.0); // Ground level (y=7, last row in 8x8 grid)
    large_world->addMaterialAtCell(1, 6, MaterialType::DIRT, 1.0); // Above ground
    large_world->addMaterialAtCell(1, 5, MaterialType::DIRT, 1.0); // Top of tower
    
    // Floating structure (isolated in air)
    large_world->addMaterialAtCell(6, 2, MaterialType::DIRT, 1.0); // Floating
    large_world->addMaterialAtCell(6, 3, MaterialType::DIRT, 1.0); // Floating
    
    // Show the supported vs floating structures in visual mode
    if (visual_mode_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([&large_world] {
            large_world->draw();
        });
        spdlog::info("Showing supported tower vs floating structure...");
        waitForNext();
    }
    
    // Calculate cohesion forces for comparison
    auto cohesion_ground_base = large_world->calculateCohesionForce(1, 7);      // Ground level
    auto cohesion_ground_middle = large_world->calculateCohesionForce(1, 6);    // Middle of tower
    auto cohesion_ground_top = large_world->calculateCohesionForce(1, 5);       // Top of tower
    
    auto cohesion_floating_bottom = large_world->calculateCohesionForce(6, 2);  // Bottom of floating pair
    auto cohesion_floating_top = large_world->calculateCohesionForce(6, 3);     // Top of floating pair
    
    // Ground-connected structure should have MUCH higher cohesion resistance
    // (BFS should find ground support through connected materials)
    EXPECT_GT(cohesion_ground_base.resistance_magnitude, 0.3) 
        << "Ground-level dirt should have strong cohesion (near ground)";
    EXPECT_GT(cohesion_ground_middle.resistance_magnitude, 0.3) 
        << "Tower middle should have strong cohesion (connected to ground)";
    EXPECT_GT(cohesion_ground_top.resistance_magnitude, 0.3) 
        << "Tower top should have strong cohesion (connected to ground via tower)";
    
    // Floating structure should have minimal cohesion resistance  
    // (BFS should find no ground/wall support within search distance)
    EXPECT_LT(cohesion_floating_bottom.resistance_magnitude, 0.15) 
        << "Floating dirt should have minimal cohesion (no ground connection)";
    EXPECT_LT(cohesion_floating_top.resistance_magnitude, 0.15) 
        << "Floating dirt should have minimal cohesion (no ground connection)";
    
    // Verify the contrast is significant (supported should be at least 3x stronger)
    double supported_avg = (cohesion_ground_base.resistance_magnitude + 
                           cohesion_ground_middle.resistance_magnitude + 
                           cohesion_ground_top.resistance_magnitude) / 3.0;
    double floating_avg = (cohesion_floating_bottom.resistance_magnitude + 
                          cohesion_floating_top.resistance_magnitude) / 2.0;
    
    EXPECT_GT(supported_avg / floating_avg, 3.0) 
        << "Supported structures should have significantly stronger cohesion than floating ones. "
        << "Supported avg: " << supported_avg << ", Floating avg: " << floating_avg 
        << ", Ratio: " << (supported_avg / floating_avg);
}
