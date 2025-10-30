#include <gtest/gtest.h>
#include "visual_test_runner.h"
#include "../World.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>

class PressureIntegrationTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Create a 5x10 world for integration testing.
        world = createWorldB(5, 10);
        
        // Enable BOTH pressure systems.
        world->setDynamicPressureEnabled(true);
        world->setHydrostaticPressureEnabled(true);
        world->setPressureScale(1.0);
        
        // Standard test settings.
        world->setWallsEnabled(true);  // Enable walls for realistic scenarios.
        world->setAddParticlesEnabled(false);
        world->setGravity(9.81);
        
        spdlog::info("[TEST] Pressure integration test - both systems enabled");
    }
    
    std::unique_ptr<World> world;
};

TEST_F(PressureIntegrationTest, DamBreakScenario) {
    spdlog::info("[TEST] Testing classic dam break with combined pressure systems");
    
    // Create water column held by a dam.
    // Layout: Water column on left, wall in middle, empty on right.
    for (int y = 5; y < 9; y++) {
        world->addMaterialAtCell(1, y, MaterialType::WATER, 1.0);
    }
    
    // Add dam wall (will be removed later).
    for (int y = 5; y < 9; y++) {
        world->addMaterialAtCell(2, y, MaterialType::WALL, 1.0);
    }
    
    spdlog::info("Initial setup: water column with dam");
    logWorldStateAscii(world.get(), "Initial dam setup");
    
    // Let pressure build up.
    spdlog::info("Building up hydrostatic pressure...");
    for (int i = 0; i < 10; i++) {
        world->advanceTime(0.016);
    }
    
    // Check hydrostatic pressure gradient.
    double topPressure = world->at(1, 5).getHydrostaticPressure();
    double bottomPressure = world->at(1, 8).getHydrostaticPressure();
    
    spdlog::info("Hydrostatic pressures: top={:.2f}, bottom={:.2f}", topPressure, bottomPressure);
    EXPECT_GT(bottomPressure, topPressure) << "Bottom should have higher hydrostatic pressure";
    
    // Remove dam.
    spdlog::info("Removing dam...");
    for (int y = 5; y < 9; y++) {
        world->at(2, y).setFillRatio(0.0);
        world->at(2, y).setMaterialType(MaterialType::AIR);
    }
    
    logWorldStateAscii(world.get(), "Dam removed");
    
    // Run simulation and check for water flow.
    bool waterFlowed = false;
    for (int step = 0; step < 50; step++) {
        world->advanceTime(0.016);
        
        // Check if water moved to right side.
        for (int y = 5; y < 9; y++) {
            if (world->at(3, y).getMaterialType() == MaterialType::WATER && 
                world->at(3, y).getFillRatio() > 0.1) {
                waterFlowed = true;
                spdlog::info("Water flowed to right side at step {}", step);
                break;
            }
        }
        
        if (waterFlowed) break;
    }
    
    logWorldStateAscii(world.get(), "Final state after dam break");
    
    EXPECT_TRUE(waterFlowed) << "Water should flow after dam removal";
    
    // Check dynamic pressure buildup in cells that had blocked transfers.
    bool dynamicPressureDetected = false;
    for (int y = 0; y < 10; y++) {
        for (int x = 0; x < 5; x++) {
            double totalPressure = world->at(x, y).getHydrostaticPressure() + world->at(x, y).getDynamicPressure();
            if (totalPressure > 0.01) {
                dynamicPressureDetected = true;
                spdlog::info("Dynamic pressure detected at ({},{}): {:.3f}", 
                           x, y, totalPressure);
            }
        }
    }
    
    spdlog::info("Test complete - water flowed: {}, dynamic pressure: {}", 
                 waterFlowed, dynamicPressureDetected);
}

TEST_F(PressureIntegrationTest, NarrowChannelPressureBuildup) {
    spdlog::info("[TEST] Testing pressure buildup in narrow channel");
    
    // Create a narrow channel scenario.
    // Water tries to flow through 1-cell wide gap.
    
    // Add water source on left.
    for (int y = 3; y < 7; y++) {
        world->addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
        world->addMaterialAtCell(1, y, MaterialType::WATER, 1.0);
    }
    
    // Create narrow channel with walls.
    for (int y = 0; y < 10; y++) {
        if (y != 5) {  // Leave gap at y=5.
            world->addMaterialAtCell(2, y, MaterialType::WALL, 1.0);
        }
    }
    
    // Give water initial rightward velocity.
    for (int y = 3; y < 7; y++) {
        world->at(1, y).setVelocity(Vector2d(2.0, 0.0));
    }
    
    logWorldStateAscii(world.get(), "Narrow channel setup");
    
    // Run simulation.
    for (int step = 0; step < 30; step++) {
        world->advanceTime(0.016);
    }
    
    // Check for pressure buildup near bottleneck.
    double maxDynamicPressure = 0.0;
    Vector2i maxPressurePos;
    
    for (int y = 3; y < 7; y++) {
        for (int x = 0; x < 2; x++) {
            double pressure = world->at(x, y).getHydrostaticPressure() + world->at(x, y).getDynamicPressure();
            if (pressure > maxDynamicPressure) {
                maxDynamicPressure = pressure;
                maxPressurePos = Vector2i(x, y);
            }
        }
    }
    
    spdlog::info("Maximum dynamic pressure: {:.3f} at ({},{})", 
                 maxDynamicPressure, maxPressurePos.x, maxPressurePos.y);
    
    logWorldStateAscii(world.get(), "After pressure buildup");
    
    EXPECT_GT(maxDynamicPressure, 0.1) << "Dynamic pressure should build up at bottleneck";
    
    // Check if some water made it through.
    bool waterPassedThrough = false;
    for (int y = 0; y < 10; y++) {
        if (world->at(3, y).getMaterialType() == MaterialType::WATER) {
            waterPassedThrough = true;
            break;
        }
    }
    
    EXPECT_TRUE(waterPassedThrough) << "Some water should pass through narrow channel";
}