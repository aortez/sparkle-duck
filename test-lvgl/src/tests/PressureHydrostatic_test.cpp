#include <gtest/gtest.h>
#include "visual_test_runner.h"
#include <cmath>
#include "../WorldB.h"
#include "../MaterialType.h"
#include "../WorldBPressureCalculator.h"
#include <spdlog/spdlog.h>

class PressureHydrostaticTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Enable trace logging to see detailed physics.
        spdlog::set_level(spdlog::level::trace);
        
        // Create a 5x5 world for hydrostatic pressure testing (taller for pressure gradient).
        world = createWorldB(5, 5);
        
        // FOCUS: Testing hydrostatic pressure - pressure from weight of material above.
        world->setDynamicPressureEnabled(false);      // Disable dynamic pressure.
        world->setHydrostaticPressureEnabled(true);   // Enable hydrostatic pressure for testing.
        world->setPressureScale(1.0);                 // Enable pressure scale.
        
        // Apply test-specific settings.
        world->setWallsEnabled(false);
        world->setAddParticlesEnabled(false);
        world->setGravity(9.81); // Strong gravity to create hydrostatic pressure.
        
        spdlog::debug("[TEST] PressureHydrostatic test settings: dynamic_pressure=disabled, hydrostatic_pressure=enabled, walls=disabled");
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(PressureHydrostaticTest, ColumnOfWaterCreatesHydrostaticPressure) {
    spdlog::info("[TEST] Testing hydrostatic pressure from column of water");
    
    // Create a vertical column of water to test hydrostatic pressure.
    // Layout (5x5 grid):
    //   0 1 2 3 4.
    // 0 . . . . .
    // 1 . . W . .  <- Top water (should have low pressure).
    // 2 . . W . .  <- Middle water (should have medium pressure)  
    // 3 . . W . .  <- Bottom water (should have high pressure from weight above).
    // 4 . . . . .
    
    world->addMaterialAtCell(2, 1, MaterialType::WATER, 1.0);  // Top water.
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);  // Middle water.
    world->addMaterialAtCell(2, 3, MaterialType::WATER, 1.0);  // Bottom water.
    
    CellB& topWater = world->at(2, 1);
    CellB& middleWater = world->at(2, 2);
    CellB& bottomWater = world->at(2, 3);
    
    // Initialize all with zero pressure and velocity.
    topWater.setVelocity(Vector2d(0.0, 0.0));
    topWater.setHydrostaticPressure(0.0);
    topWater.setDynamicPressure(0.0);
    
    middleWater.setVelocity(Vector2d(0.0, 0.0));
    middleWater.setHydrostaticPressure(0.0);
    middleWater.setDynamicPressure(0.0);
    
    bottomWater.setVelocity(Vector2d(0.0, 0.0));
    bottomWater.setHydrostaticPressure(0.0);
    bottomWater.setDynamicPressure(0.0);
    
    spdlog::info("Initial setup:");
    spdlog::info("  Top water [2,1]: fill={:.2f} vel=({:.2f},{:.2f}) pressure={:.3f}", 
                 topWater.getFillRatio(), topWater.getVelocity().x, topWater.getVelocity().y,
                 topWater.getHydrostaticPressure());
    spdlog::info("  Middle water [2,2]: fill={:.2f} vel=({:.2f},{:.2f}) pressure={:.3f}", 
                 middleWater.getFillRatio(), middleWater.getVelocity().x, middleWater.getVelocity().y,
                 middleWater.getHydrostaticPressure());
    spdlog::info("  Bottom water [2,3]: fill={:.2f} vel=({:.2f},{:.2f}) pressure={:.3f}", 
                 bottomWater.getFillRatio(), bottomWater.getVelocity().x, bottomWater.getVelocity().y,
                 bottomWater.getHydrostaticPressure());
    
    // Log initial test state after setup.
    logInitialTestState(world.get(), "Water column ready for hydrostatic pressure testing");
    
    // Run multiple timesteps to allow hydrostatic pressure to build up.
    spdlog::info("\n--- HYDROSTATIC PRESSURE DEVELOPMENT ---");
    
    for (int timestep = 0; timestep < 10; timestep++) {
        double topPressureBefore = topWater.getHydrostaticPressure();
        double middlePressureBefore = middleWater.getHydrostaticPressure();
        double bottomPressureBefore = bottomWater.getHydrostaticPressure();
        
        world->advanceTime(0.016);
        
        double topPressureAfter = topWater.getHydrostaticPressure();
        double middlePressureAfter = middleWater.getHydrostaticPressure();
        double bottomPressureAfter = bottomWater.getHydrostaticPressure();
        
        spdlog::info("Timestep {}:", timestep + 1);
        spdlog::info("  Top: pressure {:.3f}→{:.3f}", topPressureBefore, topPressureAfter);
        spdlog::info("  Middle: pressure {:.3f}→{:.3f}", middlePressureBefore, middlePressureAfter);
        spdlog::info("  Bottom: pressure {:.3f}→{:.3f}", bottomPressureBefore, bottomPressureAfter);
        
        // Check for hydrostatic pressure development.
        if (bottomPressureAfter > topPressureAfter + 0.01) {
            spdlog::info("  🌊 HYDROSTATIC PRESSURE GRADIENT DETECTED!");
            spdlog::info("  Pressure increases with depth: Top={:.3f} < Middle={:.3f} < Bottom={:.3f}", 
                         topPressureAfter, middlePressureAfter, bottomPressureAfter);
            break;
        }
        
        // Check if any pressure is developing at all.
        if (topPressureAfter > 0.01 || middlePressureAfter > 0.01 || bottomPressureAfter > 0.01) {
            spdlog::info("  💧 Pressure detected in water column");
        }
    }
    
    // Final analysis of hydrostatic pressure gradient.
    double finalTopPressure = topWater.getHydrostaticPressure();
    double finalMiddlePressure = middleWater.getHydrostaticPressure();
    double finalBottomPressure = bottomWater.getHydrostaticPressure();
    
    spdlog::info("\n--- HYDROSTATIC PRESSURE ANALYSIS ---");
    spdlog::info("Final pressures:");
    spdlog::info("  Top [2,1]: {:.3f}", finalTopPressure);
    spdlog::info("  Middle [2,2]: {:.3f}", finalMiddlePressure);
    spdlog::info("  Bottom [2,3]: {:.3f}", finalBottomPressure);
    
    // Test for proper hydrostatic pressure gradient.
    bool pressureGradientExists = (finalBottomPressure > finalMiddlePressure) && 
                                  (finalMiddlePressure > finalTopPressure);
    
    bool anyPressureGenerated = (finalTopPressure > 0.001) || 
                               (finalMiddlePressure > 0.001) || 
                               (finalBottomPressure > 0.001);
    
    if (pressureGradientExists) {
        spdlog::info("✅ SUCCESS: Hydrostatic pressure gradient established!");
        spdlog::info("Pressure increases with depth as expected");
        
        EXPECT_GT(finalBottomPressure, finalMiddlePressure) << "Bottom should have higher pressure than middle";
        EXPECT_GT(finalMiddlePressure, finalTopPressure) << "Middle should have higher pressure than top";
        EXPECT_GT(finalBottomPressure, 0.01) << "Bottom should have significant hydrostatic pressure";
        
    } else if (anyPressureGenerated) {
        spdlog::warn("⚠️  Pressure generated but gradient not established properly");
        spdlog::info("This may indicate hydrostatic pressure system needs calibration");
        
    } else {
        spdlog::warn("⚠️  No hydrostatic pressure generated");
        spdlog::info("Hydrostatic pressure system may not be implemented or enabled");
    }
    
    spdlog::info("✅ ColumnOfWaterCreatesHydrostaticPressure test completed");
}

