#include <gtest/gtest.h>
#include "../World.h"
#include "../MaterialType.h"
#include "../WorldCohesionCalculator.h"
#include "spdlog/spdlog.h"

class DistanceToSupportTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a larger world for better testing.
        world = std::make_unique<World>(6, 6);
        world->setWallsEnabled(false);  // Disable walls to test pure ground support.
        
        spdlog::info("=== Distance to Support Test Setup ===");
        spdlog::info("World: 6x6 grid, walls disabled, ground support at y=5");
    }
    
    void logDistanceDetails(uint32_t x, uint32_t y, const std::string& description) {
        const Cell& cell = world->at(x, y);
        if (cell.isEmpty()) {
            spdlog::info("Cell ({},{}) - {}: EMPTY", x, y, description);
            return;
        }
        
        double distance = world->getSupportCalculator().calculateDistanceToSupport(x, y);
        bool hasSupport = world->getSupportCalculator().hasStructuralSupport(x, y);
        auto cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(x, y);
        
        spdlog::info("Cell ({},{}) - {}: material={}, distance={:.1f}, hasSupport={}, cohesion={:.3f}",
                     x, y, description, 
                     getMaterialName(cell.getMaterialType()),
                     distance, hasSupport, cohesion.resistance_magnitude);
    }
    
    std::unique_ptr<World> world;
};

TEST_F(DistanceToSupportTest, SingleFloatingCell) {
    // Test the simplest case: one dirt cell floating in the middle.
    // Should have distance 4 to ground (y=1 to y=5).
    
    spdlog::info("=== Single Floating Cell Test ===");
    
    // Place one dirt cell in the middle, far from ground.
    world->addMaterialAtCell(3, 1, MaterialType::DIRT, 1.0);  // 4 steps from ground at y=5.
    
    logDistanceDetails(3, 1, "floating-single");
    logDistanceDetails(3, 5, "ground-level");
    
    // Expected: distance should be 4, cohesion should be minimum (0.04).
    double distance = world->getSupportCalculator().calculateDistanceToSupport(3, 1);
    auto cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(3, 1);
    
    spdlog::info("Expected distance: 4, Actual distance: {:.1f}", distance);
    spdlog::info("Expected cohesion: ~0.04, Actual cohesion: {:.3f}", cohesion.resistance_magnitude);
    
    EXPECT_GT(distance, 3.5) << "Single floating cell should be ~4 steps from ground support";
    EXPECT_LT(cohesion.resistance_magnitude, 0.1) << "Floating cell should have minimal cohesion";
}

TEST_F(DistanceToSupportTest, GroundSupportDetection) {
    // Test that ground cells are correctly identified as having support.
    
    spdlog::info("=== Ground Support Detection Test ===");
    
    // Place dirt on ground level.
    world->addMaterialAtCell(2, 5, MaterialType::DIRT, 1.0);  // On ground.
    world->addMaterialAtCell(2, 4, MaterialType::DIRT, 1.0);  // 1 step from ground.
    world->addMaterialAtCell(2, 3, MaterialType::DIRT, 1.0);  // 2 steps from ground.
    
    logDistanceDetails(2, 5, "on-ground");
    logDistanceDetails(2, 4, "one-from-ground");
    logDistanceDetails(2, 3, "two-from-ground");
    
    // Test ground support detection.
    EXPECT_TRUE(world->getSupportCalculator().hasStructuralSupport(2, 5)) << "Ground level should have structural support";
    EXPECT_FALSE(world->getSupportCalculator().hasStructuralSupport(2, 4)) << "Above ground should not have direct support";
    
    // Test distance calculations.
    EXPECT_EQ(world->getSupportCalculator().calculateDistanceToSupport(2, 5), 0.0) << "Ground should have distance 0";
    EXPECT_EQ(world->getSupportCalculator().calculateDistanceToSupport(2, 4), 1.0) << "One above ground should have distance 1";
    EXPECT_EQ(world->getSupportCalculator().calculateDistanceToSupport(2, 3), 2.0) << "Two above ground should have distance 2";
}

