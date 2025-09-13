#include "visual_test_runner.h"
#include <cmath>
#include "../WorldB.h"
#include "../MaterialType.h"
#include "../CellB.h"
#include "../Vector2d.h"
#include "spdlog/spdlog.h"
#include <sstream>
#include <iomanip>

class ForceInfluencedMovementTest : public VisualTestBase {
protected:
    void SetUp() override {
        // Call parent SetUp first.
        VisualTestBase::SetUp();
        
        // Create world with desired size using framework method.
        world = createWorldB(10, 10);
        
        // Apply test-specific defaults.
        world->setWallsEnabled(false);
        world->setCohesionBindForceEnabled(true);
        world->setCohesionComForceEnabled(true);
        world->setCohesionBindForceStrength(1);  // Using default value.
        world->setCohesionComForceStrength(50);   // Closer to default (was 0.1, default is 150).
        world->setAdhesionEnabled(true);  // Enable adhesion for wall support.
        world->setAdhesionStrength(3.0);  // Closer to default (was 0.5, default is 5).
        world->setAddParticlesEnabled(false);  // Disable automatic particle addition.
        world->reset();
        
        // Set up logging to see detailed output.
        spdlog::set_level(spdlog::level::debug);
    }
    
    void TearDown() override {
        // Call parent TearDown first (it may need to access the world).
        VisualTestBase::TearDown();
        // Then clean up our world.
        world.reset();
    }
    
    // Helper: Run simulation for multiple timesteps and check if material moved.
    bool materialMovedAfterSteps(uint32_t x, uint32_t y, MaterialType expectedType, int timesteps = 50) {
        Vector2d initialCOM = world->at(x, y).getCOM();
        
        for (int i = 0; i < timesteps; i++) {
            world->advanceTime(0.016); // Full physics simulation.
        }
        
        // Check if material is still at original position with same amount.
        const CellB& cell = world->at(x, y);
        bool stillPresent = (cell.getMaterialType() == expectedType && 
                           cell.getFillRatio() > 0.5);
        
        if (stillPresent) {
            Vector2d finalCOM = cell.getCOM();
            double comChange = (finalCOM - initialCOM).mag();
            return comChange > 0.1; // Significant COM movement.
        }
        
        return true; // Material transferred to different cell = movement occurred.
    }
    
