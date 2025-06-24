#include <gtest/gtest.h>
#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>

class PressureTransmissionTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        spdlog::set_level(spdlog::level::debug);
    }
};

// Simple test to verify pressure transmits to target cell, not source
TEST_F(PressureTransmissionTest, PressureGoesToTargetNotSource) {
    // Create 3x3 world using the test framework helper
    auto world = createWorldB(3, 3);
    world->setPressureSystem(WorldInterface::PressureSystem::TopDown);
    world->setDynamicPressureEnabled(true);
    world->setPressureScale(1.0);  // IMPORTANT: Framework sets this to 0.0 by default!
    world->setGravity(0.0); // No gravity for cleaner test
    
    // Setup: WATER tries to flow into nearly-full DIRT
    world->addMaterialAtCell(0, 1, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(1, 1, MaterialType::DIRT, 0.9); // 90% full
    
    CellB& water = world->at(0, 1);
    CellB& dirt = world->at(1, 1);
    
    // Give water rightward velocity
    water.setVelocity(Vector2d(5.0, 0.0));
    water.setCOM(Vector2d(0.9, 0.0)); // Near right edge
    
    spdlog::info("Starting simulation: water at ({},{}) with vel=({:.1f},{:.1f}), dirt at ({},{})",
                 0, 1, water.getVelocity().x, water.getVelocity().y, 1, 1);
    
    // Run simulation until collision
    for (int i = 0; i < 10; i++) {
        spdlog::debug("Step {}: water COM=({:.3f},{:.3f}), vel=({:.3f},{:.3f}), dirt pressure={:.6f}",
                     i, water.getCOM().x, water.getCOM().y, 
                     water.getVelocity().x, water.getVelocity().y, 
                     dirt.getDynamicPressure());
        
        world->advanceTime(0.016);
        
        if (dirt.getDynamicPressure() > 0.01) {
            spdlog::info("Pressure detected at step {}", i+1);
            break; // Pressure detected
        }
    }
    
    // Verify pressure went to TARGET (dirt), not SOURCE (water)
    EXPECT_GT(dirt.getDynamicPressure(), 0.1) << "Target cell should accumulate pressure";
    EXPECT_LT(water.getDynamicPressure(), 0.01) << "Source cell should have no pressure";
    
    // With the unified pressure system, we only track scalar pressure values
    // Direction information is no longer stored separately
}

// Test that walls eliminate pressure
TEST_F(PressureTransmissionTest, WallsEliminatePressure) {
    auto world = createWorldB(3, 3);
    world->setPressureSystem(WorldInterface::PressureSystem::TopDown);
    world->setDynamicPressureEnabled(true);
    world->setPressureScale(1.0);  // IMPORTANT: Framework sets this to 0.0 by default!
    world->setGravity(0.0);
    
    // Setup: WATER tries to flow into WALL
    world->addMaterialAtCell(0, 1, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(1, 1, MaterialType::WALL, 1.0);
    
    CellB& water = world->at(0, 1);
    CellB& wall = world->at(1, 1);
    
    // Give water rightward velocity
    water.setVelocity(Vector2d(5.0, 0.0));
    water.setCOM(Vector2d(0.9, 0.0));
    
    // Run simulation
    for (int i = 0; i < 10; i++) {
        world->advanceTime(0.016);
    }
    
    // Verify no pressure anywhere (walls eliminate it)
    EXPECT_LT(water.getDynamicPressure(), 0.01) << "Source should have no pressure when hitting wall";
    EXPECT_EQ(wall.getDynamicPressure(), 0.0) << "Walls cannot store pressure";
}

// Test material-specific resistance
TEST_F(PressureTransmissionTest, MaterialResistanceAffectsPressure) {
    auto world = createWorldB(3, 3);
    world->setPressureSystem(WorldInterface::PressureSystem::TopDown);
    world->setDynamicPressureEnabled(true);
    world->setPressureScale(1.0);  // IMPORTANT: Framework sets this to 0.0 by default!
    world->setGravity(0.0);
    
    // Test 1: WATER hitting WATER (weight = 0.8)
    world->addMaterialAtCell(0, 0, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(1, 0, MaterialType::WATER, 0.9);
    
    CellB& water1 = world->at(0, 0);
    CellB& water2 = world->at(1, 0);
    water1.setVelocity(Vector2d(5.0, 0.0));
    water1.setCOM(Vector2d(0.9, 0.0));
    
    // Test 2: WATER hitting DIRT (weight = 1.0)
    world->addMaterialAtCell(0, 2, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(1, 2, MaterialType::DIRT, 0.9);
    
    CellB& water3 = world->at(0, 2);
    CellB& dirt = world->at(1, 2);
    water3.setVelocity(Vector2d(5.0, 0.0));
    water3.setCOM(Vector2d(0.9, 0.0));
    
    // Run simulation
    for (int i = 0; i < 10; i++) {
        world->advanceTime(0.016);
        
        if (water2.getDynamicPressure() > 0.01 && dirt.getDynamicPressure() > 0.01) {
            break;
        }
    }
    
    // Verify DIRT accumulated more pressure due to higher weight
    double waterPressure = water2.getDynamicPressure();
    double dirtPressure = dirt.getDynamicPressure();
    
    EXPECT_GT(waterPressure, 0.1) << "Water-on-water should create pressure";
    EXPECT_GT(dirtPressure, 0.1) << "Water-on-dirt should create pressure";
    EXPECT_GT(dirtPressure, waterPressure * 1.2) << "DIRT (weight=1.0) should have more pressure than WATER (weight=0.8)";
}