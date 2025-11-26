#include "core/PhysicsSettings.h"
#include "core/World.h"
#include "core/WorldData.h"
#include "core/WorldDiagramGeneratorEmoji.h"
#include "core/organisms/TreeManager.h"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace DirtSim;

class TreeGerminationTest : public ::testing::Test {
protected:
    void SetUp() override { world = std::make_unique<World>(9, 9); }

    void setupGerminationWorld()
    {
        for (uint32_t y = 0; y < 9; ++y) {
            for (uint32_t x = 0; x < 9; ++x) {
                world->getData().at(x, y) = Cell();
            }
        }

        // Dirt at bottom 3 rows (same relative height as before).
        for (uint32_t y = 6; y < 9; ++y) {
            for (uint32_t x = 0; x < 9; ++x) {
                world->addMaterialAtCell(x, y, MaterialType::DIRT, 1.0);
            }
        }
    }

    std::unique_ptr<World> world;
};

TEST_F(TreeGerminationTest, SeedFallsOntoGround)
{
    setupGerminationWorld();
    TreeId id = world->getTreeManager().plantSeed(*world, 4, 1);

    EXPECT_EQ(world->getData().at(4, 1).material_type, MaterialType::SEED);

    std::cout << "Initial state:\n"
              << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

    double last_print = 0.0;
    for (int i = 0; i < 100; i++) {
        world->advanceTime(0.016);

        if (world->getData().timestep * 0.016 - last_print >= 1.0) {
            std::cout << "After " << (world->getData().timestep * 0.016) << " seconds:\n"
                      << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";
            last_print = world->getData().timestep * 0.016;
        }
    }

    const Tree* tree = world->getTreeManager().getTree(id);
    ASSERT_NE(tree, nullptr);
    EXPECT_GT(tree->seed_position.y, 1);
}

TEST_F(TreeGerminationTest, SeedGerminates)
{
    setupGerminationWorld();
    TreeId id = world->getTreeManager().plantSeed(*world, 4, 4);

    const Tree* tree = world->getTreeManager().getTree(id);
    EXPECT_EQ(tree->stage, GrowthStage::SEED);

    std::cout << "Initial state:\n"
              << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

    int frame = 0;
    while (tree->stage != GrowthStage::SAPLING && tree->age_seconds < 10.0) {
        world->advanceTime(0.016);
        frame++;

        if (frame % 10 == 0) {
            std::cout << "Frame " << frame << " (" << tree->age_seconds << "s):\n"
                      << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";
        }
    }

    std::cout << "Final state (frame " << frame << "):\n"
              << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

    EXPECT_EQ(tree->stage, GrowthStage::SAPLING);
}

TEST_F(TreeGerminationTest, SeedBlockedByWall)
{
    for (uint32_t y = 0; y < 9; ++y) {
        for (uint32_t x = 0; x < 9; ++x) {
            world->getData().at(x, y).replaceMaterial(MaterialType::WALL, 1.0);
        }
    }

    world->getData().at(4, 4).replaceMaterial(MaterialType::AIR, 0.0);

    TreeId id = world->getTreeManager().plantSeed(*world, 4, 4);
    const Tree* tree = world->getTreeManager().getTree(id);

    for (int i = 0; i < 1000; i++) {
        world->advanceTime(0.016);
    }

    EXPECT_EQ(tree->stage, GrowthStage::SEED);
}

