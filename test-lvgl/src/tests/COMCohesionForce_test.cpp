#include <gtest/gtest.h>
#include "visual_test_runner.h"
#include <cmath>
#include "../WorldB.h"
#include "../MaterialType.h"
#include "../WorldCohesionCalculator.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

class COMCohesionForceTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Set up logging to see detailed debug output including trace level
        spdlog::set_level(spdlog::level::trace);
        
        // Apply auto-scaling for 7x7 world before creation
        if (visual_mode_ && auto_scaling_enabled_) {
            scaleDrawingAreaForWorld(7, 7);
        }
        
        // Create a 7x7 world for testing COM cohesion forces
        // Pass the UI draw area if in visual mode, otherwise nullptr
        lv_obj_t* draw_area = (visual_mode_ && ui_) ? ui_->getDrawArea() : nullptr;
        world = std::make_unique<WorldB>(7, 7, draw_area);
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
    
    // Helper method for automatic simulation control with COM cohesion focus
    void automaticCOMCohesionSteps(WorldB* world, int max_steps, const std::string& description) {
        if (!world) return;
        
        spdlog::info("=== COM Cohesion Test: {} ===", description);
        spdlog::info("Running {} simulation steps automatically", max_steps);
        spdlog::info("Watch for materials being pulled toward their neighbors' center");
        
        const double deltaTime = 0.016;  // ~60fps
        
        for (int step = 0; step < max_steps; ++step) {
            spdlog::info("=== Simulation Step {} ===", step + 1);
            
            // Log COM cohesion forces before movement
            if (step < 5 || step % 10 == 0) {  // Log first 5 steps, then every 10th
                logCOMCohesionForces();
            }
            
            // Advance the world one timestep
            world->advanceTime(deltaTime);
            
            // Update visual display every step
            updateVisualDisplay();
            
            // Pause for visual observation every few steps
            if (visual_mode_ && (step % 5 == 0)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
        
        spdlog::info("=== COM Cohesion Test: {} COMPLETE ===", description);
        spdlog::info("All {} steps finished", max_steps);
    }
    
    // Helper to log COM cohesion forces for all non-empty cells
    void logCOMCohesionForces() {
        if (!world) return;
        
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                const auto& cell = world->at(x, y);
                if (!cell.isEmpty()) {
                    auto com_cohesion = WorldCohesionCalculator(*world).calculateCOMCohesionForce(x, y, world->getCOMCohesionRange());
                    if (com_cohesion.force_magnitude > 0.001) {
                        spdlog::info("COM Cohesion at ({},{}): mag={:.3f}, dir=({:.3f},{:.3f}), connections={}",
                                   x, y, com_cohesion.force_magnitude,
                                   com_cohesion.force_direction.x, com_cohesion.force_direction.y,
                                   com_cohesion.active_connections);
                    }
                }
            }
        }
    }
    
    // Helper to measure distance between all particles of given material
    double measureAverageParticleDistance(MaterialType material) {
        std::vector<Vector2d> positions;
        
        for (uint32_t y = 0; y < world->getHeight(); y++) {
            for (uint32_t x = 0; x < world->getWidth(); x++) {
                const auto& cell = world->at(x, y);
                if (!cell.isEmpty() && cell.getMaterialType() == material) {
                    Vector2d world_pos(static_cast<double>(x) + cell.getCOM().x, 
                                      static_cast<double>(y) + cell.getCOM().y);
                    positions.push_back(world_pos);
                }
            }
        }
        
        if (positions.size() < 2) return 0.0;
        
        double total_distance = 0.0;
        int pair_count = 0;
        
        for (size_t i = 0; i < positions.size(); i++) {
            for (size_t j = i + 1; j < positions.size(); j++) {
                Vector2d diff = positions[i] - positions[j];
                total_distance += diff.magnitude();
                pair_count++;
            }
        }
        
        return pair_count > 0 ? total_distance / pair_count : 0.0;
    }
    
    // Helper method to find world position of nth material particle
    Vector2d findMaterialWorldPosition(MaterialType material, int particle_index) {
        int found_count = 0;
        
        for (uint32_t y = 0; y < world->getHeight(); y++) {
            for (uint32_t x = 0; x < world->getWidth(); x++) {
                const auto& cell = world->at(x, y);
                if (!cell.isEmpty() && cell.getMaterialType() == material) {
                    if (found_count == particle_index) {
                        return Vector2d(static_cast<double>(x) + cell.getCOM().x, 
                                       static_cast<double>(y) + cell.getCOM().y);
                    }
                    found_count++;
                }
            }
        }
        
        return Vector2d(0.0, 0.0); // Not found
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(COMCohesionForceTest, COMCohesionIntegrationWithPhysics) {
    spdlog::info("[TEST] Integration test: Demonstrate clear behavioral difference with/without COM cohesion");
    
    // Scenario: Two METAL particles placed horizontally with a gap
    // Without COM cohesion: they should fall straight down independently
    // With COM cohesion: they should move toward each other while falling
    
    updateVisualDisplay();
    waitForStart();
    
    spdlog::info("[TEST] Phase 1: Running WITHOUT COM cohesion (baseline behavior)");
    
    // Setup: Two METAL particles with a 1-cell gap horizontally
    world->addMaterialAtCell(2, 1, MaterialType::METAL, 1.0); // Left particle
    world->addMaterialAtCell(4, 1, MaterialType::METAL, 1.0); // Right particle (gap at x=3)
    
    // Disable COM cohesion for baseline test
    world->setCohesionForceEnabled(false);
    
    // Record initial positions
    Vector2d left_initial_world_pos(2.0 + world->at(2, 1).getCOM().x, 1.0 + world->at(2, 1).getCOM().y);
    Vector2d right_initial_world_pos(4.0 + world->at(4, 1).getCOM().x, 1.0 + world->at(4, 1).getCOM().y);
    double initial_horizontal_separation = std::abs(right_initial_world_pos.x - left_initial_world_pos.x);
    
    spdlog::info("Initial horizontal separation: {:.3f}", initial_horizontal_separation);
    
    // Run simulation WITHOUT COM cohesion
    const double deltaTime = 0.016;
    const int steps = 25;
    
    for (int step = 0; step < steps; step++) {
        world->advanceTime(deltaTime);
        updateVisualDisplay();
        
        if (visual_mode_ && (step % 3 == 0)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
        }
    }
    
    // Measure final separation WITHOUT cohesion
    Vector2d left_baseline_pos = findMaterialWorldPosition(MaterialType::METAL, 0); // First METAL particle
    Vector2d right_baseline_pos = findMaterialWorldPosition(MaterialType::METAL, 1); // Second METAL particle
    double baseline_horizontal_separation = std::abs(right_baseline_pos.x - left_baseline_pos.x);
    
    spdlog::info("Baseline final horizontal separation: {:.3f}", baseline_horizontal_separation);
    spdlog::info("Baseline separation change: {:.3f}", initial_horizontal_separation - baseline_horizontal_separation);
    
    // Reset world for second test
    spdlog::info("[TEST] Phase 2: Running WITH COM cohesion (should show attraction)");
    world->reset();
    world->addMaterialAtCell(2, 1, MaterialType::METAL, 1.0); // Left particle
    world->addMaterialAtCell(4, 1, MaterialType::METAL, 1.0); // Right particle
    
    // Enable COM cohesion
    world->setCohesionForceEnabled(true);
    updateVisualDisplay();
    
    // Run simulation WITH COM cohesion
    for (int step = 0; step < steps; step++) {
        if (step < 5) {
            // Log COM forces for first few steps
            auto left_com_force = WorldCohesionCalculator(*world).calculateCOMCohesionForce(2, 1, world->getCOMCohesionRange());
            auto right_com_force = WorldCohesionCalculator(*world).calculateCOMCohesionForce(4, 1, world->getCOMCohesionRange());
            spdlog::info("Step {}: Left COM force=({:.3f},{:.3f}), Right COM force=({:.3f},{:.3f})",
                        step, left_com_force.force_direction.x, left_com_force.force_direction.y,
                        right_com_force.force_direction.x, right_com_force.force_direction.y);
        }
        
        world->advanceTime(deltaTime);
        updateVisualDisplay();
        
        if (visual_mode_ && (step % 3 == 0)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
        }
    }
    
    // Measure final separation WITH cohesion
    Vector2d left_cohesion_pos = findMaterialWorldPosition(MaterialType::METAL, 0);
    Vector2d right_cohesion_pos = findMaterialWorldPosition(MaterialType::METAL, 1);
    double cohesion_horizontal_separation = std::abs(right_cohesion_pos.x - left_cohesion_pos.x);
    
    spdlog::info("With-cohesion final horizontal separation: {:.3f}", cohesion_horizontal_separation);
    
    // Calculate the cohesion effect
    double cohesion_effect = baseline_horizontal_separation - cohesion_horizontal_separation;
    double cohesion_percentage = (initial_horizontal_separation > 0) ? 
        (cohesion_effect / initial_horizontal_separation) * 100.0 : 0.0;
    
    spdlog::info("COM cohesion effect: {:.3f} units ({:.1f}% of initial separation)", 
                cohesion_effect, cohesion_percentage);
    
    waitForNext();
    
    // CRITICAL TEST: COM cohesion should cause particles to move closer together
    EXPECT_GT(cohesion_effect, 0.15)
        << "COM cohesion should reduce horizontal separation by at least 0.15 units. "
        << "Baseline separation: " << baseline_horizontal_separation 
        << ", With-cohesion separation: " << cohesion_horizontal_separation
        << ", Effect: " << cohesion_effect
        << ". This suggests COM cohesion forces are too weak or not being applied correctly.";
    
    EXPECT_GT(cohesion_percentage, 7.5)
        << "Expected COM cohesion to reduce separation by at least 7.5%, got " << cohesion_percentage << "%";
    
    // Additional check: particles should be closer than baseline
    EXPECT_LT(cohesion_horizontal_separation, baseline_horizontal_separation)
        << "Particles with COM cohesion should be closer together than without it";
}

TEST_F(COMCohesionForceTest, COMCohesionClusteringQuantitative) {
    spdlog::info("[TEST] Quantitative test: COM cohesion should reduce average distance between particles");
    
    // Create scattered DIRT particles
    world->addMaterialAtCell(1, 1, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(5, 1, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(1, 5, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(5, 5, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(3, 3, MaterialType::DIRT, 1.0);
    
    // Measure initial average distance
    double initial_distance = measureAverageParticleDistance(MaterialType::DIRT);
    spdlog::info("Initial average particle distance: {:.3f}", initial_distance);
    
    updateVisualDisplay();
    waitForStart();
    
    // Enable COM cohesion
    world->setCohesionForceEnabled(true);
    
    // Run simulation
    const double deltaTime = 0.016;
    const int steps = 30;
    
    for (int step = 0; step < steps; step++) {
        world->advanceTime(deltaTime);
        updateVisualDisplay();
        
        if (step % 5 == 0) {
            double current_distance = measureAverageParticleDistance(MaterialType::DIRT);
            spdlog::info("Step {}: Average distance = {:.3f}", step, current_distance);
        }
        
        if (visual_mode_ && (step % 3 == 0)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    }
    
    // Measure final distance
    double final_distance = measureAverageParticleDistance(MaterialType::DIRT);
    double distance_reduction = initial_distance - final_distance;
    double reduction_percentage = (initial_distance > 0) ? (distance_reduction / initial_distance) * 100.0 : 0.0;
    
    spdlog::info("Final average particle distance: {:.3f}", final_distance);
    spdlog::info("Distance reduction: {:.3f} ({:.1f}%)", distance_reduction, reduction_percentage);
    
    waitForNext();
    
    // CRITICAL TEST: Average distance should decrease due to clustering
    EXPECT_GT(distance_reduction, 0.5) 
        << "COM cohesion should cause particles to cluster together, reducing average distance by at least 0.5 units. "
        << "Initial: " << initial_distance << ", Final: " << final_distance << ", Reduction: " << distance_reduction
        << ". This suggests COM cohesion forces are not effectively pulling particles together.";
        
    EXPECT_GT(reduction_percentage, 10.0)
        << "Expected at least 10% reduction in particle distances, got " << reduction_percentage << "%";
}

TEST_F(COMCohesionForceTest, COMCohesionMaterialStrengthComparison) {
    spdlog::info("[TEST] Testing that different materials show different cohesion strengths");
    
    updateVisualDisplay();
    waitForStart();
    
    // Test WATER particles (low cohesion = 0.1)
    world->addMaterialAtCell(1, 2, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(3, 2, MaterialType::WATER, 1.0);
    
    world->setCohesionForceEnabled(true);
    
    // Run WATER test
    const double deltaTime = 0.016;
    double water_initial_distance = measureAverageParticleDistance(MaterialType::WATER);
    
    for (int step = 0; step < 15; step++) {
        world->advanceTime(deltaTime);
        updateVisualDisplay();
        if (visual_mode_ && (step % 2 == 0)) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    double water_final_distance = measureAverageParticleDistance(MaterialType::WATER);
    double water_clustering = water_initial_distance - water_final_distance;
    
    spdlog::info("WATER clustering: {:.3f} -> {:.3f} (change: {:.3f})", 
                water_initial_distance, water_final_distance, water_clustering);
    
    // Clear and test METAL particles (high cohesion = 0.9)
    world->reset();
    world->addMaterialAtCell(1, 2, MaterialType::METAL, 1.0);
    world->addMaterialAtCell(3, 2, MaterialType::METAL, 1.0);
    updateVisualDisplay();
    
    double metal_initial_distance = measureAverageParticleDistance(MaterialType::METAL);
    
    for (int step = 0; step < 15; step++) {
        world->advanceTime(deltaTime);
        updateVisualDisplay();
        if (visual_mode_ && (step % 2 == 0)) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    double metal_final_distance = measureAverageParticleDistance(MaterialType::METAL);
    double metal_clustering = metal_initial_distance - metal_final_distance;
    
    spdlog::info("METAL clustering: {:.3f} -> {:.3f} (change: {:.3f})", 
                metal_initial_distance, metal_final_distance, metal_clustering);
    
    waitForNext();
    
    // CRITICAL TEST: METAL should cluster more strongly than WATER
    EXPECT_GT(metal_clustering, water_clustering)
        << "METAL (cohesion=0.9) should cluster more strongly than WATER (cohesion=0.1). "
        << "Metal clustering: " << metal_clustering << ", Water clustering: " << water_clustering
        << ". This indicates material properties may not be properly affecting COM cohesion forces.";
        
    double clustering_ratio = (water_clustering > 0) ? metal_clustering / water_clustering : 100.0;
    spdlog::info("[TEST] Clustering strength ratio (Metal/Water): {:.2f}", clustering_ratio);
    
    EXPECT_GT(clustering_ratio, 2.0)
        << "Expected METAL to cluster at least 2x more than WATER, got ratio: " << clustering_ratio;
}

TEST_F(COMCohesionForceTest, EmptyCellHasZeroCOMCohesion) {
    auto com_cohesion = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    
    EXPECT_EQ(com_cohesion.force_magnitude, 0.0);
    EXPECT_EQ(com_cohesion.active_connections, 0);
    EXPECT_EQ(com_cohesion.force_direction.x, 0.0);
    EXPECT_EQ(com_cohesion.force_direction.y, 0.0);
}

TEST_F(COMCohesionForceTest, IsolatedCellHasZeroCOMCohesion) {
    world->addMaterialAtCell(3, 3, MaterialType::DIRT, 1.0);
    
    auto com_cohesion = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    
    // No same-material neighbors = no COM cohesion force
    EXPECT_EQ(com_cohesion.force_magnitude, 0.0);
    EXPECT_EQ(com_cohesion.active_connections, 0);
    EXPECT_EQ(com_cohesion.force_direction.x, 0.0);
    EXPECT_EQ(com_cohesion.force_direction.y, 0.0);
}

TEST_F(COMCohesionForceTest, CellWithNeighborsHasCOMCohesion) {
    // Setup: Center cell with one neighbor to the right
    world->addMaterialAtCell(3, 3, MaterialType::DIRT, 1.0); // Center
    world->addMaterialAtCell(4, 3, MaterialType::DIRT, 1.0); // Right neighbor
    
    auto com_cohesion = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    
    // Should have COM cohesion force toward the right neighbor
    EXPECT_GT(com_cohesion.force_magnitude, 0.0);
    EXPECT_EQ(com_cohesion.active_connections, 1);
    EXPECT_GT(com_cohesion.force_direction.x, 0.0); // Force toward right neighbor
    EXPECT_EQ(com_cohesion.force_direction.y, 0.0); // No vertical component
}

TEST_F(COMCohesionForceTest, COMCohesionScalesWithMaterialProperties) {
    // Test with WATER (low cohesion = 0.1)
    world->addMaterialAtCell(3, 3, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(4, 3, MaterialType::WATER, 1.0);
    auto water_cohesion = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    
    // Clear and test with METAL (high cohesion = 0.9)
    world->at(3, 3) = CellB(MaterialType::AIR, 0.0);
    world->at(4, 3) = CellB(MaterialType::AIR, 0.0);
    world->addMaterialAtCell(3, 3, MaterialType::METAL, 1.0);
    world->addMaterialAtCell(4, 3, MaterialType::METAL, 1.0);
    auto metal_cohesion = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    
    // METAL should have much stronger COM cohesion than WATER
    EXPECT_GT(metal_cohesion.force_magnitude, water_cohesion.force_magnitude);
    EXPECT_GT(metal_cohesion.force_magnitude, water_cohesion.force_magnitude * 5.0); // At least 5x stronger
}

TEST_F(COMCohesionForceTest, COMCohesionDirectionPointsTowardNeighborCenter) {
    // Setup: Center cell with neighbors forming an L shape
    world->addMaterialAtCell(3, 3, MaterialType::DIRT, 1.0); // Center (3,3)
    world->addMaterialAtCell(4, 3, MaterialType::DIRT, 1.0); // Right (4,3)
    world->addMaterialAtCell(3, 4, MaterialType::DIRT, 1.0); // Below (3,4)
    
    auto com_cohesion = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    
    // Force should point toward the center of neighbors: (4+3)/2=3.5, (3+4)/2=3.5
    // So force direction from (3,3) toward (3.5,3.5) should be (+0.5,+0.5) normalized
    EXPECT_GT(com_cohesion.force_magnitude, 0.0);
    EXPECT_EQ(com_cohesion.active_connections, 2);
    EXPECT_GT(com_cohesion.force_direction.x, 0.0); // Toward right
    EXPECT_GT(com_cohesion.force_direction.y, 0.0); // Toward below
    
    // Force direction should be normalized
    double force_magnitude = sqrt(com_cohesion.force_direction.x * com_cohesion.force_direction.x + 
                                 com_cohesion.force_direction.y * com_cohesion.force_direction.y);
    EXPECT_NEAR(force_magnitude, 1.0, 0.001); // Should be normalized to unit vector
}

TEST_F(COMCohesionForceTest, COMCohesionIncreasesWithMoreNeighbors) {
    // Test with 1 neighbor
    world->addMaterialAtCell(3, 3, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(4, 3, MaterialType::DIRT, 1.0);
    auto cohesion_1_neighbor = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    
    // Add second neighbor
    world->addMaterialAtCell(2, 3, MaterialType::DIRT, 1.0);
    auto cohesion_2_neighbors = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    
    // Add third neighbor
    world->addMaterialAtCell(3, 2, MaterialType::DIRT, 1.0);
    auto cohesion_3_neighbors = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    
    // More neighbors should generally increase force magnitude (more connection factor)
    EXPECT_EQ(cohesion_1_neighbor.active_connections, 1);
    EXPECT_EQ(cohesion_2_neighbors.active_connections, 2);
    EXPECT_EQ(cohesion_3_neighbors.active_connections, 3);
    EXPECT_GT(cohesion_2_neighbors.force_magnitude, cohesion_1_neighbor.force_magnitude);
    EXPECT_GT(cohesion_3_neighbors.force_magnitude, cohesion_2_neighbors.force_magnitude);
}

TEST_F(COMCohesionForceTest, COMCohesionClusterFormation) {
    // Enable restart functionality for interactive clustering test
    spdlog::info("[TEST] Enabling restart functionality for COM cohesion cluster formation test");
    enableTestRestart();
    
    int iteration = 0;
    do {
        ++iteration;
        spdlog::info("[TEST] Starting COM cohesion cluster formation test iteration {}", iteration);
        
        clearRestartRequest();
        
        spdlog::info("[TEST] Setting up test scenario: Scattered DIRT particles for clustering");
        
        // Create scattered DIRT particles that should form a cluster via COM cohesion
        world->addMaterialAtCell(1, 1, MaterialType::DIRT, 1.0); // Top-left
        world->addMaterialAtCell(5, 1, MaterialType::DIRT, 1.0); // Top-right
        world->addMaterialAtCell(1, 5, MaterialType::DIRT, 1.0); // Bottom-left
        world->addMaterialAtCell(5, 5, MaterialType::DIRT, 1.0); // Bottom-right
        world->addMaterialAtCell(3, 3, MaterialType::DIRT, 1.0); // Center
        
        // Enable COM cohesion forces
        world->setCohesionForceEnabled(true);
        
        // Show initial scattered setup
        updateVisualDisplay();
        waitForStart();
        
        spdlog::info("[TEST] Particles should be pulled toward the cluster center over time");
        
        // Run extended simulation to observe clustering behavior
        automaticCOMCohesionSteps(world.get(), 20, "Scattered DIRT particles clustering");
        
        // Clear the world for next restart
        if (visual_mode_ && shouldRestartTest()) {
            spdlog::info("[TEST] Restart requested - clearing world for next iteration");
            world.reset();
            world = std::make_unique<WorldB>(7, 7, ui_ ? ui_->getDrawArea() : nullptr);
            world->setWallsEnabled(false);
            world->setAddParticlesEnabled(false);
            if (ui_) {
                auto& coordinator = VisualTestCoordinator::getInstance();
                coordinator.postTaskSync([this] {
                    ui_->setWorld(world.get());
                });
            }
        }
        
    } while (shouldRestartTest() && visual_mode_);
    
    disableTestRestart();
}

TEST_F(COMCohesionForceTest, COMCohesionRangeConfiguration) {
    spdlog::info("[TEST] Testing COM cohesion range configuration functionality");
    
    // Place DIRT particles at different distances from a center particle
    world->addMaterialAtCell(3, 3, MaterialType::DIRT, 1.0); // Center
    world->addMaterialAtCell(5, 3, MaterialType::DIRT, 1.0); // Distance 2 (horizontal)
    world->addMaterialAtCell(3, 1, MaterialType::DIRT, 1.0); // Distance 2 (vertical)
    world->addMaterialAtCell(6, 3, MaterialType::DIRT, 1.0); // Distance 3 (horizontal)
    
    // Test default range (should be 2)
    EXPECT_EQ(world->getCOMCohesionRange(), 2);
    
    // Test range 1 (should only see adjacent neighbors - none in this case)
    world->setCOMCohesionRange(1);
    auto force_r1 = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    EXPECT_EQ(world->getCOMCohesionRange(), 1);
    EXPECT_EQ(force_r1.active_connections, 0) << "Range 1 should find 0 connections at distance 2";
    
    // Test range 2 (should see distance 2 neighbors)  
    world->setCOMCohesionRange(2);
    auto force_r2 = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    EXPECT_EQ(world->getCOMCohesionRange(), 2);
    EXPECT_EQ(force_r2.active_connections, 2) << "Range 2 should find 2 connections at distance 2";
    
    // Test range 3 (should see distance 2 and 3 neighbors)
    world->setCOMCohesionRange(3);
    auto force_r3 = WorldCohesionCalculator(*world).calculateCOMCohesionForce(3, 3, world->getCOMCohesionRange());
    EXPECT_EQ(world->getCOMCohesionRange(), 3);
    EXPECT_EQ(force_r3.active_connections, 3) << "Range 3 should find 3 connections at distance 2-3";
    
    spdlog::info("Range test results: R1={} connections, R2={} connections, R3={} connections",
                 force_r1.active_connections, force_r2.active_connections, force_r3.active_connections);
}

TEST_F(COMCohesionForceTest, COMCohesionToggleButton) {
    // Enable restart functionality for interactive toggle testing
    spdlog::info("[TEST] Enabling restart functionality for COM cohesion toggle test");
    enableTestRestart();
    
    int iteration = 0;
    do {
        ++iteration;
        spdlog::info("[TEST] Starting COM cohesion toggle test iteration {}", iteration);
        
        clearRestartRequest();
        
        spdlog::info("[TEST] Setting up test scenario: Test the Cohesion Force toggle button");
        
        // Create a simple setup with two adjacent particles
        world->addMaterialAtCell(2, 3, MaterialType::METAL, 1.0);
        world->addMaterialAtCell(3, 3, MaterialType::METAL, 1.0);
        
        // Show initial setup
        updateVisualDisplay();
        waitForStart();
        
        spdlog::info("[TEST] This test demonstrates COM cohesion force behavior");
        spdlog::info("[TEST] COM cohesion should pull particles toward each other");
        spdlog::info("[TEST] Note: Cohesion Force toggle is available in main app, not test UI");
        
        // Run automatic simulation to demonstrate COM cohesion
        automaticCOMCohesionSteps(world.get(), 15, "COM cohesion forces demonstration");
        
        // Clear the world for next restart
        if (visual_mode_ && shouldRestartTest()) {
            spdlog::info("[TEST] Restart requested - clearing world for next iteration");
            world.reset();
            world = std::make_unique<WorldB>(7, 7, ui_ ? ui_->getDrawArea() : nullptr);
            world->setWallsEnabled(false);
            world->setAddParticlesEnabled(false);
            if (ui_) {
                auto& coordinator = VisualTestCoordinator::getInstance();
                coordinator.postTaskSync([this] {
                    ui_->setWorld(world.get());
                });
            }
        }
        
    } while (shouldRestartTest() && visual_mode_);
    
    disableTestRestart();
}

TEST_F(COMCohesionForceTest, VelocityConservationAfterHorizontalCollision) {
    spdlog::info("[TEST] Testing Y-velocity conservation after horizontal dirt-dirt collision");
    
    // Create a 3x4 world specifically for this collision test
    // Layout: D-D
    //         --D
    //         --D
    //         --D
    world.reset();
    lv_obj_t* draw_area = (visual_mode_ && ui_) ? ui_->getDrawArea() : nullptr;
    world = std::make_unique<WorldB>(3, 4, draw_area);
    world->setWallsEnabled(false);
    world->setAddParticlesEnabled(false);
    
    // Connect UI to the new world
    if (visual_mode_ && ui_) {
        auto& coordinator = VisualTestCoordinator::getInstance();
        coordinator.postTaskSync([this] {
            ui_->setWorld(world.get());
        });
    }
    
    // Disable all cohesion and adhesion forces for clean collision testing
    world->setCohesionForceEnabled(false);  // Disable COM cohesion forces
    world->setCohesionEnabled(false);       // Disable cohesion binding resistance
    world->setAdhesionEnabled(false);       // Disable adhesion forces
    
    // Set up the test scenario
    world->addMaterialAtCell(0, 0, MaterialType::DIRT, 1.0); // Moving particle (top-left)
    world->addMaterialAtCell(2, 0, MaterialType::DIRT, 1.0); // Column particles (right column)
    world->addMaterialAtCell(2, 1, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(2, 2, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(2, 3, MaterialType::DIRT, 1.0);
    
    // Set initial velocity:
    const Vector2d v(4.0, 1.0);
    world->at(0, 0).setVelocity(v);
    
    spdlog::info("Initial setup:");
    spdlog::info("  Moving particle at (0,0) with velocity: " + v.toString());
    spdlog::info("  Static dirt column at x=2, y=0-3");
    spdlog::info("  All cohesion and adhesion forces DISABLED for clean collision test");
    spdlog::info("  Expected: Y-velocity should be preserved through collision");
    
    updateVisualDisplay();
    if (visual_mode_) {
        waitForStart();
    }
    
    // Track Y-velocity throughout the simulation
    std::vector<double> y_velocities;
    std::vector<Vector2d> positions;
    std::vector<int> step_numbers;
    
    const double deltaTime = 0.016;
    const int max_steps = 300;
    double max_y_velocity = 1.0; // Initial Y-velocity
    bool collision_detected = false;
    int collision_step = -1;
    
    for (int step = 0; step < max_steps; ++step) {
        // Advance simulation FIRST
        world->advanceTime(deltaTime);
        updateVisualDisplay();
        
        // THEN find the moving particle (after potential transfers)
        CellB* moving_particle = nullptr;
        Vector2d current_position(0, 0);
        
        for (uint32_t y = 0; y < world->getHeight(); ++y) {
            for (uint32_t x = 0; x < world->getWidth(); ++x) {
                auto& cell = world->at(x, y);
                if (!cell.isEmpty() && cell.getMaterialType() == MaterialType::DIRT) {
                    // Check if this is the moving particle (has significant velocity)
                    Vector2d vel = cell.getVelocity();
                    // Prefer the particle with highest horizontal velocity (most likely the original moving one)
                    if (vel.mag() > 0.1 && vel.x > 0.1) {
                        moving_particle = &cell;
                        current_position = Vector2d(static_cast<double>(x) + cell.getCOM().x, 
                                                  static_cast<double>(y) + cell.getCOM().y);
                        break;
                    }
                }
            }
            if (moving_particle) break;
        }
        
        if (moving_particle) {
            Vector2d velocity = moving_particle->getVelocity();
            y_velocities.push_back(velocity.y);
            positions.push_back(current_position);
            step_numbers.push_back(step);
            
            // Track maximum Y-velocity reached
            max_y_velocity = std::max(max_y_velocity, velocity.y);
            
            // Detect collision (when particle reaches x >= 1.5, it's colliding with column)
            if (!collision_detected && current_position.x >= 1.5) {
                collision_detected = true;
                collision_step = step;
                spdlog::info("COLLISION DETECTED at step {} - position ({:.3f}, {:.3f}), velocity ({:.3f}, {:.3f})",
                           step, current_position.x, current_position.y, velocity.x, velocity.y);
            }
            
            // Log key moments
            if (step < 5 || step % 10 == 0 || collision_detected) {
                spdlog::info("Step {}: pos=({:.3f},{:.3f}), vel=({:.3f},{:.3f})", 
                           step, current_position.x, current_position.y, velocity.x, velocity.y);
            }
        }
        
        // Log all DIRT particles after physics step to debug particle tracking
        if (step >= 15) {
            spdlog::info("=== POST-PHYSICS DEBUG - Step {} ===", step);
            for (uint32_t y = 0; y < world->getHeight(); ++y) {
                for (uint32_t x = 0; x < world->getWidth(); ++x) {
                    auto& cell = world->at(x, y);
                    if (!cell.isEmpty() && cell.getMaterialType() == MaterialType::DIRT) {
                        Vector2d vel = cell.getVelocity();
                        Vector2d pos = Vector2d(static_cast<double>(x) + cell.getCOM().x, 
                                              static_cast<double>(y) + cell.getCOM().y);
                        spdlog::info("  DIRT at ({},{}) - pos=({:.3f},{:.3f}), vel=({:.3f},{:.3f}), mag={:.3f}",
                                   x, y, pos.x, pos.y, vel.x, vel.y, vel.mag());
                    }
                }
            }
        }
        
        // Stop if particle has fallen below world or velocity is very low
        if (current_position.y > 4.0 || (moving_particle && moving_particle->getVelocity().mag() < 0.01)) {
            spdlog::info("Stopping simulation at step {} - particle settled or fell", step);
            break;
        }
    }
    
    if (visual_mode_) {
        waitForNext();
    }
    
    // Analyze results
    spdlog::info("=== VELOCITY ANALYSIS ===");
    spdlog::info("Total steps tracked: {}", y_velocities.size());
    spdlog::info("Collision detected: {} at step {}", collision_detected ? "YES" : "NO", collision_step);
    spdlog::info("Maximum Y-velocity reached: {:.3f}", max_y_velocity);
    
    if (y_velocities.size() >= 2) {
        double initial_y_vel = y_velocities[0];
        double final_y_vel = y_velocities.back();
        spdlog::info("Initial Y-velocity: {:.3f}", initial_y_vel);
        spdlog::info("Final Y-velocity: {:.3f}", final_y_vel);
        
        // Check for velocity conservation violations
        double max_decrease = 0.0;
        for (size_t i = 1; i < y_velocities.size(); ++i) {
            double decrease = y_velocities[i-1] - y_velocities[i];
            if (decrease > max_decrease) {
                max_decrease = decrease;
                spdlog::info("Largest Y-velocity decrease: {:.3f} at step {} (from {:.3f} to {:.3f})",
                           decrease, step_numbers[i], y_velocities[i-1], y_velocities[i]);
            }
        }
        
        // CRITICAL TEST: Y-velocity should never significantly decrease before collision with ground
        // Allow small decreases due to numerical precision or slight damping
        double allowable_decrease = 0.2; // Allow up to 0.2 units decrease per step
        
        EXPECT_LT(max_decrease, allowable_decrease)
            << "Y-velocity decreased by " << max_decrease << " in a single step, which violates momentum conservation. "
            << "Expected max decrease < " << allowable_decrease << ". "
            << "This suggests the cohesion resistance bug is still present.";
        
        // Additional check: if collision occurred, verify Y-velocity was maintained through collision
        if (collision_detected && collision_step >= 0 && collision_step < static_cast<int>(y_velocities.size() - 5)) {
            double pre_collision_y_vel = y_velocities[collision_step];
            double post_collision_y_vel = y_velocities[collision_step + 3]; // 3 steps after collision
            double collision_velocity_loss = pre_collision_y_vel - post_collision_y_vel;
            
            spdlog::info("Pre-collision Y-velocity: {:.3f}", pre_collision_y_vel);
            spdlog::info("Post-collision Y-velocity: {:.3f}", post_collision_y_vel);
            spdlog::info("Y-velocity loss during collision: {:.3f}", collision_velocity_loss);
            
            EXPECT_LT(collision_velocity_loss, 0.5)
                << "Y-velocity loss during collision (" << collision_velocity_loss 
                << ") is too large. Expected < 0.5. "
                << "This suggests the cohesion force is interfering with gravity after collision.";
        }
        
    } else {
        FAIL() << "Test failed to track particle velocity - no data collected";
    }
}
