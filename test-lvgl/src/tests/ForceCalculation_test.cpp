#include <gtest/gtest.h>
#include <cmath>
#include "../WorldB.h"
#include "../MaterialType.h"

class ForceCalculationTest : public ::testing::Test {
protected:
    void SetUp() override {
        world = std::make_unique<WorldB>(5, 5, nullptr);
    }
    
    std::unique_ptr<WorldB> world;
};

TEST_F(ForceCalculationTest, EmptyCellHasZeroForces) {
    auto cohesion = world->calculateCohesionForce(2, 2);
    auto adhesion = world->calculateAdhesionForce(2, 2);
    
    EXPECT_EQ(cohesion.resistance_magnitude, 0.0);
    EXPECT_EQ(cohesion.connected_neighbors, 0);
    EXPECT_EQ(adhesion.force_magnitude, 0.0);
    EXPECT_EQ(adhesion.contact_points, 0);
}

TEST_F(ForceCalculationTest, IsolatedWaterHasNoForces) {
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    
    auto cohesion = world->calculateCohesionForce(2, 2);
    auto adhesion = world->calculateAdhesionForce(2, 2);
    
    // No same-material neighbors = no cohesion resistance
    EXPECT_EQ(cohesion.resistance_magnitude, 0.0);
    EXPECT_EQ(cohesion.connected_neighbors, 0);
    
    // No different-material neighbors = no adhesion
    EXPECT_EQ(adhesion.force_magnitude, 0.0);
    EXPECT_EQ(adhesion.contact_points, 0);
}

TEST_F(ForceCalculationTest, WaterWithWaterNeighborsHasCohesion) {
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(2, 1, MaterialType::WATER, 1.0); // Above
    world->addMaterialAtCell(1, 2, MaterialType::WATER, 1.0); // Left
    
    auto cohesion = world->calculateCohesionForce(2, 2);
    
    // Should have cohesion resistance from 2 same-material neighbors
    EXPECT_GT(cohesion.resistance_magnitude, 0.0);
    EXPECT_EQ(cohesion.connected_neighbors, 2);
    
    // Verify formula: resistance = material_cohesion * connected_neighbors * fill_ratio
    const MaterialProperties& props = getMaterialProperties(MaterialType::WATER);
    double expected_resistance = props.cohesion * 2.0 * 1.0;
    EXPECT_DOUBLE_EQ(cohesion.resistance_magnitude, expected_resistance);
}

TEST_F(ForceCalculationTest, WaterWithDirtNeighborHasAdhesion) {
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(3, 2, MaterialType::DIRT, 1.0); // Right
    
    auto adhesion = world->calculateAdhesionForce(2, 2);
    
    // Should have adhesion force from different-material neighbor
    EXPECT_GT(adhesion.force_magnitude, 0.0);
    EXPECT_EQ(adhesion.contact_points, 1);
    EXPECT_EQ(adhesion.target_material, MaterialType::DIRT);
    
    // Force should point toward the DIRT neighbor (direction: +1, 0)
    EXPECT_GT(adhesion.force_direction.x, 0.0);
    EXPECT_DOUBLE_EQ(adhesion.force_direction.y, 0.0);
}

TEST_F(ForceCalculationTest, MetalHasHighCohesion) {
    // Use interior coordinates to avoid boundary walls (5x5 grid: boundaries at x=0,4 y=0,4)
    world->addMaterialAtCell(2, 2, MaterialType::METAL, 1.0);
    world->addMaterialAtCell(2, 1, MaterialType::METAL, 1.0); // Above (2,2)
    
    auto cohesion_metal = world->calculateCohesionForce(2, 2);
    
    // Create new world for WATER test to avoid interference
    auto water_world = std::make_unique<WorldB>(5, 5, nullptr);
    water_world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    water_world->addMaterialAtCell(2, 1, MaterialType::WATER, 1.0); // Above (2,2)
    
    auto cohesion_water = water_world->calculateCohesionForce(2, 2);
    
    // With same neighbor count (1), METAL should have higher resistance due to higher cohesion property
    EXPECT_GT(cohesion_metal.resistance_magnitude, cohesion_water.resistance_magnitude);
    
    // Verify METAL has higher cohesion property (0.9 vs 0.1)
    const MaterialProperties& metal_props = getMaterialProperties(MaterialType::METAL);
    const MaterialProperties& water_props = getMaterialProperties(MaterialType::WATER);
    EXPECT_GT(metal_props.cohesion, water_props.cohesion);
    
    // Expected calculations:
    // METAL: cohesion=0.9, neighbors=1, fill=1.0 → resistance = 0.9 * 1 * 1.0 = 0.9
    // WATER: cohesion=0.1, neighbors=1, fill=1.0 → resistance = 0.1 * 1 * 1.0 = 0.1
    EXPECT_DOUBLE_EQ(cohesion_metal.resistance_magnitude, 0.9);
    EXPECT_DOUBLE_EQ(cohesion_water.resistance_magnitude, 0.1);
}

TEST_F(ForceCalculationTest, AdhesionUsesGeometricMean) {
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(3, 2, MaterialType::METAL, 1.0); // Right
    
    auto adhesion = world->calculateAdhesionForce(2, 2);
    
    // Verify mutual adhesion calculation (geometric mean)
    const MaterialProperties& water_props = getMaterialProperties(MaterialType::WATER);
    const MaterialProperties& metal_props = getMaterialProperties(MaterialType::METAL);
    double expected_mutual = std::sqrt(water_props.adhesion * metal_props.adhesion);
    
    // Force strength should be based on this mutual adhesion
    double expected_force_strength = expected_mutual * 1.0 * 1.0 * 1.0; // mutual * fill1 * fill2 * distance_weight
    EXPECT_DOUBLE_EQ(adhesion.force_magnitude, expected_force_strength);
}

TEST_F(ForceCalculationTest, PartialCellsFillRatioWeighting) {
    world->addMaterialAtCell(2, 2, MaterialType::WATER, 0.5); // Half-filled
    world->addMaterialAtCell(2, 1, MaterialType::WATER, 0.8); // Above, 80% filled
    
    auto cohesion = world->calculateCohesionForce(2, 2);
    
    // Expected: cohesion_property * connected_neighbors * own_fill_ratio
    // Note: connected_neighbors is count (1), not weighted by fill ratio
    const MaterialProperties& props = getMaterialProperties(MaterialType::WATER);
    double expected_resistance = props.cohesion * 1 * 0.5; // 0.1 * 1 * 0.5 = 0.05
    EXPECT_DOUBLE_EQ(cohesion.resistance_magnitude, expected_resistance);
    EXPECT_EQ(cohesion.connected_neighbors, 1);
}