TEST_F(TreeGerminationTest, SaplingGrowsBalanced)
{
    setupGerminationWorld();
    TreeId id = world->getTreeManager().plantSeed(*world, 4, 4);
    const Tree* tree = world->getTreeManager().getTree(id);

    std::cout << "Initial state (Seed at: " << tree->seed_position.x << ", "
              << tree->seed_position.y << "):\n"
              << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

    Vector2i last_seed_pos = tree->seed_position;
    std::string last_diagram = WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world);

    for (int i = 0; i < 2000; i++) {
        world->advanceTime(0.016);

        // Track seed movement.
        Vector2i current_seed_pos = tree->seed_position;
        if (current_seed_pos.x != last_seed_pos.x || current_seed_pos.y != last_seed_pos.y) {
            std::cout << "\n⚠️  SEED MOVED at frame " << i << " (t=" << tree->age_seconds << "s)\n";
            std::cout << "FROM: (" << last_seed_pos.x << ", " << last_seed_pos.y << ")\n";
            std::cout << "TO:   (" << current_seed_pos.x << ", " << current_seed_pos.y << ")\n\n";
            std::cout << "BEFORE (frame " << (i - 1) << "):\n" << last_diagram << "\n";
            std::cout << "AFTER (frame " << i << "):\n"
                      << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

            last_seed_pos = current_seed_pos;
        }

        // Save diagram for next iteration.
        last_diagram = WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world);

        // Print every 50 frames for detailed view.
        if (i % 50 == 0 && i > 0) {
            std::cout << "After " << (i * 0.016) << "s (Energy: " << tree->total_energy
                      << ", Cells: " << tree->cells.size() << ", Seed: " << tree->seed_position.x
                      << ", " << tree->seed_position.y << "):\n"
                      << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";
        }
    }

    std::cout << "Final state (Energy: " << tree->total_energy << ", Cells: " << tree->cells.size()
              << ", Seed at: (" << tree->seed_position.x << ", " << tree->seed_position.y << ")):\n"
              << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

    EXPECT_EQ(tree->stage, GrowthStage::SAPLING);
    EXPECT_GT(tree->cells.size(), 3);

    // Verify spatial balance: count materials left vs right of seed.
    int seed_x = tree->seed_position.x;
    std::cout << "\nSeed final position: (" << tree->seed_position.x << ", "
              << tree->seed_position.y << ")\n";

    int wood_left = 0, wood_right = 0;
    int leaf_left = 0, leaf_right = 0;

    for (uint32_t y = 0; y < 7; ++y) {
        for (uint32_t x = 0; x < 7; ++x) {
            const Cell& cell = world->getData().at(x, y);
            if (cell.organism_id != tree->id) continue;

            if (cell.material_type == MaterialType::WOOD) {
                if (static_cast<int>(x) < seed_x)
                    wood_left++;
                else if (static_cast<int>(x) > seed_x)
                    wood_right++;
            }
            else if (cell.material_type == MaterialType::LEAF) {
                if (static_cast<int>(x) < seed_x)
                    leaf_left++;
                else if (static_cast<int>(x) > seed_x)
                    leaf_right++;
            }
        }
    }

    std::cout << "\nSpatial Balance Check:\n";
    std::cout << "  WOOD: left=" << wood_left << ", right=" << wood_right << "\n";
    std::cout << "  LEAF: left=" << leaf_left << ", right=" << leaf_right << "\n";

    // Verify growth is balanced (accept 2:3 ratio as balanced for small trees).
    if (wood_left > 0 && wood_right > 0) {
        double wood_ratio = static_cast<double>(std::min(wood_left, wood_right))
            / static_cast<double>(std::max(wood_left, wood_right));
        std::cout << "  WOOD balance ratio: " << wood_ratio << " (should be >= 0.5)\n";
        EXPECT_GE(wood_ratio, 0.5) << "WOOD growth should be reasonably balanced (1:2 or better)";
    }

    if (leaf_left > 0 && leaf_right > 0) {
        double leaf_ratio = static_cast<double>(std::min(leaf_left, leaf_right))
            / static_cast<double>(std::max(leaf_left, leaf_right));
        std::cout << "  LEAF balance ratio: " << leaf_ratio << " (should be >= 0.5)\n";
        EXPECT_GE(leaf_ratio, 0.5) << "LEAF growth should be reasonably balanced (1:2 or better)";
    }
}