    // Helper: Check if materials stay connected (no movement between them).
    bool materialsStayConnected(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, 
                               MaterialType expectedType, int timesteps = 50) {
        for (int i = 0; i < timesteps; i++) {
            world->advanceTime(0.016);
        }
        
        // Both cells should still contain the material.
        const CellB& cell1 = world->at(x1, y1);
        const CellB& cell2 = world->at(x2, y2);
        
        return (cell1.getMaterialType() == expectedType && cell1.getFillRatio() > 0.5) &&
               (cell2.getMaterialType() == expectedType && cell2.getFillRatio() > 0.5);
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(ForceInfluencedMovementTest, IsolatedWaterMovesFreely) {
    // Isolated water should accumulate velocity and eventually move due to low cohesion.
    world->addMaterialAtCell(5, 5, MaterialType::WATER, 1.0);
    
    showInitialState(world.get(), "Isolated WATER particle at center");
    
    if (visual_mode_) {
        updateDisplay(world.get(), "Water has low cohesion (0.6), should move freely when isolated");
        pauseIfVisual(1000);
        runContinuousSimulation(world.get(), 50, "Water accumulating velocity from gravity");
    }
    
    bool moved = materialMovedAfterSteps(5, 5, MaterialType::WATER, 50);
    EXPECT_TRUE(moved) << "Isolated water should move after accumulating velocity from gravity";
    
    if (visual_mode_) {
        updateDisplay(world.get(), "Test complete - isolated water moved as expected");
        waitForNext();
    }
}


TEST_F(ForceInfluencedMovementTest, DirtClusterShowsCohesion) {
    // Create a dirt cluster on a wall foundation - center should resist breaking away due to moderate cohesion (0.3).
    // Place cluster at bottom of world with wall support for structural stability.
    
    // Add wall foundation for structural support.
    world->addMaterialAtCell(4, 9, MaterialType::WALL, 1.0); // Wall foundation left.
    world->addMaterialAtCell(5, 9, MaterialType::WALL, 1.0); // Wall foundation center.
    world->addMaterialAtCell(6, 9, MaterialType::WALL, 1.0); // Wall foundation right.
    
    // Create dirt cluster on top of wall.
    world->addMaterialAtCell(5, 8, MaterialType::DIRT, 1.0); // Center (on wall).
    world->addMaterialAtCell(5, 7, MaterialType::DIRT, 1.0); // Above.
    world->addMaterialAtCell(4, 8, MaterialType::DIRT, 1.0); // Left.
    world->addMaterialAtCell(6, 8, MaterialType::DIRT, 1.0); // Right.
    
    showInitialState(world.get(), "DIRT cluster: 4 particles in cross formation");
    
    if (visual_mode_) {
        updateDisplay(world.get(), "Dirt has moderate cohesion (0.3), cluster should stay together");
        pauseIfVisual(1000);
        
        // Run simulation with rendering every frame.
        runContinuousSimulation(world.get(), 30, "Cohesion keeping cluster together");
    } else {
        // Non-visual mode: run all steps at once.
        for (int i = 0; i < 30; i++) {
            world->advanceTime(0.016);
        }
    }
    
    // All dirt pieces should stay relatively close due to cohesion.
    bool centerPresent = (world->at(5, 8).getMaterialType() == MaterialType::DIRT && 
                         world->at(5, 8).getFillRatio() > 0.5);
    bool clustered = (world->at(5, 7).getMaterialType() == MaterialType::DIRT && 
                     world->at(5, 7).getFillRatio() > 0.5) &&
                    (world->at(4, 8).getMaterialType() == MaterialType::DIRT && 
                     world->at(4, 8).getFillRatio() > 0.5);
    
    EXPECT_TRUE(centerPresent && clustered) << "Dirt cluster should show cohesive behavior";
    
    if (visual_mode_) {
        if (centerPresent && clustered) {
            updateDisplay(world.get(), "✓ Test passed - dirt cluster maintained cohesion");
        } else {
            updateDisplay(world.get(), "✗ Test failed - dirt cluster broke apart");
        }
        waitForNext();
    }
}

TEST_F(ForceInfluencedMovementTest, IsolatedDirtMovesFreely) {
    // Isolated dirt should move freely (no cohesion resistance).
    world->addMaterialAtCell(5, 5, MaterialType::DIRT, 1.0);
    
    showInitialState(world.get(), "Isolated DIRT particle at center");
    
    if (visual_mode_) {
        updateDisplay(world.get(), "Isolated dirt has no cohesion resistance, should fall freely");
        pauseIfVisual(1000);
        runContinuousSimulation(world.get(), 50, "Dirt falling under gravity");
    }
    
    bool moved = materialMovedAfterSteps(5, 5, MaterialType::DIRT, 50);
    EXPECT_TRUE(moved) << "Isolated dirt should move freely (no cohesion resistance)";
    
    if (visual_mode_) {
        updateDisplay(world.get(), "Test complete - isolated dirt moved as expected");
        waitForNext();
    }
}

TEST_F(ForceInfluencedMovementTest, MaterialPropertyDifferences) {
    // Test that different materials behave differently due to their cohesion properties.
    
    // Place isolated samples of each material - isolated means no cohesion resistance.
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);  // Cohesion (0.6).
    world->addMaterialAtCell(4, 2, MaterialType::DIRT, 1.0);   // Cohesion (0.3)  
    world->addMaterialAtCell(6, 2, MaterialType::METAL, 1.0);  // Cohesion (0.9).
    
    showInitialState(world.get(), "Three isolated materials: WATER, DIRT, METAL");
    
    if (visual_mode_) {
        std::stringstream ss;
        ss << "Cohesion values: WATER=0.6, DIRT=0.3, METAL=0.9\n"
           << "All isolated materials should fall (no neighbors = no cohesion resistance)";
        updateDisplay(world.get(), ss.str());
        pauseIfVisual(2000);
        
        // Show simulation with rendering every frame.
        runContinuousSimulation(world.get(), 50, "All materials falling under gravity");
    }
    
    // All isolated materials should move since they have no cohesion resistance.
    bool waterMoved = materialMovedAfterSteps(2, 2, MaterialType::WATER, 50);
    bool dirtMoved = materialMovedAfterSteps(4, 2, MaterialType::DIRT, 50);
    bool metalMoved = materialMovedAfterSteps(6, 2, MaterialType::METAL, 50);
    
    EXPECT_TRUE(waterMoved) << "Isolated water should move";
    EXPECT_TRUE(dirtMoved) << "Isolated dirt should move";
    EXPECT_TRUE(metalMoved) << "Isolated metal should move";
    
    if (visual_mode_) {
        updateDisplay(world.get(), "Test complete - all isolated materials moved");
        waitForNext();
    }
    
    // The key difference is HOW they behave when connected to neighbors.
    // That's tested in the other test cases.
}

TEST_F(ForceInfluencedMovementTest, HighlyConnectedMetalStaysFixed) {
    // Create a 3x3 metal block - center piece should be completely immobilized.
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            world->addMaterialAtCell(5 + dx, 5 + dy, MaterialType::METAL, 1.0);
        }
    }
    
    // Add a single floating wall piece below the metal block for support.
    world->addMaterialAtCell(5, 7, MaterialType::WALL, 1.0);
    
    showInitialStateWithStep(world.get(), "3x3 METAL block with single wall support below - center should be immobilized");
    
    // Debug: Check pressure status.
    spdlog::info("Pressure system status:");
    spdlog::info("  - Hydrostatic pressure enabled: {}", world->isHydrostaticPressureEnabled());
    spdlog::info("  - Dynamic pressure enabled: {}", world->isDynamicPressureEnabled());
    
    // Center piece with 8 metal neighbors should have very high cohesion resistance.
    // Resistance = 0.9 * 8 * 1.0 = 7.2, much higher than gravity force ≈ 0.236.
    // Plus it has adhesion support from the wall below.
    
    Vector2d initialCOM = world->at(5, 5).getCOM();
    
    if (visual_mode_) {
        std::stringstream ss;
        ss << "Center METAL cell has 8 neighbors\n"
           << "Cohesion resistance = 0.9 * 8 = 7.2\n"
           << "Gravity force ≈ 0.236\n"
           << "Plus adhesion support from WALL below\n"
           << "Center should remain fixed";
        updateDisplay(world.get(), ss.str());
        pauseIfVisual(2000);
        
        // Run simulation with rendering every frame.
        runContinuousSimulation(world.get(), 100, "Metal block stable");
    } else {
        // Non-visual mode - run all at once.
        for (int i = 0; i < 100; i++) {
            world->advanceTime(0.016);
        }
    }
    
    // Verify all 9 cells of the 3x3 block are still METAL in their original positions.
    bool allCellsStillMetal = true;
    std::stringstream failureDetails;
    
    for (int dy = 0; dy < 3; dy++) {
        for (int dx = 0; dx < 3; dx++) {
            int x = 4 + dx;
            int y = 4 + dy;
            const CellB& cell = world->at(x, y);
            
            if (cell.getMaterialType() != MaterialType::METAL || cell.getFillRatio() < 0.9) {
                allCellsStillMetal = false;
                failureDetails << "Cell (" << x << "," << y << ") is no longer METAL or has low fill. "
                             << "Material: " << getMaterialName(cell.getMaterialType()) 
                             << ", Fill: " << cell.getFillRatio() << "\n";
            }
        }
    }
    
    // Also check center cell COM movement.
    Vector2d finalCOM = world->at(5, 5).getCOM();
    double comMovement = (finalCOM - initialCOM).mag();
    bool minimalMovement = (comMovement <= 1.01);  // Allow COM to move within cell boundaries.
    
    // Debug output.
    spdlog::info("Test result - All cells still METAL: {}, Center COM movement: {}", 
                 allCellsStillMetal,
                 comMovement);
    
    if (!allCellsStillMetal) {
        spdlog::error("Metal block integrity failed:\n{}", failureDetails.str());
    }
    
    EXPECT_TRUE(allCellsStillMetal && minimalMovement) 
        << "All 3x3 metal cells should remain in place with high cohesion. "
        << "All cells METAL: " << allCellsStillMetal 
        << ", COM movement: " << comMovement 
        << " (expected <= 1.01)\n" 
        << failureDetails.str();
    
    if (visual_mode_) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(4);
        ss << "COM movement: " << comMovement << " (expected <= 1.01)\n";
        ss << "All cells still METAL: " << (allCellsStillMetal ? "YES" : "NO") << "\n";
        if (allCellsStillMetal && minimalMovement) {
            ss << "✓ Test passed - all metal cells remained in place";
        } else {
            ss << "✗ Test failed - metal block lost integrity\n";
            if (!allCellsStillMetal) {
                ss << failureDetails.str();
            }
        }
        updateDisplay(world.get(), ss.str());
        waitForNext();
    }
}
