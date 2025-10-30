#include "visual_test_runner.h"
#include "../World.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

class AirResistanceTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Create world with 5x5 grid for more room.
        world = createWorldB(5, 5);
        
        // Apply test-specific defaults.
        world->setAddParticlesEnabled(false);
        world->setWallsEnabled(false);
        world->setAirResistanceEnabled(true);  // ENABLE air resistance for this test.
        world->setAirResistanceStrength(0.5);  // Increase from default 0.1 to 0.5 for more noticeable effect.
        world->setCohesionBindForceEnabled(false);  // Disable cohesion to isolate air resistance.
        world->setCohesionComForceEnabled(false);
        world->setAdhesionEnabled(false);
        world->setGravity(0.0);  // Disable gravity to test air resistance in isolation.
        // Don't call setup() here - it adds default materials we don't want.
    }
    
    
    std::unique_ptr<WorldInterface> world;
};

TEST_F(AirResistanceTest, AirResistanceSlowsMovement) {
    spdlog::info("Starting AirResistanceTest::AirResistanceSlowsMovement");
    
    // Reset world.
    world->reset();
    
    // Add SAND and METAL particles at the top, separated by a cell.
    // SAND at x=1, METAL at x=3 (separated by empty cell at x=2).
    uint32_t sandX = 1, sandY = 0;
    uint32_t metalX = 3, metalY = 0;
    
    world->addMaterialAtCell(sandX, sandY, MaterialType::SAND, 1.0);
    world->addMaterialAtCell(metalX, metalY, MaterialType::METAL, 1.0);
    
    // Give them the same initial velocity.
    World* worldB = dynamic_cast<World*>(world.get());
    ASSERT_NE(worldB, nullptr);
    
    Vector2d initialVelocity(0.0, 5.0);  // Fast downward velocity.
    worldB->at(sandX, sandY).setVelocity(initialVelocity);
    worldB->at(metalX, metalY).setVelocity(initialVelocity);
    
    // Track position and velocity over time for both particles.
    std::vector<double> sandVelocities;
    std::vector<double> metalVelocities;
    std::vector<double> sandPositions;
    std::vector<double> metalPositions;
    
    spdlog::info("World settings before simulation:\n{}", world->settingsToString());
    spdlog::info("Material densities - SAND: {:.1f}, METAL: {:.1f}", 
                 getMaterialProperties(MaterialType::SAND).density,
                 getMaterialProperties(MaterialType::METAL).density);
    
    logWorldState(worldB, "Initial state after adding both particles");
    
    // Verify both particles were added.
    ASSERT_FALSE(worldB->at(sandX, sandY).isEmpty()) << "SAND particle was not added";
    ASSERT_EQ(worldB->at(sandX, sandY).getMaterialType(), MaterialType::SAND);
    ASSERT_FALSE(worldB->at(metalX, metalY).isEmpty()) << "METAL particle was not added";
    ASSERT_EQ(worldB->at(metalX, metalY).getMaterialType(), MaterialType::METAL);
    
    // Track current positions.
    uint32_t currentSandX = sandX, currentSandY = sandY;
    uint32_t currentMetalX = metalX, currentMetalY = metalY;
    
    for (int step = 0; step < 10; ++step) {
        // Record current velocities and positions for both particles.
        Cell& sandCell = worldB->at(currentSandX, currentSandY);
        Cell& metalCell = worldB->at(currentMetalX, currentMetalY);
        
        sandVelocities.push_back(sandCell.getVelocity().y);
        metalVelocities.push_back(metalCell.getVelocity().y);
        sandPositions.push_back(currentSandY + sandCell.getCOM().y);
        metalPositions.push_back(currentMetalY + metalCell.getCOM().y);
        
        spdlog::info("Step {}: SAND velocity={:.3f}, METAL velocity={:.3f}", 
                     step, sandCell.getVelocity().y, metalCell.getVelocity().y);
        
        // Advance simulation.
        world->advanceTime(0.016);
        
        logWorldState(worldB, fmt::format("After timestep {}", step));
        
        // Find where the particles moved.
        bool foundSand = false, foundMetal = false;
        
        // Search for SAND.
        for (uint32_t y = 0; y < 5 && !foundSand; ++y) {
            for (uint32_t x = 0; x < 5; ++x) {
                Cell& cell = worldB->at(x, y);
                if (!cell.isEmpty() && cell.getMaterialType() == MaterialType::SAND) {
                    currentSandX = x;
                    currentSandY = y;
                    foundSand = true;
                    break;
                }
            }
        }
        
        // Search for METAL.
        for (uint32_t y = 0; y < 5 && !foundMetal; ++y) {
            for (uint32_t x = 0; x < 5; ++x) {
                Cell& cell = worldB->at(x, y);
                if (!cell.isEmpty() && cell.getMaterialType() == MaterialType::METAL) {
                    currentMetalX = x;
                    currentMetalY = y;
                    foundMetal = true;
                    break;
                }
            }
        }
        
        ASSERT_TRUE(foundSand) << "Lost track of SAND particle at step " << step;
        ASSERT_TRUE(foundMetal) << "Lost track of METAL particle at step " << step;
    }
    
    // Analyze results.
    spdlog::info("=== Final Analysis ===");
    
    // Calculate average velocity reduction for each material.
    double sandInitialVel = sandVelocities.front();
    double sandFinalVel = sandVelocities.back();
    double metalInitialVel = metalVelocities.front();
    double metalFinalVel = metalVelocities.back();
    
    double sandVelReduction = (sandInitialVel - sandFinalVel) / sandInitialVel;
    double metalVelReduction = (metalInitialVel - metalFinalVel) / metalInitialVel;
    
    spdlog::info("SAND velocity reduction: {:.1f}% (from {:.3f} to {:.3f})",
                 sandVelReduction * 100, sandInitialVel, sandFinalVel);
    spdlog::info("METAL velocity reduction: {:.1f}% (from {:.3f} to {:.3f})",
                 metalVelReduction * 100, metalInitialVel, metalFinalVel);
    
    // Expected behavior: Both should show air resistance effects.
    // With gravity disabled, particles should slow down from initial velocity.
    double expectedVelocityWithoutResistance = 5.0;  // No gravity, so velocity should remain constant without resistance.
    
    spdlog::info("Expected velocity without resistance: {:.3f}", expectedVelocityWithoutResistance);
    
    // Both particles should have lower velocity due to air resistance.
    EXPECT_LT(sandFinalVel, expectedVelocityWithoutResistance * 0.95) 
        << "SAND should be slowed by air resistance";
    EXPECT_LT(metalFinalVel, expectedVelocityWithoutResistance * 0.96) 
        << "METAL should be slowed by air resistance (less affected due to higher density)";
    
    // Since METAL is much denser (7.8) than SAND (1.8), it should be significantly less affected by air resistance.
    // and maintain more of its velocity.
    EXPECT_GT(metalFinalVel, sandFinalVel) 
        << "Much denser METAL should maintain higher velocity than lighter SAND due to less air resistance effect";
}