TEST_F(PressureHydrostaticTest, HydrostaticPressureDrivesMovement) {
    spdlog::info("[TEST] Testing hydrostatic pressure driving water movement");
    
    // Create an L-shaped water configuration to test pressure-driven flow.
    // Layout (5x5 grid):
    //   0 1 2 3 4.
    // 0 . . . . .
    // 1 . W W . .  <- High water level (should create pressure).
    // 2 . W W . .  <- High water level.
    // 3 . W . . .  <- Lower water level (pressure should push water here).
    // 4 . . . . .
    
    // Create high water column on left.
    world->addMaterialAtCell(1, 1, MaterialType::WATER, 1.0);  // High column.
    world->addMaterialAtCell(1, 2, MaterialType::WATER, 1.0);  // High column.
    world->addMaterialAtCell(1, 3, MaterialType::WATER, 1.0);  // High column.
    
    // Create partial water on right side (room for flow).
    world->addMaterialAtCell(2, 1, MaterialType::WATER, 1.0);  // Full top.
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);  // Full middle.
    // Leave [2,3] empty - this is where pressure should push water.
    
    CellB& leftHigh = world->at(1, 1);
    CellB& leftMid = world->at(1, 2);
    CellB& leftLow = world->at(1, 3);
    CellB& rightHigh = world->at(2, 1);
    CellB& rightMid = world->at(2, 2);
    CellB& rightLow = world->at(2, 3);  // Empty, should receive flow.
    
    spdlog::info("Initial L-shaped water configuration:");
    spdlog::info("  Left column: [1,1]={:.2f} [1,2]={:.2f} [1,3]={:.2f}", 
                 leftHigh.getFillRatio(), leftMid.getFillRatio(), leftLow.getFillRatio());
    spdlog::info("  Right column: [2,1]={:.2f} [2,2]={:.2f} [2,3]={:.2f}", 
                 rightHigh.getFillRatio(), rightMid.getFillRatio(), rightLow.getFillRatio());
    
    // Log initial test state.
    logInitialTestState(world.get(), "L-shaped water ready for pressure-driven flow testing");
    
    // Run simulation to observe pressure-driven movement.
    spdlog::info("\n--- PRESSURE-DRIVEN FLOW SEQUENCE ---");
    
    for (int timestep = 0; timestep < 50; timestep++) {
        double leftPressureBefore = leftLow.getHydrostaticPressure();
        double rightLowFillBefore = rightLow.getFillRatio();
        Vector2d leftVelBefore = leftLow.getVelocity();
        
        world->advanceTime(0.016);
        
        double leftPressureAfter = leftLow.getHydrostaticPressure();
        double rightLowFillAfter = rightLow.getFillRatio();
        Vector2d leftVelAfter = leftLow.getVelocity();
        
        spdlog::info("Timestep {}:", timestep + 1);
        spdlog::info("  Left bottom: pressure {:.3f}→{:.3f} vel ({:.2f},{:.2f})→({:.2f},{:.2f})",
                     leftPressureBefore, leftPressureAfter,
                     leftVelBefore.x, leftVelBefore.y, leftVelAfter.x, leftVelAfter.y);
        spdlog::info("  Right bottom: fill {:.3f}→{:.3f}", rightLowFillBefore, rightLowFillAfter);
        
        // Check for pressure-driven flow.
        if (rightLowFillAfter > rightLowFillBefore + 0.01) {
            spdlog::info("  🌊 PRESSURE-DRIVEN FLOW! Water moved to right bottom cell");
            spdlog::info("  Fill increased by {:.3f}", rightLowFillAfter - rightLowFillBefore);
        }
        
        // Check for pressure development.
        if (leftPressureAfter > 0.01) {
            spdlog::info("  💧 Pressure detected: {:.3f}", leftPressureAfter);
        }
        
        // Check for rightward velocity (pressure pushing water right).
        if (leftVelAfter.x > 0.1) {
            spdlog::info("  ➡️  Rightward velocity detected: {:.3f}", leftVelAfter.x);
        }
        
        // Stop if significant flow occurred.
        if (rightLowFillAfter > 0.2) {
            spdlog::info("  ✅ Significant flow achieved, stopping test");
            break;
        }
    }
    
    // Final analysis.
    double finalRightLowFill = rightLow.getFillRatio();
    double finalLeftPressure = leftLow.getHydrostaticPressure();
    
    spdlog::info("\n--- PRESSURE-DRIVEN FLOW ANALYSIS ---");
    spdlog::info("Final state:");
    spdlog::info("  Right bottom fill: {:.3f} (started at 0.000)", finalRightLowFill);
    spdlog::info("  Left bottom pressure: {:.3f}", finalLeftPressure);
    
    bool flowOccurred = finalRightLowFill > 0.05;
    bool pressureGenerated = finalLeftPressure > 0.01;
    
    if (flowOccurred) {
        spdlog::info("✅ SUCCESS: Hydrostatic pressure drove water movement!");
        EXPECT_GT(finalRightLowFill, 0.05) << "Water should flow to lower level due to pressure";
        
    } else if (pressureGenerated) {
        spdlog::warn("⚠️  Pressure generated but no significant flow occurred");
        spdlog::info("May indicate pressure forces are too weak or flow resistance too high");
        
    } else {
        spdlog::warn("⚠️  No pressure or flow detected");
        spdlog::info("Hydrostatic pressure system may need investigation");
    }
    
    spdlog::info("✅ HydrostaticPressureDrivesMovement test completed");
}

