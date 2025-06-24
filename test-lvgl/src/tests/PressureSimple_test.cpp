#include <gtest/gtest.h>
#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>

class PressureSimpleTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Create a 3x3 world for simple testing
        world = createWorldB(3, 3);
        
        // Enable BOTH pressure systems
        world->setDynamicPressureEnabled(true);
        world->setHydrostaticPressureEnabled(true);
        world->setPressureScale(10.0);  // Increase pressure scale for visible effects
        
        // Standard test settings
        world->setWallsEnabled(false);
        world->setAddParticlesEnabled(false);
        world->setGravity(9.81);
        
        spdlog::info("[TEST] Simple pressure test - both systems enabled, scale=10.0");
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(PressureSimpleTest, HydrostaticPressureDrivesMovement) {
    spdlog::info("[TEST] Testing if hydrostatic pressure alone can drive water movement");
    
    // Create a simple 2-cell water column
    world->addMaterialAtCell(1, 0, MaterialType::WATER, 1.0);  // Top
    world->addMaterialAtCell(1, 1, MaterialType::WATER, 1.0);  // Bottom
    
    logWorldStateAscii(world.get(), "Initial water column");
    
    // Run one timestep to calculate hydrostatic pressure
    world->advanceTime(0.016);
    
    // Check pressures
    double topPressure = world->at(1, 0).getHydrostaticPressure();
    double bottomPressure = world->at(1, 1).getHydrostaticPressure();
    
    spdlog::info("After first timestep:");
    spdlog::info("  Top pressure: {:.3f}", topPressure);
    spdlog::info("  Bottom pressure: {:.3f}", bottomPressure);
    
    EXPECT_LT(topPressure, bottomPressure) << "Bottom cell should have higher pressure";
    
    // Check if pressure creates forces
    Vector2d topVelocity = world->at(1, 0).getVelocity();
    Vector2d bottomVelocity = world->at(1, 1).getVelocity();
    
    spdlog::info("  Top velocity: ({:.3f}, {:.3f})", topVelocity.x, topVelocity.y);
    spdlog::info("  Bottom velocity: ({:.3f}, {:.3f})", bottomVelocity.x, bottomVelocity.y);
    
    // With pressure scale of 10, bottom water should have significant downward velocity
    EXPECT_GT(bottomVelocity.y, 1.0) << "Bottom water should have downward velocity from pressure";
}

TEST_F(PressureSimpleTest, WaterFlowsToEmptySpace) {
    spdlog::info("[TEST] Testing if water flows to adjacent empty space");
    
    // Create water next to empty space
    world->addMaterialAtCell(0, 1, MaterialType::WATER, 0.9);  // Almost full water
    // Cell (1,1) is empty
    
    // Give water a small rightward velocity
    world->at(0, 1).setVelocity(Vector2d(0.5, 0.0));
    world->at(0, 1).setCOM(Vector2d(0.5, 0.0));  // COM towards right edge
    
    logWorldStateAscii(world.get(), "Water next to empty space");
    
    // Track initial state
    double initialWaterLeft = world->at(0, 1).getFillRatio();
    double initialWaterRight = world->at(1, 1).getFillRatio();
    
    spdlog::info("Initial: left={:.3f}, right={:.3f}", initialWaterLeft, initialWaterRight);
    
    // Run simulation
    for (int i = 0; i < 10; i++) {
        world->advanceTime(0.016);
        
        double currentLeft = world->at(0, 1).getFillRatio();
        double currentRight = world->at(1, 1).getFillRatio();
        
        if (currentRight > 0.1) {
            spdlog::info("Water transferred at step {}: left={:.3f}, right={:.3f}", 
                        i, currentLeft, currentRight);
            break;
        }
    }
    
    // Check final state
    double finalWaterLeft = world->at(0, 1).getFillRatio();
    double finalWaterRight = world->at(1, 1).getFillRatio();
    
    logWorldStateAscii(world.get(), "Final state");
    
    spdlog::info("Final: left={:.3f}, right={:.3f}", finalWaterLeft, finalWaterRight);
    
    EXPECT_GT(finalWaterRight, 0.1) << "Some water should have moved to the right cell";
    EXPECT_LT(finalWaterLeft, initialWaterLeft) << "Left cell should have less water";
}

TEST_F(PressureSimpleTest, BlockedTransferCreatesDynamicPressure) {
    spdlog::info("[TEST] Testing if blocked transfers create dynamic pressure");
    
    // Create water trying to move into a wall
    world->addMaterialAtCell(1, 1, MaterialType::WATER, 0.8);
    world->addMaterialAtCell(2, 1, MaterialType::WALL, 1.0);
    
    // Give water strong rightward velocity toward wall
    world->at(1, 1).setVelocity(Vector2d(5.0, 0.0));
    world->at(1, 1).setCOM(Vector2d(0.8, 0.0));  // COM near right edge
    
    logWorldStateAscii(world.get(), "Water moving toward wall");
    
    double initialPressure = world->at(1, 1).getDynamicPressure();
    spdlog::info("Initial dynamic pressure: {:.3f}", initialPressure);
    
    // Run simulation - water should hit wall and build pressure
    for (int i = 0; i < 5; i++) {
        world->advanceTime(0.016);
        
        double currentPressure = world->at(1, 1).getDynamicPressure();
        Vector2d currentVelocity = world->at(1, 1).getVelocity();
        
        spdlog::info("Step {}: pressure={:.3f}, velocity=({:.3f},{:.3f})", 
                    i, currentPressure, currentVelocity.x, currentVelocity.y);
    }
    
    logWorldStateAscii(world.get(), "After collision with wall");
    
    // Check if any water cell has debug pressure (water may have moved)
    bool pressureFound = false;
    double maxPressure = 0.0;
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            if (world->at(x, y).getMaterialType() == MaterialType::WATER) {
                double debugPressure = world->at(x, y).getDebugDynamicPressure();
                maxPressure = std::max(maxPressure, debugPressure);
                if (debugPressure > 0.0) {
                    pressureFound = true;
                    spdlog::info("Water at ({},{}) has debug pressure: {:.3f}", x, y, debugPressure);
                }
            }
        }
    }
    
    EXPECT_TRUE(pressureFound) << "At least one water cell should have debug pressure from blocked transfer";
    EXPECT_GT(maxPressure, 0.1) << "Maximum debug pressure should be significant";
}