#include "core/World.h"
#include "core/organisms/TreeManager.h"
#include <gtest/gtest.h>

using namespace DirtSim;

class TreeManagerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        world = std::make_unique<World>(10, 10);
        manager = std::make_unique<TreeManager>();
    }

    std::unique_ptr<World> world;
    std::unique_ptr<TreeManager> manager;
};

TEST_F(TreeManagerTest, PlantSeedCreatesTree)
{
    TreeId id = manager->plantSeed(*world, 5, 5);

    EXPECT_NE(id, INVALID_TREE_ID);
    EXPECT_NE(manager->getTree(id), nullptr);
}