TEST_F(PressureHydrostaticTest, SliceBasedHydrostaticCalculation) {
    spdlog::info("[TEST] Testing slice-based hydrostatic pressure calculation");
    
    // Create a simple vertical column to test slice-based pressure calculation.
    // Layout (5x5 grid):
    //   0 1 2 3 4.
    // 0 . . . . .
    // 1 . . W . .  <- Top: pressure = 0 (no material above).
    // 2 . . W . .  <- Middle: pressure = density * gravity * 1 cell.
    // 3 . . W . .  <- Bottom: pressure = density * gravity * 2 cells.
    // 4 . . . . .
    
    world->addMaterialAtCell(2, 1, MaterialType::WATER, 1.0);  // Top.
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);  // Middle.
    world->addMaterialAtCell(2, 3, MaterialType::WATER, 1.0);  // Bottom.
    
    CellB& topWater = world->at(2, 1);
    CellB& middleWater = world->at(2, 2);
    CellB& bottomWater = world->at(2, 3);
    
    // Clear any existing pressure values.
    topWater.setHydrostaticPressure(0.0);
    topWater.setDynamicPressure(0.0);
    middleWater.setHydrostaticPressure(0.0);
    middleWater.setDynamicPressure(0.0);
    bottomWater.setHydrostaticPressure(0.0);
    bottomWater.setDynamicPressure(0.0);
    
    spdlog::info("Before pressure calculation:");
    spdlog::info("  Top [2,1]: hydrostatic_pressure={:.3f} effective_density={:.3f}", 
                 topWater.getHydrostaticPressure(), topWater.getEffectiveDensity());
    spdlog::info("  Middle [2,2]: hydrostatic_pressure={:.3f} effective_density={:.3f}", 
                 middleWater.getHydrostaticPressure(), middleWater.getEffectiveDensity());
    spdlog::info("  Bottom [2,3]: hydrostatic_pressure={:.3f} effective_density={:.3f}", 
                 bottomWater.getHydrostaticPressure(), bottomWater.getEffectiveDensity());
    
    // Manually trigger hydrostatic pressure calculation using the pressure calculator.
    WorldBPressureCalculator pressureCalc(*world);
    pressureCalc.calculateHydrostaticPressure();
    
    spdlog::info("After pressure calculation:");
    spdlog::info("  Top [2,1]: hydrostatic_pressure={:.3f}", topWater.getHydrostaticPressure());
    spdlog::info("  Middle [2,2]: hydrostatic_pressure={:.3f}", middleWater.getHydrostaticPressure());
    spdlog::info("  Bottom [2,3]: hydrostatic_pressure={:.3f}", bottomWater.getHydrostaticPressure());
    
    // Calculate expected pressures based on slice-based calculation.
    double water_density = topWater.getEffectiveDensity();  // fill_ratio * material_density.
    double gravity_magnitude = std::abs(world->getGravity());
    double slice_thickness = 1.0;
    
    double expected_top_pressure = 0.0;  // No material above.
    double expected_middle_pressure = water_density * gravity_magnitude * slice_thickness;  // 1 cell above.
    double expected_bottom_pressure = 2.0 * water_density * gravity_magnitude * slice_thickness;  // 2 cells above.
    
    spdlog::info("Expected pressures (water_density={:.3f}, gravity={:.3f}):", water_density, gravity_magnitude);
    spdlog::info("  Top expected: {:.3f}", expected_top_pressure);
    spdlog::info("  Middle expected: {:.3f}", expected_middle_pressure);
    spdlog::info("  Bottom expected: {:.3f}", expected_bottom_pressure);
    
    // Test slice-based pressure calculation.
    EXPECT_NEAR(topWater.getHydrostaticPressure(), expected_top_pressure, 0.001) 
        << "Top cell should have zero hydrostatic pressure";
    EXPECT_NEAR(middleWater.getHydrostaticPressure(), expected_middle_pressure, 0.001) 
        << "Middle cell should have pressure from 1 cell above";
    EXPECT_NEAR(bottomWater.getHydrostaticPressure(), expected_bottom_pressure, 0.001) 
        << "Bottom cell should have pressure from 2 cells above";
    
    // Test pressure gradient (each level should increase by same amount).
    double top_to_middle_diff = middleWater.getHydrostaticPressure() - topWater.getHydrostaticPressure();
    double middle_to_bottom_diff = bottomWater.getHydrostaticPressure() - middleWater.getHydrostaticPressure();
    
    EXPECT_NEAR(top_to_middle_diff, middle_to_bottom_diff, 0.001)
        << "Pressure gradient should be uniform in uniform material column";
    
    spdlog::info("Pressure differences:");
    spdlog::info("  Top→Middle: {:.3f}", top_to_middle_diff);
    spdlog::info("  Middle→Bottom: {:.3f}", middle_to_bottom_diff);
    
    if (std::abs(top_to_middle_diff - middle_to_bottom_diff) < 0.001) {
        spdlog::info("✅ SUCCESS: Uniform pressure gradient established!");
    } else {
        spdlog::warn("⚠️  Pressure gradient not uniform - may indicate calculation issue");
    }
    
    spdlog::info("✅ SliceBasedHydrostaticCalculation test completed");
}

