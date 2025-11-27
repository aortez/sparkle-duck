#include "core/Cell.h"
#include "core/LoggingChannels.h"
#include "core/MaterialType.h"
#include "core/PhysicsSettings.h"
#include "core/World.h"
#include "core/WorldData.h"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace DirtSim;

/**
 * @brief Test fixture for diagonal water leveling tests.
 *
 * Tests that water initially placed in a diagonal pattern (bottom-left triangle)
 * eventually levels out to a flat bottom layer due to pressure equalization.
 */
class DiagonalWaterLevelingTest : public ::testing::Test {
protected:
    uint32_t interior_size_ = 10; // Default 10x10 interior.

    void SetUp() override
    {
        // Initialize logging channels.
        LoggingChannels::initialize(spdlog::level::info, spdlog::level::debug);

        // Suppress swap logging noise.
        LoggingChannels::swap()->set_level(spdlog::level::off);
    }

    void TearDown() override { world.reset(); }

    /**
     * @brief Create world with specified interior size.
     */
    void createWorld(uint32_t interior_size)
    {
        interior_size_ = interior_size;
        uint32_t world_size = interior_size + 2; // Add wall border.

        world = std::make_unique<World>(world_size, world_size);

        // Configure physics for pressure-driven leveling.
        world->getPhysicsSettings().gravity = 9.81;
        world->getPhysicsSettings().pressure_dynamic_enabled = true;
        world->getPhysicsSettings().pressure_dynamic_strength = 1.0;
        world->getPhysicsSettings().pressure_hydrostatic_enabled = true;
        world->getPhysicsSettings().pressure_hydrostatic_strength = 0.3;
        world->getPhysicsSettings().pressure_diffusion_strength = 5.0;
        world->getPhysicsSettings().pressure_scale = 1.0;
        world->getPhysicsSettings().swap_enabled = true;

        world->setWallsEnabled(false);
        world->setLeftThrowEnabled(false);
        world->setRightThrowEnabled(false);
        world->setLowerRightQuadrantEnabled(false);
    }

    /**
     * @brief Set up the container with wall border.
     */
    void setupWallBorder()
    {
        uint32_t world_size = interior_size_ + 2;
        uint32_t last = world_size - 1;

        // Top and bottom walls.
        for (uint32_t x = 0; x < world_size; ++x) {
            world->addMaterialAtCell(x, 0, MaterialType::WALL, 1.0);
            world->addMaterialAtCell(x, last, MaterialType::WALL, 1.0);
        }
        // Left and right walls.
        for (uint32_t y = 0; y < world_size; ++y) {
            world->addMaterialAtCell(0, y, MaterialType::WALL, 1.0);
            world->addMaterialAtCell(last, y, MaterialType::WALL, 1.0);
        }
    }

    /**
     * @brief Fill the interior diagonally with water.
     *
     * Fills bottom-left triangle where y > x (in interior coordinates).
     */
    void setupDiagonalWater()
    {
        uint32_t water_count = 0;
        for (uint32_t x = 1; x <= interior_size_; ++x) {
            for (uint32_t y = 1; y <= interior_size_; ++y) {
                // Convert to interior coordinates for diagonal check.
                uint32_t interior_x = x - 1;
                uint32_t interior_y = y - 1;

                // Fill bottom-left triangle: y > x (below diagonal).
                if (interior_y > interior_x) {
                    world->addMaterialAtCell(x, y, MaterialType::WATER, 1.0);
                    ++water_count;
                }
            }
        }
        spdlog::info("Placed {} water cells in diagonal pattern", water_count);
    }

    /**
     * @brief Count water cells in the interior.
     */
    uint32_t countTotalWater() const
    {
        uint32_t count = 0;
        for (uint32_t x = 1; x <= interior_size_; ++x) {
            for (uint32_t y = 1; y <= interior_size_; ++y) {
                const Cell& cell = world->getData().at(x, y);
                if (cell.material_type == MaterialType::WATER && cell.fill_ratio > 0.5) {
                    ++count;
                }
            }
        }
        return count;
    }

    /**
     * @brief Count water cells in a specific row (interior y coordinate).
     */
    uint32_t countWaterInRow(uint32_t interior_y) const
    {
        uint32_t count = 0;
        uint32_t y = interior_y + 1; // Convert to world coordinate.
        for (uint32_t x = 1; x <= interior_size_; ++x) {
            const Cell& cell = world->getData().at(x, y);
            if (cell.material_type == MaterialType::WATER && cell.fill_ratio > 0.5) {
                ++count;
            }
        }
        return count;
    }

    /**
     * @brief Calculate variance of water distribution across columns.
     *
     * Lower variance = more level water.
     */
    double calculateLevelVariance() const
    {
        std::vector<uint32_t> column_heights(interior_size_, 0);

        // Count water cells in each column.
        for (uint32_t x = 1; x <= interior_size_; ++x) {
            for (uint32_t y = 1; y <= interior_size_; ++y) {
                const Cell& cell = world->getData().at(x, y);
                if (cell.material_type == MaterialType::WATER && cell.fill_ratio > 0.5) {
                    ++column_heights[x - 1];
                }
            }
        }

        // Calculate mean height.
        double sum = 0;
        for (auto h : column_heights) {
            sum += h;
        }
        double mean = sum / static_cast<double>(interior_size_);

        // Calculate variance.
        double variance = 0;
        for (auto h : column_heights) {
            double diff = h - mean;
            variance += diff * diff;
        }
        variance /= static_cast<double>(interior_size_);

        return variance;
    }

