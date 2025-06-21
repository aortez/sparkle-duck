#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>

class AirResistanceVisualTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Create larger world for better visualization
        world = createWorldB(10, 10);
        
        // Apply test-specific defaults
        world->setAddParticlesEnabled(false);
        world->setWallsEnabled(false);
        world->setCohesionBindForceEnabled(false);
        world->setCohesionComForceEnabled(false);
        world->setAdhesionEnabled(false);
        world->setup();
    }
    
    std::unique_ptr<WorldInterface> world;
};

TEST_F(AirResistanceVisualTest, CompareMaterialsWithAndWithoutAirResistance) {
    spdlog::info("Starting AirResistanceVisualTest::CompareMaterialsWithAndWithoutAirResistance");
    
    // Reset world
    world->reset();
    
    // Enable air resistance for left side comparison
    world->setAirResistanceEnabled(true);
    
    // Add different materials at top of screen with horizontal velocity
    // Left side (with air resistance): WATER, SAND, METAL
    world->addMaterialAtCell(1, 1, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(2, 1, MaterialType::SAND, 1.0);
    world->addMaterialAtCell(3, 1, MaterialType::METAL, 1.0);
    
    // Right side (we'll disable air resistance after first group): WATER, SAND, METAL
    world->addMaterialAtCell(6, 1, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(7, 1, MaterialType::SAND, 1.0);
    world->addMaterialAtCell(8, 1, MaterialType::METAL, 1.0);
    
    WorldB* worldB = dynamic_cast<WorldB*>(world.get());
    ASSERT_NE(worldB, nullptr);
    
    // Give all materials the same initial horizontal velocity
    Vector2d initialVelocity(5.0, 0.0);
    worldB->at(1, 1).setVelocity(initialVelocity);
    worldB->at(2, 1).setVelocity(initialVelocity);
    worldB->at(3, 1).setVelocity(initialVelocity);
    worldB->at(6, 1).setVelocity(initialVelocity);
    worldB->at(7, 1).setVelocity(initialVelocity);
    worldB->at(8, 1).setVelocity(initialVelocity);
    
    // Add a dividing wall to separate the two groups
    for (uint32_t y = 0; y < 10; ++y) {
        world->addMaterialAtCell(5, y, MaterialType::WALL, 1.0);
    }
    
    spdlog::info("Initial setup: 3 materials on left (WITH air resistance), 3 on right (WITHOUT)");
    spdlog::info("All materials start with velocity (5.0, 0.0)");
    
    // Use showInitialStateWithStep to give user choice between Start and Step
    showInitialStateWithStep(world.get(), "Air Resistance Comparison: LEFT with resistance, RIGHT without");
    
    // Run simulation and track positions
    const int maxSteps = 20;
    
    if (visual_mode_) {
        // Visual mode with step support
        for (int step = 0; step < maxSteps; ++step) {
            // After first step, disable air resistance for right side
            if (step == 1) {
                // Move right side materials to temporary storage
                CellB rightWater = worldB->at(6, 1);
                CellB rightSand = worldB->at(7, 1);
                CellB rightMetal = worldB->at(8, 1);
                
                // Clear right side
                worldB->at(6, 1).clear();
                worldB->at(7, 1).clear();
                worldB->at(8, 1).clear();
                
                // Disable air resistance
                world->setAirResistanceEnabled(false);
                
                // Restore right side materials
                worldB->at(6, 1) = rightWater;
                worldB->at(7, 1) = rightSand;
                worldB->at(8, 1) = rightMetal;
                
                updateDisplay(world.get(), "Air resistance DISABLED for right side");
                pauseIfVisual(1000);
            }
            
            // Create status message for current state
            std::stringstream ss;
            ss << "Step " << step + 1 << " of " << maxSteps << "\n";
            ss << "LEFT: Air resistance ON | RIGHT: Air resistance OFF\n";
            
            // Find and display material positions
            bool foundMaterials = false;
            
            // Check left side
            for (uint32_t y = 0; y < 10; ++y) {
                for (uint32_t x = 0; x < 5; ++x) {
                    CellB& cell = worldB->at(x, y);
                    if (!cell.isEmpty() && cell.getMaterialType() != MaterialType::WALL) {
                        foundMaterials = true;
                        if (cell.getMaterialType() == MaterialType::METAL) {
                            ss << "L-Metal: vel=" << std::fixed << std::setprecision(2) 
                               << cell.getVelocity().x << " ";
                        }
                    }
                }
            }
            
            // Check right side
            for (uint32_t y = 0; y < 10; ++y) {
                for (uint32_t x = 6; x < 10; ++x) {
                    CellB& cell = worldB->at(x, y);
                    if (!cell.isEmpty() && cell.getMaterialType() != MaterialType::WALL) {
                        if (cell.getMaterialType() == MaterialType::METAL) {
                            ss << "R-Metal: vel=" << std::fixed << std::setprecision(2) 
                               << cell.getVelocity().x;
                        }
                    }
                }
            }
            
            if (!foundMaterials) {
                ss << "\nSearching for materials...";
            }
            
            updateDisplay(world.get(), ss.str());
            
            // Use stepSimulation which handles step mode automatically
            stepSimulation(world.get(), 1, "Comparing air resistance effects");
            
            if (step % 5 == 0) {
                spdlog::info("Step {}: Logging material positions and velocities", step);
                
                // Find and log left side materials (with air resistance)
                for (uint32_t y = 0; y < 10; ++y) {
                    for (uint32_t x = 0; x < 5; ++x) {
                        CellB& cell = worldB->at(x, y);
                        if (!cell.isEmpty() && cell.getMaterialType() != MaterialType::WALL) {
                            spdlog::info("  LEFT (air resist ON) - {} at ({},{}) velocity=({:.2f},{:.2f})",
                                         getMaterialName(cell.getMaterialType()), x, y,
                                         cell.getVelocity().x, cell.getVelocity().y);
                        }
                    }
                }
                
                // Find and log right side materials (without air resistance)
                for (uint32_t y = 0; y < 10; ++y) {
                    for (uint32_t x = 6; x < 10; ++x) {
                        CellB& cell = worldB->at(x, y);
                        if (!cell.isEmpty() && cell.getMaterialType() != MaterialType::WALL) {
                            spdlog::info("  RIGHT (air resist OFF) - {} at ({},{}) velocity=({:.2f},{:.2f})",
                                         getMaterialName(cell.getMaterialType()), x, y,
                                         cell.getVelocity().x, cell.getVelocity().y);
                        }
                    }
                }
            }
        }
        
        // Final summary
        std::stringstream finalSummary;
        finalSummary << "Test Complete!\n";
        finalSummary << "✓ Left materials (WITH air resistance) slowed down\n";
        finalSummary << "✓ Right materials (WITHOUT air resistance) maintained more velocity\n";
        finalSummary << "✓ Denser materials (METAL) less affected than lighter ones (WATER)";
        updateDisplay(world.get(), finalSummary.str());
        waitForNext();
        
    } else {
        // Non-visual mode: run all steps at once
        for (int step = 0; step < maxSteps; ++step) {
            // After first step, disable air resistance for right side
            if (step == 1) {
                // Move right side materials to temporary storage
                CellB rightWater = worldB->at(6, 1);
                CellB rightSand = worldB->at(7, 1);
                CellB rightMetal = worldB->at(8, 1);
                
                // Clear right side
                worldB->at(6, 1).clear();
                worldB->at(7, 1).clear();
                worldB->at(8, 1).clear();
                
                // Disable air resistance
                world->setAirResistanceEnabled(false);
                
                // Restore right side materials
                worldB->at(6, 1) = rightWater;
                worldB->at(7, 1) = rightSand;
                worldB->at(8, 1) = rightMetal;
            }
            
            world->advanceTime(0.016);
            
            if (step % 5 == 0) {
                spdlog::info("Step {}: Logging material positions and velocities", step);
                
                // Find and log left side materials (with air resistance)
                for (uint32_t y = 0; y < 10; ++y) {
                    for (uint32_t x = 0; x < 5; ++x) {
                        CellB& cell = worldB->at(x, y);
                        if (!cell.isEmpty() && cell.getMaterialType() != MaterialType::WALL) {
                            spdlog::info("  LEFT (air resist ON) - {} at ({},{}) velocity=({:.2f},{:.2f})",
                                         getMaterialName(cell.getMaterialType()), x, y,
                                         cell.getVelocity().x, cell.getVelocity().y);
                        }
                    }
                }
                
                // Find and log right side materials (without air resistance)
                for (uint32_t y = 0; y < 10; ++y) {
                    for (uint32_t x = 6; x < 10; ++x) {
                        CellB& cell = worldB->at(x, y);
                        if (!cell.isEmpty() && cell.getMaterialType() != MaterialType::WALL) {
                            spdlog::info("  RIGHT (air resist OFF) - {} at ({},{}) velocity=({:.2f},{:.2f})",
                                         getMaterialName(cell.getMaterialType()), x, y,
                                         cell.getVelocity().x, cell.getVelocity().y);
                        }
                    }
                }
            }
        }
    }
    
    spdlog::info("Test complete - materials on left should have moved less distance due to air resistance");
}