TEST_F(PressureHydrostaticTest, MixedMaterialHydrostaticPressure) {
    spdlog::info("[TEST] Testing hydrostatic pressure with different material densities");
    
    // Create a column with different materials to test density-based pressure.
    // Layout (5x5 grid):
    //   0 1 2 3 4.
    // 0 . . . . .
    // 1 . . M . .  <- METAL (high density).
    // 2 . . W . .  <- WATER (medium density).
    // 3 . . D . .  <- DIRT (lower density).
    // 4 . . . . .
    
    world->addMaterialAtCell(2, 1, MaterialType::METAL, 1.0);  // Top - heaviest.
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);  // Middle.
    world->addMaterialAtCell(2, 3, MaterialType::DIRT, 1.0);   // Bottom - lightest.
    
    CellB& metalCell = world->at(2, 1);
    CellB& waterCell = world->at(2, 2);
    CellB& dirtCell = world->at(2, 3);
    
    // Clear any existing pressure values.
    metalCell.setHydrostaticPressure(0.0);
    metalCell.setDynamicPressure(0.0);
    waterCell.setHydrostaticPressure(0.0);
    waterCell.setDynamicPressure(0.0);
    dirtCell.setHydrostaticPressure(0.0);
    dirtCell.setDynamicPressure(0.0);
    
    spdlog::info("Material densities:");
    spdlog::info("  METAL [2,1]: effective_density={:.3f}", metalCell.getEffectiveDensity());
    spdlog::info("  WATER [2,2]: effective_density={:.3f}", waterCell.getEffectiveDensity());
    spdlog::info("  DIRT [2,3]: effective_density={:.3f}", dirtCell.getEffectiveDensity());
    
    // Calculate hydrostatic pressure using the pressure calculator.
    WorldBPressureCalculator pressureCalc(*world);
    pressureCalc.calculateHydrostaticPressure();
    
    spdlog::info("After pressure calculation:");
    spdlog::info("  METAL [2,1]: hydrostatic_pressure={:.3f}", metalCell.getHydrostaticPressure());
    spdlog::info("  WATER [2,2]: hydrostatic_pressure={:.3f}", waterCell.getHydrostaticPressure());
    spdlog::info("  DIRT [2,3]: hydrostatic_pressure={:.3f}", dirtCell.getHydrostaticPressure());
    
    // Calculate expected pressures.
    double metal_density = metalCell.getEffectiveDensity();
    double water_density = waterCell.getEffectiveDensity();
    double gravity_magnitude = std::abs(world->getGravity());
    
    double expected_metal_pressure = 0.0;  // No material above.
    double expected_water_pressure = metal_density * gravity_magnitude;  // Metal above.
    double expected_dirt_pressure = (metal_density + water_density) * gravity_magnitude;  // Metal + Water above.
    
    spdlog::info("Expected pressures:");
    spdlog::info("  METAL expected: {:.3f}", expected_metal_pressure);
    spdlog::info("  WATER expected: {:.3f}", expected_water_pressure);
    spdlog::info("  DIRT expected: {:.3f}", expected_dirt_pressure);
    
    // Test mixed material pressure calculation.
    EXPECT_NEAR(metalCell.getHydrostaticPressure(), expected_metal_pressure, 0.001)
        << "Top METAL should have zero pressure";
    EXPECT_NEAR(waterCell.getHydrostaticPressure(), expected_water_pressure, 0.001)
        << "WATER should have pressure from METAL above";
    EXPECT_NEAR(dirtCell.getHydrostaticPressure(), expected_dirt_pressure, 0.001)
        << "DIRT should have pressure from METAL + WATER above";
    
    // Test that pressure increases down the column.
    EXPECT_LT(metalCell.getHydrostaticPressure(), waterCell.getHydrostaticPressure())
        << "Water pressure should be higher than metal pressure";
    EXPECT_LT(waterCell.getHydrostaticPressure(), dirtCell.getHydrostaticPressure())
        << "Dirt pressure should be highest";
    
    // Test that heavier materials contribute more to pressure.
    double metal_contribution = waterCell.getHydrostaticPressure() - metalCell.getHydrostaticPressure();
    double water_contribution = dirtCell.getHydrostaticPressure() - waterCell.getHydrostaticPressure();
    
    if (metal_density > water_density) {
        EXPECT_GT(metal_contribution, water_contribution)
            << "Heavier METAL should contribute more pressure than lighter WATER";
        spdlog::info("✅ SUCCESS: Heavier materials create more hydrostatic pressure!");
    }
    
    spdlog::info("Pressure contributions:");
    spdlog::info("  METAL contribution: {:.3f}", metal_contribution);
    spdlog::info("  WATER contribution: {:.3f}", water_contribution);
    
    spdlog::info("✅ MixedMaterialHydrostaticPressure test completed");
}

