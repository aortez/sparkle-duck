#include "visual_test_runner.h"
#include "../WorldB.h"
#include "../RulesBNew.h"
#include <spdlog/spdlog.h>

class WorldBVisualTest : public VisualTestBase {
protected:
    void SetUp() override {
        VisualTestBase::SetUp();
        
        // Default to small test world
        width = 20;
        height = 20;
        createTestWorldB();
    }
    
    void createTestWorldB() {
        worldB = std::make_unique<WorldB>(width, height);
        auto rules = std::make_unique<RulesBNew>();
        worldB->setWorldRulesBNew(std::move(rules));
    }

    void TearDown() override {
        worldB.reset();
        VisualTestBase::TearDown();
    }
    
    // Test data members
    std::unique_ptr<WorldB> worldB;
    uint32_t width;
    uint32_t height;
};

TEST_F(WorldBVisualTest, EmptyWorldAdvance) {
    spdlog::info("Starting WorldBVisualTest::EmptyWorldAdvance test");
    
    // Verify initial state
    EXPECT_EQ(worldB->getWidth(), width);
    EXPECT_EQ(worldB->getHeight(), height);
    EXPECT_EQ(worldB->getTimestep(), 0);
    
    // Advance time once
    worldB->advanceTime(0.016);
    
    // Verify timestep advanced
    EXPECT_EQ(worldB->getTimestep(), 1);
    
    // Verify world is still valid
    worldB->validateState("After empty advance");
    
    spdlog::info("EmptyWorldAdvance test completed successfully");
}

TEST_F(WorldBVisualTest, MaterialInitialization) {
    spdlog::info("Starting WorldBVisualTest::MaterialInitialization test");
    
    // Initialize test materials
    worldB->initializeTestMaterials();
    
    // Verify boundaries are walls
    EXPECT_TRUE(worldB->at(0, 0).isWall());
    EXPECT_TRUE(worldB->at(width-1, 0).isWall());
    EXPECT_TRUE(worldB->at(0, height-1).isWall());
    EXPECT_TRUE(worldB->at(width-1, height-1).isWall());
    
    // Check that some dirt was added
    bool foundDirt = false;
    for (uint32_t y = 1; y < height-1; ++y) {
        for (uint32_t x = 1; x < width-1; ++x) {
            if (worldB->at(x, y).material == MaterialType::DIRT) {
                foundDirt = true;
                EXPECT_GT(worldB->at(x, y).fill_ratio, 0.0);
                EXPECT_LE(worldB->at(x, y).fill_ratio, 1.0);
                break;
            }
        }
        if (foundDirt) break;
    }
    EXPECT_TRUE(foundDirt) << "Should have found some dirt cells";
    
    // Check that some water was added
    bool foundWater = false;
    for (uint32_t y = 1; y < height-1; ++y) {
        for (uint32_t x = 1; x < width-1; ++x) {
            if (worldB->at(x, y).material == MaterialType::WATER) {
                foundWater = true;
                EXPECT_GT(worldB->at(x, y).fill_ratio, 0.0);
                EXPECT_LE(worldB->at(x, y).fill_ratio, 1.0);
                break;
            }
        }
        if (foundWater) break;
    }
    EXPECT_TRUE(foundWater) << "Should have found some water cells";
    
    worldB->validateState("After material initialization");
    
    spdlog::info("MaterialInitialization test completed successfully");
}

TEST_F(WorldBVisualTest, CellBMaterialProperties) {
    spdlog::info("Starting WorldBVisualTest::CellBMaterialProperties test");
    
    // Test material property access
    CellB testCell;
    
    // Test different materials
    testCell.setMaterial(MaterialType::DIRT, 0.8);
    EXPECT_EQ(testCell.material, MaterialType::DIRT);
    EXPECT_DOUBLE_EQ(testCell.fill_ratio, 0.8);
    EXPECT_GT(testCell.getDensity(), 1.0); // Dirt should be denser than water
    
    testCell.setMaterial(MaterialType::WATER, 0.6);
    EXPECT_EQ(testCell.material, MaterialType::WATER);
    EXPECT_DOUBLE_EQ(testCell.fill_ratio, 0.6);
    EXPECT_DOUBLE_EQ(testCell.getDensity(), 1.0); // Water reference density
    
    testCell.setMaterial(MaterialType::WOOD, 1.0);
    EXPECT_EQ(testCell.material, MaterialType::WOOD);
    EXPECT_DOUBLE_EQ(testCell.fill_ratio, 1.0);
    EXPECT_LT(testCell.getDensity(), 1.0); // Wood should be less dense than water
    
    // Test air (empty)
    testCell.setMaterial(MaterialType::AIR, 0.0);
    EXPECT_TRUE(testCell.isEmpty());
    EXPECT_FALSE(testCell.isWall());
    
    // Test wall
    testCell.setMaterial(MaterialType::WALL, 1.0);
    EXPECT_TRUE(testCell.isWall());
    EXPECT_FALSE(testCell.isEmpty());
    
    spdlog::info("CellBMaterialProperties test completed successfully");
}