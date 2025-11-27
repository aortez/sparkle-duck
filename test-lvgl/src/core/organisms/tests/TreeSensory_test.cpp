#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "core/WorldData.h"
#include "core/organisms/Tree.h"
#include "core/organisms/TreeManager.h"
#include "core/organisms/brains/RuleBasedBrain.h"
#include <gtest/gtest.h>

using namespace DirtSim;

/**
 * Test that histograms for OOB neural cells are empty.
 *
 * For a 9×9 world with tree centered at (4,4), the neural grid offset is (-3,-3).
 * Neural cells outside the 9×9 world bounds should have empty histograms.
 */
TEST(TreeSensoryTest, OOBCellsHaveEmptyHistograms)
{
    // Create 9×9 world.
    auto world = std::make_unique<World>(9, 9);

    // Fill with air.
    for (uint32_t y = 0; y < 9; y++) {
        for (uint32_t x = 0; x < 9; x++) {
            world->getData().at(x, y) = Cell();
        }
    }

    // Add dirt at bottom.
    for (uint32_t y = 6; y < 9; y++) {
        for (uint32_t x = 0; x < 9; x++) {
            world->addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
        }
    }

    // Plant seed at center (4,4).
    TreeId tree_id = world->getTreeManager().plantSeed(*world, 4, 4);
    Tree* tree = world->getTreeManager().getTree(tree_id);
    ASSERT_NE(tree, nullptr);

    // Gather sensory data.
    TreeSensoryData sensory = tree->gatherSensoryData(*world);

    // For 9×9 world centered at seed (4,4), offset should be (-3,-3).
    EXPECT_EQ(sensory.world_offset.x, -3);
    EXPECT_EQ(sensory.world_offset.y, -3);

    // Check that OOB cells have empty histograms (all zeros).
    // Left edge: neural x=0-2 should be OOB.
    for (int y = 0; y < 15; y++) {
        for (int x = 0; x < 3; x++) {
            double total = 0.0;
            for (int m = 0; m < TreeSensoryData::NUM_MATERIALS; m++) {
                total += sensory.material_histograms[y][x][m];
            }
            EXPECT_NEAR(total, 0.0, 1e-6)
                << "Neural cell (" << x << "," << y << ") should have empty histogram (left OOB)";
        }
    }

    // Right edge: neural x=12-14 should be OOB.
    for (int y = 0; y < 15; y++) {
        for (int x = 12; x < 15; x++) {
            double total = 0.0;
            for (int m = 0; m < TreeSensoryData::NUM_MATERIALS; m++) {
                total += sensory.material_histograms[y][x][m];
            }
            EXPECT_NEAR(total, 0.0, 1e-6)
                << "Neural cell (" << x << "," << y << ") should have empty histogram (right OOB)";
        }
    }

    // Top edge: neural y=0-2 should be OOB.
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 15; x++) {
            double total = 0.0;
            for (int m = 0; m < TreeSensoryData::NUM_MATERIALS; m++) {
                total += sensory.material_histograms[y][x][m];
            }
            EXPECT_NEAR(total, 0.0, 1e-6)
                << "Neural cell (" << x << "," << y << ") should have empty histogram (top OOB)";
        }
    }

    // Bottom edge: neural y=12-14 should be OOB.
    for (int y = 12; y < 15; y++) {
        for (int x = 0; x < 15; x++) {
            double total = 0.0;
            for (int m = 0; m < TreeSensoryData::NUM_MATERIALS; m++) {
                total += sensory.material_histograms[y][x][m];
            }
            EXPECT_NEAR(total, 0.0, 1e-6)
                << "Neural cell (" << x << "," << y << ") should have empty histogram (bottom OOB)";
        }
    }
}

/**
 * Test that mass calculation doesn't double-count cells.
 */
TEST(TreeSensoryTest, MassCalculationNoDuplicates)
{
    // Create 9×9 world.
    auto world = std::make_unique<World>(9, 9);

    // Fill with air.
    for (uint32_t y = 0; y < 9; y++) {
        for (uint32_t x = 0; x < 9; x++) {
            world->getData().at(x, y) = Cell();
        }
    }

    // Add dirt at bottom.
    for (uint32_t y = 6; y < 9; y++) {
        for (uint32_t x = 0; x < 9; x++) {
            world->addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
        }
    }

    // Plant seed at center - it will fall to (4,5).
    TreeId tree_id = world->getTreeManager().plantSeed(*world, 4, 4);
    Tree* tree = world->getTreeManager().getTree(tree_id);
    ASSERT_NE(tree, nullptr);

    // Advance physics so seed falls.
    for (int i = 0; i < 10; i++) {
        world->advanceTime(0.016);
    }

    // Manually create a simple tree structure:
    // - SEED at (4,5) - should not count in above/below (at seed.y)
    // - ROOT at (4,6) - should count as below_ground (mass 1.2)
    // - WOOD at (4,4) - should count as above_ground (mass 0.3)

    // Force grow ROOT at (4,6).
    world->getData().at(4, 6).replaceMaterial(MaterialType::ROOT, 1.0);
    world->getData().at(4, 6).organism_id = tree_id;
    tree->cells.insert(Vector2i{ 4, 6 });

    // Force grow WOOD at (4,4).
    world->getData().at(4, 4).replaceMaterial(MaterialType::WOOD, 1.0);
    world->getData().at(4, 4).organism_id = tree_id;
    tree->cells.insert(Vector2i{ 4, 4 });

    // Update seed position (it fell).
    tree->seed_position = Vector2i{ 4, 5 };

    // Gather sensory data.
    TreeSensoryData sensory = tree->gatherSensoryData(*world);

    // Check histogram directly - expected: only 3 cells should have material.

    int seed_count = 0;
    int root_count = 0;
    int wood_count = 0;

    for (int y = 0; y < 15; y++) {
        for (int x = 0; x < 15; x++) {
            const auto& hist = sensory.material_histograms[y][x];
            if (hist[static_cast<int>(MaterialType::SEED)] > 0.5) seed_count++;
            if (hist[static_cast<int>(MaterialType::ROOT)] > 0.5) root_count++;
            if (hist[static_cast<int>(MaterialType::WOOD)] > 0.5) wood_count++;
        }
    }

    EXPECT_EQ(seed_count, 1) << "Should find exactly 1 SEED in histograms";
    EXPECT_EQ(root_count, 1) << "Should find exactly 1 ROOT in histograms";
    EXPECT_EQ(wood_count, 1) << "Should find exactly 1 WOOD in histograms";
}