TEST_F(AirResistanceTest, DenserMaterialsLessAffected) {
    spdlog::info("Starting AirResistanceTest::DenserMaterialsLessAffected");
    
    // Reset world.
    world->reset();
    
    // Add two materials with different densities side by side.
    world->addMaterialAtCell(1, 0, MaterialType::WATER, 1.0);  // Density 1.0.
    world->addMaterialAtCell(3, 0, MaterialType::METAL, 1.0);  // Density 8.0.
    
    World* worldB = dynamic_cast<World*>(world.get());
    ASSERT_NE(worldB, nullptr);
    
    // Give them the same initial velocity.
    Vector2d initialVelocity(0.0, 5.0);
    worldB->at(1, 0).setVelocity(initialVelocity);
    worldB->at(3, 0).setVelocity(initialVelocity);
    
    // Simulate for several steps.
    for (int step = 0; step < 5; ++step) {
        world->advanceTime(0.016);
    }
    
    // Find final velocities.
    double waterVelocity = 0.0;
    double metalVelocity = 0.0;
    
    for (uint32_t y = 0; y < 5; ++y) {
        for (uint32_t x = 0; x < 5; ++x) {
            Cell& cell = worldB->at(x, y);
            if (!cell.isEmpty()) {
                if (cell.getMaterialType() == MaterialType::WATER) {
                    waterVelocity = cell.getVelocity().mag();
                } else if (cell.getMaterialType() == MaterialType::METAL) {
                    metalVelocity = cell.getVelocity().mag();
                }
            }
        }
    }
    
    spdlog::info("Water velocity after air resistance: {:.3f}", waterVelocity);
    spdlog::info("Metal velocity after air resistance: {:.3f}", metalVelocity);
    
    // Metal should maintain more of its velocity due to higher density.
    EXPECT_GT(metalVelocity, waterVelocity * 1.1) 
        << "Denser metal should be less affected by air resistance than water";
}

