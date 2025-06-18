#include <gtest/gtest.h>
#include <cmath>
#include "../WorldB.h"
#include "../WorldCohesionCalculator.h"
#include "../MaterialType.h"

class ForceDebugTest : public ::testing::Test {
protected:
    void SetUp() override {
        world = std::make_unique<WorldB>(5, 5, nullptr);
        world->setWallsEnabled(false);
        world->reset();
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(ForceDebugTest, DebugWaterForces) {
    // Place isolated water and examine forces in detail
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    
    // Set initial velocity
    CellB& waterCell = world->at(2, 2);
    waterCell.setVelocity(Vector2d(0.0, 0.5));
    
    // Calculate forces directly
    auto cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(2, 2);
    auto adhesion = world->calculateAdhesionForce(2, 2);
    
    std::cout << "=== WATER FORCE DEBUG ===" << std::endl;
    std::cout << "Cohesion resistance: " << cohesion.resistance_magnitude << std::endl;
    std::cout << "Cohesion neighbors: " << cohesion.connected_neighbors << std::endl;
    std::cout << "Adhesion magnitude: " << adhesion.force_magnitude << std::endl;
    std::cout << "Adhesion direction: (" << adhesion.force_direction.x << ", " << adhesion.force_direction.y << ")" << std::endl;
    
    // Calculate driving forces manually (matching implementation)
    double gravity = 9.81;
    double deltaTime = 0.016;
    double density = getMaterialDensity(MaterialType::WATER); // Should be 1.0
    
    Vector2d gravity_force(0.0, gravity * deltaTime * density);
    Vector2d net_driving_force = gravity_force + adhesion.force_direction * adhesion.force_magnitude;
    
    std::cout << "Gravity force: (" << gravity_force.x << ", " << gravity_force.y << ")" << std::endl;
    std::cout << "Net driving force: (" << net_driving_force.x << ", " << net_driving_force.y << ")" << std::endl;
    std::cout << "Driving magnitude: " << net_driving_force.mag() << std::endl;
    
    std::cout << "Movement check: driving(" << net_driving_force.mag() << ") > resistance(" << cohesion.resistance_magnitude << ") = " 
              << (net_driving_force.mag() > cohesion.resistance_magnitude ? "MOVE" : "BLOCKED") << std::endl;
    
    // Now test actual movement queuing
    Vector2d velocityBefore = waterCell.getVelocity();
    std::cout << "Velocity before: (" << velocityBefore.x << ", " << velocityBefore.y << ")" << std::endl;
    
    world->clearPendingMoves();
    world->queueMaterialMovesForTesting(deltaTime);
    
    Vector2d velocityAfter = waterCell.getVelocity();
    std::cout << "Velocity after: (" << velocityAfter.x << ", " << velocityAfter.y << ")" << std::endl;
    
    const auto& moves = world->getPendingMoves();
    std::cout << "Generated moves: " << moves.size() << std::endl;
    
    // This test should help us understand what's happening
    EXPECT_EQ(cohesion.connected_neighbors, 0) << "Isolated water should have no cohesion neighbors";
    EXPECT_EQ(cohesion.resistance_magnitude, 0.0) << "Isolated water should have no cohesion resistance";
    EXPECT_GT(net_driving_force.mag(), 0.0) << "Should have non-zero driving force from gravity";
}