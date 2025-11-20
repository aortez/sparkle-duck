#include "core/World.h"
#include "core/organisms/TreeManager.h"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace DirtSim;

class TreeGerminationTest : public ::testing::Test {
protected:
    void SetUp() override { world = std::make_unique<World>(7, 7); }

    void setupGerminationWorld()
    {
        for (uint32_t y = 0; y < 7; ++y) {
            for (uint32_t x = 0; x < 7; ++x) {
                world->at(x, y) = Cell();
            }
        }

        for (uint32_t y = 4; y < 7; ++y) {
            for (uint32_t x = 0; x < 7; ++x) {
                world->addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
            }
        }
    }

    std::unique_ptr<World> world;
};

TEST_F(TreeGerminationTest, SeedFallsOntoGround)
{
    setupGerminationWorld();
    TreeId id = world->getTreeManager().plantSeed(*world, 3, 1);

    EXPECT_EQ(world->at(3, 1).material_type, MaterialType::SEED);

    for (int i = 0; i < 100; i++) {
        world->advanceTime(0.016);
    }

    const Tree* tree = world->getTreeManager().getTree(id);
    ASSERT_NE(tree, nullptr);
    EXPECT_GT(tree->seed_position.y, 1);
}

TEST_F(TreeGerminationTest, SeedGerminates)
{
    setupGerminationWorld();
    TreeId id = world->getTreeManager().plantSeed(*world, 3, 3);

    const Tree* tree = world->getTreeManager().getTree(id);
    EXPECT_EQ(tree->stage, GrowthStage::SEED);

    while (tree->stage != GrowthStage::SAPLING && tree->age_seconds < 10.0) {
        world->advanceTime(0.016);
    }

    EXPECT_EQ(tree->stage, GrowthStage::SAPLING);

    EXPECT_EQ(world->at(3, 3).material_type, MaterialType::SEED);
    EXPECT_EQ(world->at(3, 4).material_type, MaterialType::ROOT);
    EXPECT_EQ(world->at(3, 2).material_type, MaterialType::WOOD);
}

TEST_F(TreeGerminationTest, SeedBlockedByWall)
{
    for (uint32_t y = 0; y < 7; ++y) {
        for (uint32_t x = 0; x < 7; ++x) {
            world->at(x, y).replaceMaterial(MaterialType::WALL, 1.0);
        }
    }

    world->at(3, 3).replaceMaterial(MaterialType::AIR, 0.0);

    TreeId id = world->getTreeManager().plantSeed(*world, 3, 3);
    const Tree* tree = world->getTreeManager().getTree(id);

    for (int i = 0; i < 1000; i++) {
        world->advanceTime(0.016);
    }

    EXPECT_EQ(tree->stage, GrowthStage::SEED);
}

TEST_F(TreeGerminationTest, SaplingGrowsBalanced)
{
    setupGerminationWorld();
    TreeId id = world->getTreeManager().plantSeed(*world, 3, 3);
    const Tree* tree = world->getTreeManager().getTree(id);

    for (int i = 0; i < 2000; i++) {
        world->advanceTime(0.016);
    }

    EXPECT_EQ(tree->stage, GrowthStage::SAPLING);
    EXPECT_GT(tree->cells.size(), 3);
}

TEST_F(TreeGerminationTest, RootsStopAtWater)
{
    for (uint32_t y = 0; y < 7; ++y) {
        for (uint32_t x = 0; x < 7; ++x) {
            world->at(x, y) = Cell();
        }
    }

    for (uint32_t y = 5; y < 7; ++y) {
        for (uint32_t x = 0; x < 7; ++x) {
            world->at(x, y).replaceMaterial(MaterialType::WATER, 1.0);
        }
    }

    for (uint32_t x = 0; x < 7; ++x) {
        world->at(x, 4).replaceMaterial(MaterialType::DIRT, 1.0);
    }

    world->getTreeManager().plantSeed(*world, 3, 3);

    for (int i = 0; i < 2000; i++) {
        world->advanceTime(0.016);
    }

    EXPECT_EQ(world->at(3, 4).material_type, MaterialType::ROOT);
    EXPECT_NE(world->at(3, 5).material_type, MaterialType::ROOT);
    EXPECT_EQ(world->at(3, 5).material_type, MaterialType::WATER);
}

TEST_F(TreeGerminationTest, TreeStopsGrowingWhenOutOfEnergy)
{
    setupGerminationWorld();
    TreeId id = world->getTreeManager().plantSeed(*world, 3, 3);
    Tree* tree = world->getTreeManager().getTree(id);

    tree->total_energy = 20.0;

    for (int i = 0; i < 3000; i++) {
        world->advanceTime(0.016);
    }

    EXPECT_LT(tree->total_energy, 10.0);
    int initial_cells = 3;
    EXPECT_GT(tree->cells.size(), initial_cells);
    EXPECT_LT(tree->cells.size(), 10);
}