TEST_F(DistanceToSupportTest, FloatingLShapeDetailed) {
    // Recreate the problematic L-shape with detailed BFS debugging.
    
    spdlog::info("=== Floating L-Shape Detailed Test ===");
    spdlog::info("Configuration:");
    spdlog::info("-----");
    spdlog::info("DDD--");  // y=1.
    spdlog::info("D----");  // y=2.
    spdlog::info("-----");  // y=3.
    spdlog::info("-----");  // y=4.
    spdlog::info("-----");  // y=5 (ground).
    
    // Create L-shape floating 4 steps from ground.
    world->addMaterialAtCell(0, 1, MaterialType::DIRT, 1.0);  // L-corner.
    world->addMaterialAtCell(1, 1, MaterialType::DIRT, 1.0);  // Horizontal arm.
    world->addMaterialAtCell(2, 1, MaterialType::DIRT, 1.0);  // Horizontal end.
    world->addMaterialAtCell(0, 2, MaterialType::DIRT, 1.0);  // Vertical arm.
    
    spdlog::info("Initial structure analysis:");
    logDistanceDetails(0, 1, "L-corner");
    logDistanceDetails(1, 1, "horizontal-arm");
    logDistanceDetails(2, 1, "horizontal-end");
    logDistanceDetails(0, 2, "vertical-arm");
    
    // All cells should be distance 3-4 from ground.
    double corner_dist = world->getSupportCalculator().calculateDistanceToSupport(0, 1);
    double arm_dist = world->getSupportCalculator().calculateDistanceToSupport(1, 1);
    double end_dist = world->getSupportCalculator().calculateDistanceToSupport(2, 1);
    double vertical_dist = world->getSupportCalculator().calculateDistanceToSupport(0, 2);
    
    spdlog::info("Distance summary: corner={:.1f}, arm={:.1f}, end={:.1f}, vertical={:.1f}",
                corner_dist, arm_dist, end_dist, vertical_dist);
    
    // All should be distance 3-4 from ground support, resulting in minimal cohesion.
    EXPECT_GT(corner_dist, 2.5) << "L-corner should be far from support";
    EXPECT_GT(arm_dist, 2.5) << "Horizontal arm should be far from support";
    EXPECT_GT(end_dist, 2.5) << "Horizontal end should be far from support";
    EXPECT_GT(vertical_dist, 2.5) << "Vertical arm should be far from support";
    
    // Check cohesion reduction.
    auto corner_cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(0, 1);
    auto end_cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(2, 1);
    
    EXPECT_LT(corner_cohesion.resistance_magnitude, 0.15) << "L-corner should have reduced cohesion";
    EXPECT_LT(end_cohesion.resistance_magnitude, 0.15) << "Horizontal end should have reduced cohesion";
}

TEST_F(DistanceToSupportTest, VerticalTowerShouldTopple) {
    // Test a tall thin tower that should be unstable without support.
    
    spdlog::info("=== Vertical Tower Test ===");
    spdlog::info("Configuration:");
    spdlog::info("--D--");  // y=0 (top).
    spdlog::info("--D--");  // y=1.
    spdlog::info("--D--");  // y=2.
    spdlog::info("--D--");  // y=3.
    spdlog::info("--D--");  // y=4.
    spdlog::info("-----");  // y=5 (ground).
    
    // Create tall tower with no ground support.
    for (uint32_t y = 0; y < 5; y++) {
        world->addMaterialAtCell(2, y, MaterialType::DIRT, 1.0);
    }
    
    spdlog::info("Tower analysis:");
    logDistanceDetails(2, 0, "tower-top");
    logDistanceDetails(2, 2, "tower-middle");
    logDistanceDetails(2, 4, "tower-bottom");
    
    // Top should be distance 5 from ground, bottom should be distance 1.
    double top_distance = world->getSupportCalculator().calculateDistanceToSupport(2, 0);
    double bottom_distance = world->getSupportCalculator().calculateDistanceToSupport(2, 4);
    
    EXPECT_GT(top_distance, 4.5) << "Tower top should be far from support";
    EXPECT_LT(bottom_distance, 1.5) << "Tower bottom should be close to support";
    
    // Top should have much less cohesion than bottom.
    auto top_cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(2, 0);
    auto bottom_cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(2, 4);
    
    EXPECT_LT(top_cohesion.resistance_magnitude, bottom_cohesion.resistance_magnitude) 
        << "Tower top should have less cohesion than bottom";
}

TEST_F(DistanceToSupportTest, FloatingIsland) {
    // Test multiple disconnected dirt cells - all should have max distance.
    
    spdlog::info("=== Floating Island Test ===");
    spdlog::info("Configuration:");
    spdlog::info("D-D--");  // y=1 (two separate floating dirt cells).
    spdlog::info("-----");
    spdlog::info("--D--");  // y=3 (another separate cell).
    spdlog::info("-----");
    spdlog::info("-----");  // y=5 (ground).
    
    // Create separate floating dirt cells.
    world->addMaterialAtCell(0, 1, MaterialType::DIRT, 1.0);  // Island 1.
    world->addMaterialAtCell(2, 1, MaterialType::DIRT, 1.0);  // Island 2.
    world->addMaterialAtCell(2, 3, MaterialType::DIRT, 1.0);  // Island 3.
    
    spdlog::info("Floating islands analysis:");
    logDistanceDetails(0, 1, "island-1");
    logDistanceDetails(2, 1, "island-2");
    logDistanceDetails(2, 3, "island-3");
    
    // All should be far from support and have minimal cohesion.
    double dist1 = world->getSupportCalculator().calculateDistanceToSupport(0, 1);
    double dist2 = world->getSupportCalculator().calculateDistanceToSupport(2, 1);
    double dist3 = world->getSupportCalculator().calculateDistanceToSupport(2, 3);
    
    EXPECT_GT(dist1, 3.5) << "Island 1 should be far from support";
    EXPECT_GT(dist2, 3.5) << "Island 2 should be far from support";
    EXPECT_GT(dist3, 1.5) << "Island 3 should be moderately far from support";
    
    auto cohesion1 = WorldCohesionCalculator(*world).calculateCohesionForce(0, 1);
    auto cohesion2 = WorldCohesionCalculator(*world).calculateCohesionForce(2, 1);
    
    // These have 0 neighbors, so cohesion should be minimal regardless.
    EXPECT_LT(cohesion1.resistance_magnitude, 0.1) << "Isolated cells should have minimal cohesion";
    EXPECT_LT(cohesion2.resistance_magnitude, 0.1) << "Isolated cells should have minimal cohesion";
}

