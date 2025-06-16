#include <gtest/gtest.h>
#include <cmath>
#include "../WorldB.h"
#include "../MaterialType.h"

class ForceInfluencedMovementTest : public ::testing::Test {
protected:
    void SetUp() override {
        world = std::make_unique<WorldB>(10, 10, nullptr);
        world->setWallsEnabled(false);
        world->reset();
    }
    
    // Helper: Run simulation for multiple timesteps and check if material moved
    bool materialMovedAfterSteps(uint32_t x, uint32_t y, MaterialType expectedType, int timesteps = 50) {
        Vector2d initialCOM = world->at(x, y).getCOM();
        
        for (int i = 0; i < timesteps; i++) {
            world->advanceTime(0.016); // Full physics simulation
        }
        
        // Check if material is still at original position with same amount
        const CellB& cell = world->at(x, y);
        bool stillPresent = (cell.getMaterialType() == expectedType && 
                           cell.getFillRatio() > 0.5);
        
        if (stillPresent) {
            Vector2d finalCOM = cell.getCOM();
            double comChange = (finalCOM - initialCOM).mag();
            return comChange > 0.1; // Significant COM movement
        }
        
        return true; // Material transferred to different cell = movement occurred
    }
    
    // Helper: Check if materials stay connected (no movement between them)
    bool materialsStayConnected(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, 
                               MaterialType expectedType, int timesteps = 50) {
        for (int i = 0; i < timesteps; i++) {
            world->advanceTime(0.016);
        }
        
        // Both cells should still contain the material
        const CellB& cell1 = world->at(x1, y1);
        const CellB& cell2 = world->at(x2, y2);
        
        return (cell1.getMaterialType() == expectedType && cell1.getFillRatio() > 0.5) &&
               (cell2.getMaterialType() == expectedType && cell2.getFillRatio() > 0.5);
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(ForceInfluencedMovementTest, IsolatedWaterMovesFreely) {
    // Isolated water should accumulate velocity and eventually move due to low cohesion
    world->addMaterialAtCell(5, 5, MaterialType::WATER, 1.0);
    
    bool moved = materialMovedAfterSteps(5, 5, MaterialType::WATER, 50);
    EXPECT_TRUE(moved) << "Isolated water should move after accumulating velocity from gravity";
}


TEST_F(ForceInfluencedMovementTest, DirtClusterShowsCohesion) {
    // Create a dirt cluster - center should resist breaking away due to moderate cohesion (0.4)
    world->addMaterialAtCell(5, 5, MaterialType::DIRT, 1.0); // Center
    world->addMaterialAtCell(5, 4, MaterialType::DIRT, 1.0); // Above
    world->addMaterialAtCell(4, 5, MaterialType::DIRT, 1.0); // Left
    world->addMaterialAtCell(6, 5, MaterialType::DIRT, 1.0); // Right
    
    // All dirt pieces should stay relatively close due to cohesion
    bool centerPresent = (world->at(5, 5).getMaterialType() == MaterialType::DIRT);
    bool clustered = materialsStayConnected(5, 5, 5, 4, MaterialType::DIRT, 30) &&
                    materialsStayConnected(5, 5, 4, 5, MaterialType::DIRT, 30);
    
    EXPECT_TRUE(centerPresent && clustered) << "Dirt cluster should show cohesive behavior";
}

TEST_F(ForceInfluencedMovementTest, IsolatedDirtMovesFreely) {
    // Isolated dirt should move freely (no cohesion resistance)
    world->addMaterialAtCell(5, 5, MaterialType::DIRT, 1.0);
    
    bool moved = materialMovedAfterSteps(5, 5, MaterialType::DIRT, 50);
    EXPECT_TRUE(moved) << "Isolated dirt should move freely (no cohesion resistance)";
}

TEST_F(ForceInfluencedMovementTest, MaterialPropertyDifferences) {
    // Test that different materials behave differently due to their cohesion properties
    
    // Place isolated samples of each material - isolated means no cohesion resistance
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);  // Low cohesion (0.1)
    world->addMaterialAtCell(4, 2, MaterialType::DIRT, 1.0);   // Medium cohesion (0.4)  
    world->addMaterialAtCell(6, 2, MaterialType::METAL, 1.0);  // High cohesion (0.9)
    
    // All isolated materials should move since they have no cohesion resistance
    bool waterMoved = materialMovedAfterSteps(2, 2, MaterialType::WATER, 50);
    bool dirtMoved = materialMovedAfterSteps(4, 2, MaterialType::DIRT, 50);
    bool metalMoved = materialMovedAfterSteps(6, 2, MaterialType::METAL, 50);
    
    EXPECT_TRUE(waterMoved) << "Isolated water should move";
    EXPECT_TRUE(dirtMoved) << "Isolated dirt should move";
    EXPECT_TRUE(metalMoved) << "Isolated metal should move";
    
    // The key difference is HOW they behave when connected to neighbors
    // That's tested in the other test cases
}

TEST_F(ForceInfluencedMovementTest, HighlyConnectedMetalStaysFixed) {
    // Create a 3x3 metal block - center piece should be completely immobilized
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            world->addMaterialAtCell(5 + dx, 5 + dy, MaterialType::METAL, 1.0);
        }
    }
    
    // Center piece with 8 metal neighbors should have very high cohesion resistance
    // Resistance = 0.9 * 8 * 1.0 = 7.2, much higher than gravity force ≈ 0.236
    
    Vector2d initialCOM = world->at(5, 5).getCOM();
    
    // Run simulation - center should stay essentially fixed
    for (int i = 0; i < 100; i++) {
        world->advanceTime(0.016);
    }
    
    Vector2d finalCOM = world->at(5, 5).getCOM();
    double comMovement = (finalCOM - initialCOM).mag();
    
    // Center should stay in the same cell with minimal COM movement
    const CellB& centerCell = world->at(5, 5);
    bool stillMetal = (centerCell.getMaterialType() == MaterialType::METAL);
    bool minimalMovement = (comMovement < 0.1);
    
    EXPECT_TRUE(stillMetal && minimalMovement) << "Highly connected metal center should stay essentially fixed";
}