TEST_F(TreeGerminationTest, RootsStopAtWater)
{
    world->getPhysicsSettings().swap_enabled = false;

    for (uint32_t y = 0; y < 9; ++y) {
        for (uint32_t x = 0; x < 9; ++x) {
            world->getData().at(x, y) = Cell();
        }
    }

    // Water at bottom 2 rows.
    for (uint32_t y = 7; y < 9; ++y) {
        for (uint32_t x = 0; x < 9; ++x) {
            world->getData().at(x, y).replaceMaterial(MaterialType::WATER, 1.0);
        }
    }

    // Dirt layer above water.
    for (uint32_t x = 0; x < 9; ++x) {
        world->getData().at(x, 6).replaceMaterial(MaterialType::DIRT, 1.0);
    }

    std::cout << "Initial water test setup:\n"
              << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

    world->getTreeManager().plantSeed(*world, 4, 4);

    for (int i = 0; i < 2000; i++) {
        world->advanceTime(0.016);
        if (i % 500 == 0) {
            std::cout << "Frame " << i << ":\n"
                      << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";
        }
    }

    std::cout << "Final water test state:\n"
              << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

    int root_count = 0;
    int water_count = 0;
    for (uint32_t y = 0; y < 9; ++y) {
        for (uint32_t x = 0; x < 9; ++x) {
            if (world->getData().at(x, y).material_type == MaterialType::ROOT) root_count++;
            if (world->getData().at(x, y).material_type == MaterialType::WATER) water_count++;
        }
    }

    EXPECT_GE(root_count, 1);
    EXPECT_GE(water_count, 10);
}

TEST_F(TreeGerminationTest, TreeStopsGrowingWhenOutOfEnergy)
{
    setupGerminationWorld();
    TreeId id = world->getTreeManager().plantSeed(*world, 4, 4);
    Tree* tree = world->getTreeManager().getTree(id);

    const double initial_energy = 25.0;
    tree->total_energy = initial_energy;

    for (int i = 0; i < 3000; i++) {
        world->advanceTime(0.016);
    }

    // With 25.0 energy and trunk/branch growth model:
    // - SEED (starting cell, no cost)
    // - ROOT (12.0) → 13.0 remaining
    // - WOOD from germination (10.0) → 3.0 remaining
    // - Can't afford another WOOD (10.0) or ROOT (12.0)
    // Expected: 3 cells (SEED + ROOT + WOOD), 3.0 energy remaining.

    EXPECT_EQ(tree->cells.size(), 3) << "Tree should have SEED + ROOT + WOOD (25.0 energy limit)";
    EXPECT_DOUBLE_EQ(tree->total_energy, 3.0)
        << "Should have 3.0 energy remaining after germination";
}

TEST_F(TreeGerminationTest, WoodCellsStayStationary)
{
    setupGerminationWorld();
    TreeId id = world->getTreeManager().plantSeed(*world, 4, 4);
    const Tree* tree = world->getTreeManager().getTree(id);

    std::cout << "Initial state:\n"
              << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

    // Run until we have at least 2 WOOD cells.
    std::vector<Vector2i> wood_positions;
    int frame = 0;
    bool found_second_wood = false;

    while (!found_second_wood && tree->age_seconds < 20.0) {
        world->advanceTime(0.016);
        frame++;

        // Track all WOOD cells.
        wood_positions.clear();
        for (uint32_t y = 0; y < 9; ++y) {
            for (uint32_t x = 0; x < 9; ++x) {
                const Cell& cell = world->getData().at(x, y);
                if (cell.material_type == MaterialType::WOOD && cell.organism_id == tree->id) {
                    wood_positions.push_back(Vector2i{ static_cast<int>(x), static_cast<int>(y) });
                }
            }
        }

        if (wood_positions.size() >= 2) {
            found_second_wood = true;
            std::cout << "Frame " << frame << " (" << tree->age_seconds << "s): Found "
                      << wood_positions.size() << " WOOD cells:\n";
            for (size_t i = 0; i < wood_positions.size(); i++) {
                std::cout << "  WOOD[" << i << "] at (" << wood_positions[i].x << ", "
                          << wood_positions[i].y << ")\n";
            }
            std::cout << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";
        }
    }

    ASSERT_TRUE(found_second_wood) << "Tree should grow at least 2 WOOD cells";
    ASSERT_GE(wood_positions.size(), 2);

    // Save second WOOD position.
    Vector2i second_wood_pos = wood_positions[1];
    std::cout << "\nTracking WOOD[1] at (" << second_wood_pos.x << ", " << second_wood_pos.y
              << ")\n\n";

    // Run for another 100 frames and verify second WOOD cell doesn't move.
    for (int i = 0; i < 100; i++) {
        world->advanceTime(0.016);
        frame++;

        const Cell& cell = world->getData().at(second_wood_pos.x, second_wood_pos.y);

        if ((frame - 1) % 20 == 0) {
            std::cout << "Frame " << frame << " (" << tree->age_seconds << "s):\n";
            std::cout << "  WOOD[1] at (" << second_wood_pos.x << ", " << second_wood_pos.y
                      << "): material=" << getMaterialName(cell.material_type)
                      << ", fill=" << cell.fill_ratio << ", organism_id=" << cell.organism_id
                      << "\n";
            std::cout << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";
        }

        EXPECT_EQ(cell.material_type, MaterialType::WOOD)
            << "Frame " << frame << ": WOOD cell at (" << second_wood_pos.x << ", "
            << second_wood_pos.y << ") changed to " << getMaterialName(cell.material_type);
        EXPECT_EQ(cell.organism_id, tree->id)
            << "Frame " << frame << ": WOOD cell lost organism_id";
    }

    std::cout << "Final state (frame " << frame << "):\n"
              << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";
}

