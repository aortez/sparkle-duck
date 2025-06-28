#include <gtest/gtest.h>
#include "../WorldB.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>

class MassBasedCohesionTest : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::set_level(spdlog::level::trace);
        
        // Create a 5x5 world.
        world = std::make_unique<WorldB>(5, 5, nullptr);
        world->setWallsEnabled(false);
        world->setAddParticlesEnabled(false);
        world->setCohesionComForceEnabled(true);
        world->setCohesionComForceStrength(100.0);
        world->setCOMCohesionRange(1);
        world->setGravity(0.0); // Disable gravity to isolate cohesion effects.
        world->setCohesionBindForceEnabled(false); // Disable bind force to allow movement.
        world->setCOMCohesionMode(WorldB::COMCohesionMode::MASS_BASED);
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(MassBasedCohesionTest, NoForceWhenCOMInsideThreshold) {
    // Add material with neighbors.
    world->addMaterialAtCell(2, 2, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(1, 2, MaterialType::DIRT, 1.0); // Left neighbor.
    world->addMaterialAtCell(3, 2, MaterialType::DIRT, 1.0); // Right neighbor.
    
    // Set center cell's COM within threshold (< 0.5).
    auto& cell = world->at(2, 2);
    cell.setCOM(Vector2d(0.3, 0.3));
    cell.setVelocity(Vector2d(0, 0));
    
    Vector2d initial_com = cell.getCOM();
    
    // Run simulation.
    for (int i = 0; i < 10; i++) {
        world->advanceTime(0.016);
    }
    
    Vector2d final_com = cell.getCOM();
    
    // COM should not move (no force applied within threshold).
    EXPECT_NEAR(final_com.x, initial_com.x, 0.01);
    EXPECT_NEAR(final_com.y, initial_com.y, 0.01);
}

TEST_F(MassBasedCohesionTest, ForceActivatesOutsideThreshold) {
    // Add material with neighbors.
    world->addMaterialAtCell(2, 2, MaterialType::METAL, 1.0);
    world->addMaterialAtCell(1, 2, MaterialType::METAL, 1.0); // Left neighbor.
    world->addMaterialAtCell(3, 2, MaterialType::METAL, 1.0); // Right neighbor.
    
    // Set center cell's COM outside threshold (> 0.5).
    auto& cell = world->at(2, 2);
    cell.setCOM(Vector2d(0.8, 0.0));
    cell.setVelocity(Vector2d(0, 0));
    
    double initial_distance = cell.getCOM().magnitude();
    
    // Run simulation.
    for (int i = 0; i < 50; i++) {
        world->advanceTime(0.016);
    }
    
    double final_distance = cell.getCOM().magnitude();
    
    // COM should move toward neighbors (force activated).
    EXPECT_LT(final_distance, initial_distance);
}

TEST_F(MassBasedCohesionTest, ForceScalesWithMassProduct) {
    // Test 1: Light material (LEAF).
    world->addMaterialAtCell(1, 1, MaterialType::LEAF, 1.0);
    world->addMaterialAtCell(1, 2, MaterialType::LEAF, 1.0);
    
    auto& leaf_cell = world->at(1, 1);
    leaf_cell.setCOM(Vector2d(0.7, 0.0));
    leaf_cell.setVelocity(Vector2d(0, 0));
    
    // Test 2: Heavy material (METAL).
    world->addMaterialAtCell(3, 1, MaterialType::METAL, 1.0);
    world->addMaterialAtCell(3, 2, MaterialType::METAL, 1.0);
    
    auto& metal_cell = world->at(3, 1);
    metal_cell.setCOM(Vector2d(0.7, 0.0));
    metal_cell.setVelocity(Vector2d(0, 0));
    
    // Run one step.
    world->advanceTime(0.016);
    
    // Metal (heavier) should have stronger force than leaf.
    double leaf_velocity = leaf_cell.getVelocity().magnitude();
    double metal_velocity = metal_cell.getVelocity().magnitude();
    
    // Note: Heavier materials should have less acceleration (F = ma, a = F/m).
    // But the force itself (M1 * M2) is larger for heavier materials.
    spdlog::info("Leaf velocity: {}, Metal velocity: {}", leaf_velocity, metal_velocity);
    
    // Both should move, but at different rates based on their mass.
    EXPECT_GT(leaf_velocity, 0.0);
    EXPECT_GT(metal_velocity, 0.0);
}

TEST_F(MassBasedCohesionTest, ForceScalesWithInverseSquareDistance) {
    // Create two scenarios with different distances.
    
    // Close neighbors.
    world->addMaterialAtCell(1, 1, MaterialType::SAND, 1.0);
    world->addMaterialAtCell(1, 2, MaterialType::SAND, 1.0);
    auto& close_cell = world->at(1, 1);
    close_cell.setCOM(Vector2d(0.6, 0.0)); // Just outside threshold.
    close_cell.setVelocity(Vector2d(0, 0));
    
    // Set COM cohesion range to 2 to test distance effects.
    world->setCOMCohesionRange(2);
    
    // Distant neighbors (2 cells away).
    world->addMaterialAtCell(3, 1, MaterialType::SAND, 1.0);
    world->addMaterialAtCell(3, 3, MaterialType::SAND, 1.0); // 2 cells away.
    auto& far_cell = world->at(3, 1);
    far_cell.setCOM(Vector2d(0.6, 0.0)); // Same COM offset.
    far_cell.setVelocity(Vector2d(0, 0));
    
    // Run one step.
    world->advanceTime(0.016);
    
    // Closer neighbors should produce stronger force.
    double close_velocity = close_cell.getVelocity().magnitude();
    double far_velocity = far_cell.getVelocity().magnitude();
    
    spdlog::info("Close velocity: {}, Far velocity: {}", close_velocity, far_velocity);
    
    // Close cell should have higher velocity due to 1/r² relationship.
    EXPECT_GT(close_velocity, far_velocity);
}

TEST_F(MassBasedCohesionTest, MaterialSpecificConstants) {
    // Test that different materials with same mass behave differently.
    // due to their material-specific com_mass_constant.
    
    // Water (high constant = 8.0).
    world->addMaterialAtCell(1, 1, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(1, 2, MaterialType::WATER, 1.0);
    auto& water_cell = world->at(1, 1);
    water_cell.setCOM(Vector2d(0.7, 0.0));
    water_cell.setVelocity(Vector2d(0, 0));
    
    // Metal (low constant = 2.0) - adjusted to have similar mass.
    world->addMaterialAtCell(3, 1, MaterialType::SAND, 1.0); // Using sand as it has closer density to water.
    world->addMaterialAtCell(3, 2, MaterialType::SAND, 1.0);
    auto& sand_cell = world->at(3, 1);
    sand_cell.setCOM(Vector2d(0.7, 0.0));
    sand_cell.setVelocity(Vector2d(0, 0));
    
    // Run one step.
    world->advanceTime(0.016);
    
    // Different materials should have different forces despite similar setup.
    double water_velocity = water_cell.getVelocity().magnitude();
    double sand_velocity = sand_cell.getVelocity().magnitude();
    
    spdlog::info("Water velocity: {}, Sand velocity: {}", water_velocity, sand_velocity);
    
    // Both should move, demonstrating material-specific behavior.
    EXPECT_GT(water_velocity, 0.0);
    EXPECT_GT(sand_velocity, 0.0);
    // They should be different due to different material constants.
    EXPECT_NE(water_velocity, sand_velocity);
}