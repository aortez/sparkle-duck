#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../MaterialType.h"
#include <spdlog/spdlog.h>

class WorldBVisualTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Default to 3x3 world, but tests can override these
        width = 3;
        height = 3;
        createTestWorldB();
    }
    
    void createTestWorldB() {
        // Create small world for testing with no draw area (tests don't need UI)
        world = std::make_unique<WorldB>(width, height, nullptr);
        world->setAddParticlesEnabled(false);
        // Disable walls for testing to get clean mass calculations
        world->setWallsEnabled(false);
        world->reset(); // Reset to clear any walls that were created
    }

    void TearDown() override {
        world.reset();
        VisualTestBase::TearDown();
    }
    
    // Test data members
    std::unique_ptr<WorldB> world;
    uint32_t width;
    uint32_t height;
};

TEST_F(WorldBVisualTest, EmptyWorldAdvance) {
    spdlog::info("Starting WorldBVisualTest::EmptyWorldAdvance test");
    
    // Verify world is initially empty
    EXPECT_EQ(world->getWidth(), 3);
    EXPECT_EQ(world->getHeight(), 3);
    EXPECT_NEAR(world->getTotalMass(), 0.0, 0.001);
    
    // Advance time should work on empty world
    world->advanceTime(0.016);
    
    // Mass should still be zero
    EXPECT_NEAR(world->getTotalMass(), 0.0, 0.001);
}

TEST_F(WorldBVisualTest, MaterialInitialization) {
    spdlog::info("Starting WorldBVisualTest::MaterialInitialization test");
    
    // Test all material types can be added
    std::vector<MaterialType> materials = {
        MaterialType::DIRT,
        MaterialType::WATER, 
        MaterialType::WOOD,
        MaterialType::SAND,
        MaterialType::METAL,
        MaterialType::LEAF
    };
    
    double expectedMass = 0.0;
    
    for (size_t i = 0; i < materials.size() && i < 6; ++i) {
        MaterialType mat = materials[i];
        int x = i % 3;
        int y = i / 3;
        
        // Add material at cell coordinates
        world->addMaterialAtCell(x, y, mat);
        expectedMass += getMaterialDensity(mat);
        
        spdlog::info("Added {} at ({},{}) - density: {}", 
                     getMaterialName(mat), x, y, getMaterialDensity(mat));
    }
    
    // Verify total mass matches expected
    double actualMass = world->getTotalMass();
    EXPECT_NEAR(actualMass, expectedMass, 0.1);
    
    spdlog::info("Expected mass: {}, Actual mass: {}", expectedMass, actualMass);
}

TEST_F(WorldBVisualTest, BasicGravity) {
    spdlog::info("Starting WorldBVisualTest::BasicGravity test");
    
    // Create 3x3 world and add dirt at top
    world->addMaterialAtCell(1, 0, MaterialType::DIRT);
    
    double initialMass = world->getTotalMass();
    EXPECT_NEAR(initialMass, getMaterialDensity(MaterialType::DIRT), 0.1);
    
    // Advance time to let gravity work
    for (int i = 0; i < 10; ++i) {
        world->advanceTime(0.016);
    }
    
    // Mass should be conserved
    double finalMass = world->getTotalMass();
    EXPECT_NEAR(finalMass, initialMass, 0.1);
    
    spdlog::info("Initial mass: {}, Final mass: {}", initialMass, finalMass);
}

TEST_F(WorldBVisualTest, MaterialProperties) {
    spdlog::info("Starting WorldBVisualTest::MaterialProperties test");
    
    // Test material properties are correct
    EXPECT_NEAR(getMaterialDensity(MaterialType::AIR), 0.001, 0.0001);
    EXPECT_NEAR(getMaterialDensity(MaterialType::DIRT), 1.5, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::WATER), 1.0, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::WOOD), 0.8, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::SAND), 1.8, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::METAL), 7.8, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::LEAF), 0.3, 0.1);
    EXPECT_NEAR(getMaterialDensity(MaterialType::WALL), 1000.0, 1.0);
    
    // Test fluid properties
    EXPECT_TRUE(isMaterialFluid(MaterialType::AIR));
    EXPECT_FALSE(isMaterialFluid(MaterialType::DIRT));
    EXPECT_TRUE(isMaterialFluid(MaterialType::WATER));
    EXPECT_FALSE(isMaterialFluid(MaterialType::WOOD));
    
    // Test rigid properties
    EXPECT_FALSE(isMaterialRigid(MaterialType::AIR));
    EXPECT_FALSE(isMaterialRigid(MaterialType::DIRT));
    EXPECT_FALSE(isMaterialRigid(MaterialType::WATER));
    EXPECT_TRUE(isMaterialRigid(MaterialType::WOOD));
    EXPECT_TRUE(isMaterialRigid(MaterialType::METAL));
    EXPECT_TRUE(isMaterialRigid(MaterialType::WALL));
}

TEST_F(WorldBVisualTest, VelocityLimiting) {
    spdlog::info("Starting WorldBVisualTest::VelocityLimiting test");
    
    // Test that WorldB implements velocity limiting as designed
    // This is more of a functionality check than physics validation
    
    // Add material 
    world->addMaterialAtCell(1, 1, MaterialType::DIRT);
    
    // Advance several timesteps
    for (int i = 0; i < 20; ++i) {
        world->advanceTime(0.016);
    }
    
    // WorldB should handle velocity limiting internally
    // We can't directly test velocities since they're internal to CellB
    // But we can verify the world still functions properly
    double mass = world->getTotalMass();
    EXPECT_GT(mass, 0.0);
    
    spdlog::info("Mass after velocity limiting test: {}", mass);
}

TEST_F(WorldBVisualTest, ResetFunctionality) {
    spdlog::info("Starting WorldBVisualTest::ResetFunctionality test");
    
    // Add some materials
    world->addMaterialAtCell(0, 0, MaterialType::DIRT);
    world->addMaterialAtCell(2, 2, MaterialType::WATER);
    
    double massBeforeReset = world->getTotalMass();
    EXPECT_GT(massBeforeReset, 0.0);
    
    // Reset the world
    world->reset();
    
    // World should be empty after reset
    double massAfterReset = world->getTotalMass();
    EXPECT_NEAR(massAfterReset, 0.0, 0.001);
    
    spdlog::info("Mass before reset: {}, after reset: {}", massBeforeReset, massAfterReset);
}