TEST_F(TreeGerminationTest, DebugWoodFalling)
{
    setupGerminationWorld();
    TreeId id = world->getTreeManager().plantSeed(*world, 4, 4);
    const Tree* tree = world->getTreeManager().getTree(id);

    std::cout << "=== DEEP DEBUG: Wood Cell Physics ===\n\n";
    std::cout << "Initial state:\n"
              << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

    // Run until we have 2 WOOD cells.
    std::vector<Vector2i> wood_positions;
    int frame = 0;
    bool found_second_wood = false;

    while (!found_second_wood && tree->age_seconds < 20.0) {
        world->advanceTime(0.016);
        frame++;

        wood_positions.clear();
        for (uint32_t y = 0; y < 9; ++y) {
            for (uint32_t x = 0; x < 9; ++x) {
                const Cell& cell = world->getData().at(x, y);
                if (cell.material_type == MaterialType::WOOD && cell.organism_id == tree->id) {
                    wood_positions.push_back(Vector2i{ static_cast<int>(x), static_cast<int>(y) });
                }
            }
        }

        if (wood_positions.size() >= 2) {
            found_second_wood = true;
            std::cout << "\n=== Frame " << frame << ": Found 2 WOOD cells ===\n";
            for (size_t i = 0; i < wood_positions.size(); i++) {
                std::cout << "  WOOD[" << i << "] at (" << wood_positions[i].x << ", "
                          << wood_positions[i].y << ")\n";
            }
        }
    }

    ASSERT_TRUE(found_second_wood);

    // Track both wood cells in detail for 50 frames.
    Vector2i wood0_pos = wood_positions[0];
    Vector2i wood1_pos = wood_positions[1];

    std::cout << "\n=== Detailed Tracking ===\n";
    std::cout << "WOOD[0] (first/center): (" << wood0_pos.x << ", " << wood0_pos.y << ")\n";
    std::cout << "WOOD[1] (second/left):  (" << wood1_pos.x << ", " << wood1_pos.y << ")\n";
    std::cout << "Initial Seed position: (" << tree->seed_position.x << ", "
              << tree->seed_position.y << ")\n\n";

    Vector2i last_seed_pos = tree->seed_position;

    for (int i = 0; i < 50; i++) {
        world->advanceTime(0.016);
        frame++;

        // Get current cell data.
        const Cell& wood0 = world->getData().at(wood0_pos.x, wood0_pos.y);
        const Cell& wood1 = world->getData().at(wood1_pos.x, wood1_pos.y);

        // Check for seed movement.
        Vector2i current_seed_pos = tree->seed_position;
        bool seed_moved =
            (current_seed_pos.x != last_seed_pos.x) || (current_seed_pos.y != last_seed_pos.y);

        // Print every 5 frames.
        if (i % 5 == 0) {
            std::cout << "\n━━━ Frame " << frame << " (t=" << tree->age_seconds << "s) ━━━\n";
            if (seed_moved) {
                std::cout << "⚠️  SEED MOVED: (" << last_seed_pos.x << ", " << last_seed_pos.y
                          << ") → (" << current_seed_pos.x << ", " << current_seed_pos.y << ")\n";
            }
            std::cout << "Seed position: (" << tree->seed_position.x << ", "
                      << tree->seed_position.y << ")\n";
            std::cout << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";

            // WOOD[0] details.
            std::cout << "WOOD[0] at (" << wood0_pos.x << ", " << wood0_pos.y << "):\n";
            std::cout << "  material: " << getMaterialName(wood0.material_type) << "\n";
            std::cout << "  fill_ratio: " << wood0.fill_ratio << "\n";
            std::cout << "  organism_id: " << wood0.organism_id << "\n";
            std::cout << "  com: (" << wood0.com.x << ", " << wood0.com.y << ")\n";
            std::cout << "  velocity: (" << wood0.velocity.x << ", " << wood0.velocity.y << ")\n";
            std::cout << "  pressure: " << wood0.pressure
                      << " (hydro: " << wood0.hydrostatic_component
                      << ", dyn: " << wood0.dynamic_component << ")\n";
            std::cout << "  pressure_gradient: (" << wood0.pressure_gradient.x << ", "
                      << wood0.pressure_gradient.y << ")\n";
            std::cout << "  pending_force: (" << wood0.pending_force.x << ", "
                      << wood0.pending_force.y << ")\n";
            std::cout << "  has_any_support: " << wood0.has_any_support << "\n";
            std::cout << "  has_vertical_support: " << wood0.has_vertical_support << "\n";

            // WOOD[1] details.
            std::cout << "WOOD[1] at (" << wood1_pos.x << ", " << wood1_pos.y << "):\n";
            std::cout << "  material: " << getMaterialName(wood1.material_type) << "\n";
            std::cout << "  fill_ratio: " << wood1.fill_ratio << "\n";
            std::cout << "  organism_id: " << wood1.organism_id << "\n";
            std::cout << "  com: (" << wood1.com.x << ", " << wood1.com.y << ")\n";
            std::cout << "  velocity: (" << wood1.velocity.x << ", " << wood1.velocity.y << ")\n";
            std::cout << "  pressure: " << wood1.pressure
                      << " (hydro: " << wood1.hydrostatic_component
                      << ", dyn: " << wood1.dynamic_component << ")\n";
            std::cout << "  pressure_gradient: (" << wood1.pressure_gradient.x << ", "
                      << wood1.pressure_gradient.y << ")\n";
            std::cout << "  pending_force: (" << wood1.pending_force.x << ", "
                      << wood1.pending_force.y << ")\n";
            std::cout << "  has_any_support: " << wood1.has_any_support << "\n";
            std::cout << "  has_vertical_support: " << wood1.has_vertical_support << "\n";

            // Show SEED details if it moved.
            if (seed_moved) {
                const Cell& seed_cell = world->getData().at(current_seed_pos.x, current_seed_pos.y);
                std::cout << "SEED at (" << current_seed_pos.x << ", " << current_seed_pos.y
                          << "):\n";
                std::cout << "  com: (" << seed_cell.com.x << ", " << seed_cell.com.y << ")\n";
                std::cout << "  velocity: (" << seed_cell.velocity.x << ", " << seed_cell.velocity.y
                          << ")\n";
                std::cout << "  has_any_support: " << seed_cell.has_any_support << "\n";
                std::cout << "  has_vertical_support: " << seed_cell.has_vertical_support << "\n";
                last_seed_pos = current_seed_pos;
            }

            // Check if WOOD[1] moved.
            bool wood1_still_there =
                world->getData().at(wood1_pos.x, wood1_pos.y).material_type == MaterialType::WOOD
                && world->getData().at(wood1_pos.x, wood1_pos.y).organism_id == tree->id;

            if (!wood1_still_there) {
                std::cout << "\n⚠️  WOOD[1] MOVED FROM (" << wood1_pos.x << ", " << wood1_pos.y
                          << ")!\n";
                // Find where it went.
                for (uint32_t y = 0; y < 7; ++y) {
                    for (uint32_t x = 0; x < 7; ++x) {
                        const Cell& cell = world->getData().at(x, y);
                        if (cell.material_type == MaterialType::WOOD && cell.organism_id == tree->id
                            && !(
                                static_cast<int>(x) == wood0_pos.x
                                && static_cast<int>(y) == wood0_pos.y)) {
                            std::cout << "Found WOOD[1] at new position: (" << x << ", " << y
                                      << ")\n";
                            wood1_pos = Vector2i{ static_cast<int>(x), static_cast<int>(y) };
                            break;
                        }
                    }
                }
            }
        }
    }

    std::cout << "\n=== Final State ===\n";
    std::cout << WorldDiagramGeneratorEmoji::generateEmojiDiagram(*world) << "\n";
}