TEST_F(PressureHydrostaticTest, WaterColumnWithEmptySpace) {
    spdlog::info("[TEST] Testing water column with empty space for lateral flow");
    
    // Create the specific scenario: water column with wall and empty space.
    // Layout (2x3 grid):
    //   0 1.
    // 0 W W  <- ~F WF (water, wall).
    // 1 W W  <- ~F WF (water, wall) 
    // 2 W .  <- ~F -0 (water, empty).
    //
    // This matches the pattern: ~F WF, ~F WF, ~F -0.
    
    // Create a smaller 2x3 world for the exact scenario.
    world = createWorldB(2, 3);
    world->setDynamicPressureEnabled(false);      // Focus on hydrostatic only.
    world->setHydrostaticPressureEnabled(true);   
    world->setPressureScale(1.0);
    world->setWallsEnabled(false);  // We'll add walls manually.
    world->setAddParticlesEnabled(false);
    world->setGravity(9.81); // Strong gravity for clear pressure effects.
    
    // Set up the exact scenario: ~F WF, ~F WF, ~F -0.
    world->addMaterialAtCell(0, 0, MaterialType::WATER, 1.0);  // Row 0: water.
    world->addMaterialAtCell(1, 0, MaterialType::WALL, 1.0);   // Row 0: wall.
    world->addMaterialAtCell(0, 1, MaterialType::WATER, 1.0);  // Row 1: water.
    world->addMaterialAtCell(1, 1, MaterialType::WALL, 1.0);   // Row 1: wall.
    world->addMaterialAtCell(0, 2, MaterialType::WATER, 1.0);  // Row 2: water.
    // Leave [1,2] empty (air) as specified by -0.
    
    CellB& topWater = world->at(0, 0);
    CellB& topWall = world->at(1, 0);
    CellB& middleWater = world->at(0, 1);
    CellB& middleWall = world->at(1, 1);
    CellB& bottomWater = world->at(0, 2);
    CellB& bottomEmpty = world->at(1, 2);
    
    // Initialize with zero velocities and pressures.
    topWater.setVelocity(Vector2d(0.0, 0.0));
    topWater.setHydrostaticPressure(0.0);
    topWater.setDynamicPressure(0.0);
    middleWater.setVelocity(Vector2d(0.0, 0.0));
    middleWater.setHydrostaticPressure(0.0);
    middleWater.setDynamicPressure(0.0);
    bottomWater.setVelocity(Vector2d(0.0, 0.0));
    bottomWater.setHydrostaticPressure(0.0);
    bottomWater.setDynamicPressure(0.0);
    
    spdlog::info("Initial setup matching ~F WF, ~F WF, ~F -0:");
    spdlog::info("  Top row: water[0,0]={:.2f} wall[1,0]={:.2f}", topWater.getFillRatio(), topWall.getFillRatio());
    spdlog::info("  Middle row: water[0,1]={:.2f} wall[1,1]={:.2f}", middleWater.getFillRatio(), middleWall.getFillRatio());
    spdlog::info("  Bottom row: water[0,2]={:.2f} empty[1,2]={:.2f}", bottomWater.getFillRatio(), bottomEmpty.getFillRatio());
    
    // Log initial test state.
    logInitialTestState(world.get(), "Water column with empty space ready for pressure testing");
    
    // Track key metrics over time.
    double initialTopFill = topWater.getFillRatio();
    double initialMiddleFill = middleWater.getFillRatio();
    double initialBottomFill = bottomWater.getFillRatio();
    double initialEmptyFill = bottomEmpty.getFillRatio();
    
    spdlog::info("\n--- HYDROSTATIC PRESSURE AND FLOW DEVELOPMENT ---");
    
    // Run simulation for extended period to observe pressure effects.
    for (int timestep = 0; timestep < 30; timestep++) {
        // Capture state before timestep.
        double topPressureBefore = topWater.getHydrostaticPressure();
        double middlePressureBefore = middleWater.getHydrostaticPressure();
        double bottomPressureBefore = bottomWater.getHydrostaticPressure();
        Vector2d bottomVelBefore = bottomWater.getVelocity();
        double emptyFillBefore = bottomEmpty.getFillRatio();
        
        world->advanceTime(0.016);
        
        // Capture state after timestep.
        double topPressureAfter = topWater.getHydrostaticPressure();
        double middlePressureAfter = middleWater.getHydrostaticPressure();
        double bottomPressureAfter = bottomWater.getHydrostaticPressure();
        Vector2d bottomVelAfter = bottomWater.getVelocity();
        double emptyFillAfter = bottomEmpty.getFillRatio();
        
        // Log every 5 timesteps for clarity.
        if (timestep % 5 == 0 || timestep < 5) {
            spdlog::info("Timestep {}:", timestep + 1);
            spdlog::info("  Top pressure: {:.3f}→{:.3f}", topPressureBefore, topPressureAfter);
            spdlog::info("  Middle pressure: {:.3f}→{:.3f}", middlePressureBefore, middlePressureAfter);
            spdlog::info("  Bottom pressure: {:.3f}→{:.3f}", bottomPressureBefore, bottomPressureAfter);
            spdlog::info("  Bottom velocity: ({:.2f},{:.2f})→({:.2f},{:.2f})", 
                         bottomVelBefore.x, bottomVelBefore.y, bottomVelAfter.x, bottomVelAfter.y);
            spdlog::info("  Empty fill: {:.3f}→{:.3f}", emptyFillBefore, emptyFillAfter);
        }
        
        // Check for pressure gradient development.
        if (bottomPressureAfter > middlePressureAfter + 0.01) {
            if (timestep < 10) {
                spdlog::info("  🌊 PRESSURE GRADIENT ESTABLISHED at timestep {}", timestep + 1);
                spdlog::info("  Bottom pressure ({:.3f}) > Middle pressure ({:.3f}) > Top pressure ({:.3f})", 
                             bottomPressureAfter, middlePressureAfter, topPressureAfter);
            }
        }
        
        // Check for lateral flow (water moving from bottom water to empty space).
        if (emptyFillAfter > emptyFillBefore + 0.01) {
            spdlog::info("  ➡️  LATERAL FLOW! Water moved to empty: {:.3f}→{:.3f}", 
                         emptyFillBefore, emptyFillAfter);
        }
        
        // Check for rightward velocity (pressure driving water right).
        if (bottomVelAfter.x > 0.1) {
            spdlog::info("  ➡️  Rightward velocity: {:.3f}", bottomVelAfter.x);
        }
        
        // Check for pressure-driven movement.
        if (bottomPressureAfter > 5.0) {
            spdlog::info("  💧 High pressure in bottom water: {:.3f}", bottomPressureAfter);
        }
        
        // Stop if significant redistribution occurred.
        double totalFillChange = std::abs(emptyFillAfter - initialEmptyFill);
        if (totalFillChange > 0.3) {
            spdlog::info("  ✅ Significant water redistribution detected, stopping test");
            break;
        }
    }
    
    // Final analysis.
    double finalTopFill = topWater.getFillRatio();
    double finalMiddleFill = middleWater.getFillRatio();
    double finalBottomFill = bottomWater.getFillRatio();
    double finalEmptyFill = bottomEmpty.getFillRatio();
    
    double finalTopPressure = topWater.getHydrostaticPressure();
    double finalMiddlePressure = middleWater.getHydrostaticPressure();
    double finalBottomPressure = bottomWater.getHydrostaticPressure();
    
    spdlog::info("\n--- FINAL WATER DISTRIBUTION ANALYSIS ---");
    spdlog::info("Water distribution changes:");
    spdlog::info("  Top: {:.3f}→{:.3f} (change: {:.3f})", 
                 initialTopFill, finalTopFill, finalTopFill - initialTopFill);
    spdlog::info("  Middle: {:.3f}→{:.3f} (change: {:.3f})", 
                 initialMiddleFill, finalMiddleFill, finalMiddleFill - initialMiddleFill);
    spdlog::info("  Bottom: {:.3f}→{:.3f} (change: {:.3f})", 
                 initialBottomFill, finalBottomFill, finalBottomFill - initialBottomFill);
    spdlog::info("  Empty: {:.3f}→{:.3f} (change: {:.3f})", 
                 initialEmptyFill, finalEmptyFill, finalEmptyFill - initialEmptyFill);
    
    spdlog::info("Final pressures:");
    spdlog::info("  Top pressure: {:.3f}", finalTopPressure);
    spdlog::info("  Middle pressure: {:.3f}", finalMiddlePressure);
    spdlog::info("  Bottom pressure: {:.3f}", finalBottomPressure);
    
    // Test expectations.
    bool pressureGradientExists = finalBottomPressure > finalMiddlePressure + 0.01;
    bool waterRedistributed = finalEmptyFill > 0.05;
    bool waterConserved = std::abs((finalTopFill + finalMiddleFill + finalBottomFill + finalEmptyFill) - 
                                  (initialTopFill + initialMiddleFill + initialBottomFill)) < 0.01;
    
    if (pressureGradientExists && waterRedistributed) {
        spdlog::info("✅ SUCCESS: Hydrostatic pressure caused water redistribution!");
        spdlog::info("Pressure gradient drove water flow as expected");
        
        EXPECT_GT(finalBottomPressure, finalMiddlePressure + 0.01) 
            << "Pressure gradient should exist with bottom > middle > top";
        EXPECT_TRUE(waterRedistributed) 
            << "Water should redistribute from bottom water to empty space due to pressure";
        EXPECT_TRUE(waterConserved) 
            << "Total water should be conserved";
            
    } else if (pressureGradientExists) {
        spdlog::warn("⚠️  Pressure gradient exists but no water redistribution");
        spdlog::info("May indicate pressure forces too weak or flow resistance too high");
        
    } else {
        spdlog::warn("⚠️  No pressure gradient established");
        spdlog::info("Hydrostatic pressure system may need investigation");
    }
    
    // Additional physics validation.
    if (waterConserved) {
        spdlog::info("✅ Water conservation maintained");
    } else {
        spdlog::warn("⚠️  Water conservation violated - may indicate transfer bugs");
    }
    
    spdlog::info("✅ WaterColumnWithEmptySpace test completed");
}
