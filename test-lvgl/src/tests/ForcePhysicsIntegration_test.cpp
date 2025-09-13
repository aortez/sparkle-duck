#include <gtest/gtest.h>
#include <cmath>
#include "../WorldB.h"
#include "../MaterialType.h"
#include "../WorldBCohesionCalculator.h"

class ForcePhysicsIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        world = std::make_unique<WorldB>(5, 5, nullptr);
        world->setWallsEnabled(false);
        world->reset();
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(ForcePhysicsIntegrationTest, GravityBuildsVelocityOverTime) {
    // Place isolated water - should accumulate velocity from gravity over multiple timesteps.
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    
    CellB& waterCell = world->at(2, 2);
    
    std::cout << "=== GRAVITY ACCUMULATION TEST ===" << std::endl;
    
    // Run multiple physics timesteps to build up velocity.
    double deltaTime = 0.016;
    for (int i = 0; i < 10; i++) {
        Vector2d velocityBefore = waterCell.getVelocity();
        
        // Apply gravity (this should add to velocity each timestep).
        world->advanceTime(deltaTime);
        
        Vector2d velocityAfter = waterCell.getVelocity();
        
        std::cout << "Timestep " << i << ": velocity (" << velocityAfter.x << ", " << velocityAfter.y 
                  << ") delta: (" << (velocityAfter.x - velocityBefore.x) << ", " << (velocityAfter.y - velocityBefore.y) << ")" << std::endl;
        
        // Check if moves were generated in this timestep.
        world->clearPendingMoves();
        auto moves = world->computeMaterialMoves(deltaTime);
        
        if (!moves.empty()) {
            std::cout << "  -> Generated " << moves.size() << " moves!" << std::endl;
            break;
        }
    }
    
    Vector2d finalVelocity = waterCell.getVelocity();
    std::cout << "Final velocity: (" << finalVelocity.x << ", " << finalVelocity.y << ")" << std::endl;
    std::cout << "Final velocity magnitude: " << finalVelocity.mag() << std::endl;
    
    // Water should accumulate downward velocity from gravity.
    EXPECT_GT(finalVelocity.y, 0.0) << "Water should have accumulated downward velocity from gravity";
}

TEST_F(ForcePhysicsIntegrationTest, ManualHighVelocityTriggersCrossing) {
    // Test with manually set high velocity to verify boundary crossing logic works.
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    
    CellB& waterCell = world->at(2, 2);
    
    // Set high velocity that should definitely trigger boundary crossing.
    Vector2d highVelocity(0.0, 100.0); // Very high downward velocity.
    waterCell.setVelocity(highVelocity);
    
    std::cout << "=== HIGH VELOCITY TEST ===" << std::endl;
    std::cout << "Set velocity: (" << highVelocity.x << ", " << highVelocity.y << ")" << std::endl;
    
    // Calculate expected COM change.
    double deltaTime = 0.016;
    Vector2d expectedCOMChange = highVelocity * deltaTime;
    std::cout << "Expected COM change: (" << expectedCOMChange.x << ", " << expectedCOMChange.y << ")" << std::endl;
    std::cout << "Should trigger boundary crossing: " << (expectedCOMChange.mag() > 1.0 ? "YES" : "NO") << std::endl;
    
    // Test force threshold (should pass for isolated water).
    auto cohesion = WorldBCohesionCalculator(*world).calculateCohesionForce(2, 2);
    auto adhesion = world->getAdhesionCalculator().calculateAdhesionForce(2, 2);
    Vector2d gravity_force(0.0, 9.81 * deltaTime * 1.0); // gravity * deltaTime * density.
    Vector2d net_driving_force = gravity_force + adhesion.force_direction * adhesion.force_magnitude;
    
    std::cout << "Force check: driving(" << net_driving_force.mag() << ") > resistance(" << cohesion.resistance_magnitude << ") = " 
              << (net_driving_force.mag() > cohesion.resistance_magnitude ? "MOVE" : "BLOCKED") << std::endl;
    
    // Queue moves.
    world->clearPendingMoves();
    auto moves = world->computeMaterialMoves(deltaTime);
    std::cout << "Generated moves: " << moves.size() << std::endl;
    
    EXPECT_GT(moves.size(), 0) << "High velocity should trigger boundary crossing and generate moves";
}