TEST_F(DistanceToSupportTest, DiagonalStaircase) {
    // Test diagonal structures that might "stick" unrealistically.
    
    spdlog::info("=== Diagonal Staircase Test ===");
    spdlog::info("Configuration:");
    spdlog::info("D----");  // y=1.
    spdlog::info("-D---");  // y=2.
    spdlog::info("--D--");  // y=3.
    spdlog::info("---D-");  // y=4.
    spdlog::info("-----");  // y=5 (ground).
    
    // Create diagonal staircase.
    world->addMaterialAtCell(0, 1, MaterialType::DIRT, 1.0);  // Top step.
    world->addMaterialAtCell(1, 2, MaterialType::DIRT, 1.0);  // Step 2.
    world->addMaterialAtCell(2, 3, MaterialType::DIRT, 1.0);  // Step 3.
    world->addMaterialAtCell(3, 4, MaterialType::DIRT, 1.0);  // Bottom step.
    
    spdlog::info("Diagonal staircase analysis:");
    logDistanceDetails(0, 1, "top-step");
    logDistanceDetails(1, 2, "step-2");
    logDistanceDetails(2, 3, "step-3");
    logDistanceDetails(3, 4, "bottom-step");
    
    // Each step should be closer to support than the previous.
    double top_dist = world->getSupportCalculator().calculateDistanceToSupport(0, 1);
    double step2_dist = world->getSupportCalculator().calculateDistanceToSupport(1, 2);
    double step3_dist = world->getSupportCalculator().calculateDistanceToSupport(2, 3);
    double bottom_dist = world->getSupportCalculator().calculateDistanceToSupport(3, 4);
    
    spdlog::info("Staircase distances: top={:.1f}, step2={:.1f}, step3={:.1f}, bottom={:.1f}",
                top_dist, step2_dist, step3_dist, bottom_dist);
    
    // Distance should decrease as we get closer to ground.
    EXPECT_GT(top_dist, step2_dist) << "Top step should be farther than step 2";
    EXPECT_GT(step2_dist, step3_dist) << "Step 2 should be farther than step 3";
    EXPECT_GT(step3_dist, bottom_dist) << "Step 3 should be farther than bottom step";
    
    // All should have reduced cohesion since they're disconnected.
    auto top_cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(0, 1);
    auto bottom_cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(3, 4);
    
    // These have 0 neighbors each, so cohesion should be 0 regardless of distance.
    EXPECT_LT(top_cohesion.resistance_magnitude, 0.1) << "Isolated diagonal cells should have minimal cohesion";
    EXPECT_LT(bottom_cohesion.resistance_magnitude, 0.1) << "Isolated diagonal cells should have minimal cohesion";
}

TEST_F(DistanceToSupportTest, MetalAnchorSupport) {
    // Test that metal provides structural support like the original cantilever test.
    
    spdlog::info("=== Metal Anchor Support Test ===");
    
    // Place metal anchor and connected dirt.
    world->addMaterialAtCell(0, 2, MaterialType::METAL, 1.0);  // Metal anchor.
    world->addMaterialAtCell(1, 2, MaterialType::DIRT, 1.0);   // Connected dirt.
    world->addMaterialAtCell(2, 2, MaterialType::DIRT, 1.0);   // Cantilever dirt.
    
    logDistanceDetails(0, 2, "metal-anchor");
    logDistanceDetails(1, 2, "connected-dirt");
    logDistanceDetails(2, 2, "cantilever-dirt");
    
    // Metal should have distance 0 (self-support).
    // Connected dirt should have distance 1.
    // Cantilever should have distance 2.
    double metal_dist = world->getSupportCalculator().calculateDistanceToSupport(0, 2);
    double connected_dist = world->getSupportCalculator().calculateDistanceToSupport(1, 2);
    double cantilever_dist = world->getSupportCalculator().calculateDistanceToSupport(2, 2);
    
    EXPECT_EQ(metal_dist, 0.0) << "Metal should provide self-support";
    EXPECT_EQ(connected_dist, 1.0) << "Connected dirt should be distance 1 from metal";
    EXPECT_EQ(cantilever_dist, 2.0) << "Cantilever should be distance 2 from metal";
    
    // Cantilever should have reduced cohesion.
    auto cantilever_cohesion = WorldCohesionCalculator(*world).calculateCohesionForce(2, 2);
    EXPECT_LT(cantilever_cohesion.resistance_magnitude, 0.15) << "Cantilever should have reduced cohesion";
}