    /**
     * @brief Print world state with ASCII art (compact for large worlds).
     */
    void printWorld() const
    {
        uint32_t world_size = interior_size_ + 2;
        spdlog::info("World state ({}x{} interior):", interior_size_, interior_size_);

        // For large worlds, only print first/last few rows.
        bool compact = world_size > 20;
        for (uint32_t y = 0; y < world_size; ++y) {
            if (compact && y > 5 && y < world_size - 6) {
                if (y == 6) {
                    spdlog::info("  ... ({} rows omitted) ...", world_size - 12);
                }
                continue;
            }
            std::string row = "  y=" + std::to_string(y) + ": ";
            for (uint32_t x = 0; x < world_size; ++x) {
                if (compact && x > 10 && x < world_size - 11) {
                    if (x == 11) {
                        row += "...";
                    }
                    continue;
                }
                const Cell& cell = world->getData().at(x, y);
                if (cell.material_type == MaterialType::WATER) {
                    row += "~";
                }
                else if (cell.material_type == MaterialType::WALL) {
                    row += "#";
                }
                else if (cell.material_type == MaterialType::DIRT) {
                    row += "@";
                }
                else {
                    row += ".";
                }
            }
            spdlog::info("{}", row);
        }
    }

    std::unique_ptr<World> world;
};

/**
 * @brief Test that diagonal water levels out to a flat bottom layer (10x10).
 *
 * Initial state: Water fills bottom-left triangle (45 cells in 10x10 = 45%).
 * Expected: Water levels out to fill approximately the bottom 4-5 rows uniformly.
 */
TEST_F(DiagonalWaterLevelingTest, DiagonalWaterLevelsOut_10x10)
{
    spdlog::info("Starting DiagonalWaterLevelingTest::DiagonalWaterLevelsOut_10x10");

    createWorld(10);
    setupWallBorder();
    setupDiagonalWater();

    spdlog::info("Initial state:");
    printWorld();

    uint32_t initial_water = countTotalWater();
    double initial_variance = calculateLevelVariance();

    spdlog::info("Initial water count: {}, variance: {:.2f}", initial_water, initial_variance);

    // The diagonal pattern should have high variance (uneven column heights).
    EXPECT_GT(initial_variance, 5.0) << "Initial diagonal pattern should have high variance";

    // Run simulation to allow leveling.
    const double deltaTime = 0.016; // ~60 FPS.
    const int num_steps = 2000;     // Run for ~32 seconds of simulation time.

    for (int step = 0; step < num_steps; ++step) {
        world->advanceTime(deltaTime);

        // Log progress periodically.
        if (step % 500 == 0) {
            double variance = calculateLevelVariance();
            spdlog::info("Step {}: water={}, variance={:.2f}", step, countTotalWater(), variance);

            if (step >= 500) {
                printWorld();
            }
        }
    }

    spdlog::info("Final state:");
    printWorld();

    uint32_t final_water = countTotalWater();
    double final_variance = calculateLevelVariance();

    spdlog::info("Final water count: {}, variance: {:.2f}", final_water, final_variance);

    // Verify water conservation.
    EXPECT_NEAR(final_water, initial_water, 2) << "Water should be conserved";

    // Verify water has leveled out (low variance).
    EXPECT_LT(final_variance, 2.0) << "Final variance should be low (water leveled)";

    // Verify bottom rows are mostly full.
    uint32_t bottom_row_water = countWaterInRow(interior_size_ - 1);
    EXPECT_GE(bottom_row_water, interior_size_ - 2) << "Bottom row should be mostly filled";
}

/**
 * @brief Test that diagonal water levels out to a flat bottom layer (50x50).
 *
 * Initial state: Water fills bottom-left triangle (1225 cells in 50x50 = 49%).
 * Expected: Water levels out to fill approximately the bottom 24-25 rows uniformly.
 */
TEST_F(DiagonalWaterLevelingTest, DiagonalWaterLevelsOut_50x50)
{
    spdlog::info("Starting DiagonalWaterLevelingTest::DiagonalWaterLevelsOut_50x50");

    createWorld(50);
    setupWallBorder();
    setupDiagonalWater();

    spdlog::info("Initial state:");
    printWorld();

    uint32_t initial_water = countTotalWater();
    double initial_variance = calculateLevelVariance();

    spdlog::info("Initial water count: {}, variance: {:.2f}", initial_water, initial_variance);

    // The diagonal pattern should have high variance (uneven column heights).
    EXPECT_GT(initial_variance, 100.0) << "Initial diagonal pattern should have high variance";

    // Run simulation to allow leveling.
    // Larger world needs more steps for pressure to propagate.
    const double deltaTime = 0.016; // ~60 FPS.
    const int num_steps = 10000;    // Run for ~160 seconds of simulation time.

    for (int step = 0; step < num_steps; ++step) {
        world->advanceTime(deltaTime);

        // Log progress periodically.
        if (step % 2000 == 0) {
            double variance = calculateLevelVariance();
            spdlog::info("Step {}: water={}, variance={:.2f}", step, countTotalWater(), variance);

            if (step >= 2000) {
                printWorld();
            }
        }
    }

    spdlog::info("Final state:");
    printWorld();

    uint32_t final_water = countTotalWater();
    double final_variance = calculateLevelVariance();

    spdlog::info("Final water count: {}, variance: {:.2f}", final_water, final_variance);

    // Verify water conservation (allow small tolerance for larger world).
    EXPECT_NEAR(final_water, initial_water, 10) << "Water should be conserved";

    // Verify water has leveled out (low variance).
    // Larger world may have more variance, so use proportionally larger threshold.
    EXPECT_LT(final_variance, 10.0) << "Final variance should be low (water leveled)";

    // Verify bottom rows are mostly full.
    uint32_t bottom_row_water = countWaterInRow(interior_size_ - 1);
    EXPECT_GE(bottom_row_water, interior_size_ - 5) << "Bottom row should be mostly filled";
}