TEST_F(AirResistanceTest, AirResistanceCanBeDisabled) {
    spdlog::info("Starting AirResistanceTest::AirResistanceCanBeDisabled");
    
    // First run with air resistance enabled.
    world->reset();
    world->setAirResistanceEnabled(true);
    
    world->addMaterialAtCell(2, 0, MaterialType::SAND, 1.0);
    World* worldB = dynamic_cast<World*>(world.get());
    worldB->at(2, 0).setVelocity(Vector2d(3.0, 0.0));  // Horizontal velocity.
    
    // Run for a few steps.
    for (int i = 0; i < 5; ++i) {
        world->advanceTime(0.016);
    }
    
    // Find sand and check velocity (it may have moved).
    double velocityWithResistance = 0.0;
    bool foundSand = false;
    for (uint32_t y = 0; y < 5 && !foundSand; ++y) {
        for (uint32_t x = 0; x < 5; ++x) {
            Cell& cell = worldB->at(x, y);
            if (!cell.isEmpty() && cell.getMaterialType() == MaterialType::SAND) {
                velocityWithResistance = cell.getVelocity().x;
                foundSand = true;
                spdlog::info("Found sand at ({},{}) with velocity {:.3f}", x, y, velocityWithResistance);
                break;
            }
        }
    }
    
    // Now run with air resistance disabled.
    world->reset();
    world->setAirResistanceEnabled(false);
    
    world->addMaterialAtCell(2, 0, MaterialType::SAND, 1.0);
    worldB->at(2, 0).setVelocity(Vector2d(3.0, 0.0));  // Same initial velocity.
    
    // Run for same number of steps.
    for (int i = 0; i < 5; ++i) {
        world->advanceTime(0.016);
    }
    
    // Find sand and check velocity (it may have moved).
    double velocityWithoutResistance = 0.0;
    foundSand = false;
    for (uint32_t y = 0; y < 5 && !foundSand; ++y) {
        for (uint32_t x = 0; x < 5; ++x) {
            Cell& cell = worldB->at(x, y);
            if (!cell.isEmpty() && cell.getMaterialType() == MaterialType::SAND) {
                velocityWithoutResistance = cell.getVelocity().x;
                foundSand = true;
                spdlog::info("Found sand at ({},{}) with velocity {:.3f}", x, y, velocityWithoutResistance);
                break;
            }
        }
    }
    
    spdlog::info("Velocity with air resistance: {:.3f}", velocityWithResistance);
    spdlog::info("Velocity without air resistance: {:.3f}", velocityWithoutResistance);
    
    EXPECT_LT(velocityWithResistance, velocityWithoutResistance * 0.95)
        << "Velocity should be lower when air resistance is enabled";
}
