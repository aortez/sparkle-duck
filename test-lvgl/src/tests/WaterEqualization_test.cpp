#include "core/Cell.h"
#include "core/LoggingChannels.h"
#include "core/MaterialType.h"
#include "core/World.h"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace DirtSim;

/**
 * @brief Test fixture for water equalization tests.
 *
 * Tests hydrostatic pressure-driven flow through openings to verify
 * that water can flow horizontally and upward to equalize between columns.
 */
class WaterEqualizationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Initialize logging channels.
        LoggingChannels::initialize(spdlog::level::info, spdlog::level::debug);

        // Suppress swap logging noise (not relevant for pressure flow analysis).
        LoggingChannels::swap()->set_level(spdlog::level::off);

        // Create 3x6 world for water equalization test.
        world = std::make_unique<World>(3, 6);

        // Configure physics for hydrostatic pressure demonstration.
        world->physicsSettings.gravity = 9.81;
        world->physicsSettings.pressure_dynamic_enabled = false;
        world->physicsSettings.pressure_dynamic_strength = 0.0;
        world->physicsSettings.pressure_hydrostatic_enabled = true;
        world->physicsSettings.pressure_hydrostatic_strength = 0.3;
        world->physicsSettings.pressure_diffusion_strength = 1.0;
        world->physicsSettings.pressure_scale = 1.0;
        world->physicsSettings.swap_enabled = true;

        world->setWallsEnabled(false);
        world->setLeftThrowEnabled(false);
        world->setRightThrowEnabled(false);
        world->setLowerRightQuadrantEnabled(false);
    }

    void TearDown() override { world.reset(); }

    /**
     * @brief Set up the U-tube configuration.
     *
     * Creates:
     * - Left column (x=0): Full water column (6 cells)
     * - Middle column (x=1): Wall with bottom cell open
     * - Right column (x=2): Empty
     */
    void setupUTube()
    {
        // Left column: fill with water.
        for (uint32_t y = 0; y < 6; y++) {
            world->addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
        }

        // Middle column: wall barrier with bottom cell open for flow.
        for (uint32_t y = 0; y < 5; y++) { // Only y=0 to y=4 (leave y=5 open).
            world->addMaterialAtCell(1, y, MaterialType::WALL, 1.0);
        }
        // Bottom cell at (1, 5) is left empty for water to flow through.

        // Right column: empty (air) - no need to explicitly set.
    }

    /**
     * @brief Count water cells in a column.
     */
    uint32_t countWaterInColumn(uint32_t x) const
    {
        uint32_t count = 0;
        for (uint32_t y = 0; y < world->data.height; ++y) {
            const Cell& cell = world->at(x, y);
            if (cell.material_type == MaterialType::WATER && cell.fill_ratio > 0.5) {
                count++;
            }
        }
        return count;
    }

    /**
     * @brief Print world state for debugging.
     */
    void printWorld() const
    {
        spdlog::info("World state:");
        for (uint32_t y = 0; y < world->data.height; ++y) {
            std::string row = "  y=" + std::to_string(y) + ": ";
            for (uint32_t x = 0; x < world->data.width; ++x) {
                const Cell& cell = world->at(x, y);
                if (cell.material_type == MaterialType::WATER) {
                    row += "[W]";
                }
                else if (cell.material_type == MaterialType::WALL) {
                    row += "[#]";
                }
                else {
                    row += "[ ]";
                }
            }
            spdlog::info("{}", row);
        }
    }

    std::unique_ptr<World> world;
};

/**
 * @brief Test that water flows from left column through opening to right column.
 *
 * Expected behavior:
 * - Left column starts with 6 water cells.
 * - Right column starts with 0 water cells.
 * - After sufficient simulation steps, water should flow through bottom opening.
 * - Eventually both columns should have approximately 3 cells each (equalized).
 */
TEST_F(WaterEqualizationTest, WaterFlowsThroughOpening)
{
    spdlog::info("Starting WaterEqualizationTest::WaterFlowsThroughOpening");

    // Setup U-tube configuration.
    setupUTube();

    spdlog::info("Initial state:");
    printWorld();

    uint32_t left_initial = countWaterInColumn(0);
    uint32_t right_initial = countWaterInColumn(2);

    spdlog::info("Initial water counts - Left: {}, Right: {}", left_initial, right_initial);

    EXPECT_EQ(left_initial, 6);
    EXPECT_EQ(right_initial, 0);

    // Run simulation for many steps to allow equalization.
    const double deltaTime = 0.016; // ~60 FPS.
    const int num_steps = 1000;     // Run for ~16 seconds of simulation time.

    for (int step = 0; step < num_steps; ++step) {
        world->advanceTime(deltaTime);

        // Log progress every 100 steps.
        if (step % 100 == 0) {
            uint32_t left = countWaterInColumn(0);
            uint32_t right = countWaterInColumn(2);
            spdlog::info("Step {}: Left: {}, Right: {}", step, left, right);

            // Log detailed state of bottom row after it settles.
            if (step >= 100) {
                const Cell& cell_0_5 = world->at(0, 5);
                const Cell& cell_1_5 = world->at(1, 5);
                const Cell& cell_2_5 = world->at(2, 5);
                spdlog::info(
                    "  Cell(0,5): pressure={:.3f}, gradient=({:.3f},{:.3f})",
                    cell_0_5.pressure,
                    cell_0_5.pressure_gradient.x,
                    cell_0_5.pressure_gradient.y);
                spdlog::info(
                    "  Cell(1,5): pressure={:.3f}, gradient=({:.3f},{:.3f})",
                    cell_1_5.pressure,
                    cell_1_5.pressure_gradient.x,
                    cell_1_5.pressure_gradient.y);
                spdlog::info(
                    "  Cell(2,5): pressure={:.3f}, gradient=({:.3f},{:.3f}), vel=({:.3f},{:.3f})",
                    cell_2_5.pressure,
                    cell_2_5.pressure_gradient.x,
                    cell_2_5.pressure_gradient.y,
                    cell_2_5.velocity.x,
                    cell_2_5.velocity.y);
            }
        }
    }

    spdlog::info("Final state:");
    printWorld();

    uint32_t left_final = countWaterInColumn(0);
    uint32_t right_final = countWaterInColumn(2);

    spdlog::info("Final water counts - Left: {}, Right: {}", left_final, right_final);

    // Verify that water has moved from left to right.
    EXPECT_LT(left_final, left_initial) << "Water should have left the left column";
    EXPECT_GT(right_final, right_initial) << "Water should have entered the right column";

    // Verify approximate equalization (within 1 cell tolerance).
    // Expected: both columns around 3 cells each.
    EXPECT_NEAR(left_final, 3, 1) << "Left column should have ~3 cells";
    EXPECT_NEAR(right_final, 3, 1) << "Right column should have ~3 cells";

    // Total water should be conserved (6 cells total).
    uint32_t total = left_final + right_final;
    EXPECT_NEAR(total, 6, 1) << "Total water should be conserved";
}
