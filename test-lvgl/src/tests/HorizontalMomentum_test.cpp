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
 * @brief Test fixture for horizontal momentum conservation.
 *
 * Tests that material moving horizontally through AIR cells maintains
 * constant velocity (when air resistance is disabled).
 */
class HorizontalMomentumTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Initialize logging channels.
        LoggingChannels::initialize(spdlog::level::info, spdlog::level::debug);

        // Enable swap logging at debug level to see all swap decisions.
        LoggingChannels::swap()->set_level(spdlog::level::debug);

        // Create 7x5 world for horizontal motion test (walls make usable area 5x3).
        world = std::make_unique<World>(7, 5);

        world->setWallsEnabled(true); // Need walls to contain the test.
        world->setLeftThrowEnabled(false);
        world->setRightThrowEnabled(false);
        world->setLowerRightQuadrantEnabled(false);

        // Disable only drag forces (keep cohesion/adhesion to verify they work correctly).
        world->getPhysicsSettings().air_resistance = 0.0;
        world->getPhysicsSettings().friction_strength = 0.0;
        world->getPhysicsSettings().viscosity_strength = 0.0;
        // cohesion_strength and adhesion_strength left at defaults to verify fix works.

        // Enable swaps for material movement through AIR.
        world->getPhysicsSettings().swap_enabled = true;

        // Disable gravity for pure horizontal test.
        world->getPhysicsSettings().gravity = 0.0;
    }

    void TearDown() override { world.reset(); }

    /**
     * @brief Print world state for debugging.
     */
    void printWorld(int step) const
    {
        spdlog::info("=== Step {} ===", step);
        for (uint32_t y = 0; y < world->getData().height; ++y) {
            std::string row = "  y=" + std::to_string(y) + ": ";
            for (uint32_t x = 0; x < world->getData().width; ++x) {
                const Cell& cell = world->getData().at(x, y);
                if (cell.material_type == MaterialType::DIRT) {
                    row += "[D]";
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
 * @brief Test that dirt maintains horizontal velocity when moving through AIR.
 *
 * Setup:
 * - 3x3 world with walls at boundaries (actually 5x5 with walls making usable 3x3)
 * - Dirt at middle-left (1,2) with horizontal velocity
 * - Air resistance disabled
 * - Gravity disabled
 *
 * Expected:
 * - Dirt should move right at constant velocity
 * - Each swap with AIR should preserve horizontal velocity
 * - Dirt should reach right wall with same velocity
 */
TEST_F(HorizontalMomentumTest, DirtMaintainsHorizontalVelocity)
{
    spdlog::info("Starting HorizontalMomentumTest::DirtMaintainsHorizontalVelocity");
    spdlog::info("  World: 5x3 (with walls = 7x5 total)");
    spdlog::info("  Air resistance: DISABLED");
    spdlog::info("  Gravity: DISABLED");

    // Place dirt near left side (x=2, y=2 in 7x5 grid), away from walls.
    // Walls are at x=0, x=6, y=0, y=4.
    const uint32_t startX = 2;
    const uint32_t startY = 2;

    world->addMaterialAtCell(startX, startY, MaterialType::DIRT, 1.0);

    // Give dirt horizontal velocity to the right.
    Cell& dirtCell = world->getData().at(startX, startY);
    dirtCell.velocity = Vector2d(2.0, 0.0); // 2.0 cells/second to the right.
    dirtCell.setCOM(0.5, 0.0);              // Start near right edge to trigger quick swap.

    spdlog::info("  Initial dirt position: ({}, {})", startX, startY);
    spdlog::info("  Initial velocity: ({:.3f}, {:.3f})", dirtCell.velocity.x, dirtCell.velocity.y);

    printWorld(0);

    // Track velocity history.
    std::vector<double> velocities;
    velocities.push_back(dirtCell.velocity.x);

    // Run simulation and track dirt movement.
    const double deltaTime = 0.016; // 16ms timestep.
    const int maxSteps = 100;
    int dirtX = startX;

    for (int step = 1; step <= maxSteps; ++step) {
        world->advanceTime(deltaTime);

        // Find dirt cell.
        bool found = false;
        for (uint32_t y = 0; y < world->getData().height; ++y) {
            for (uint32_t x = 0; x < world->getData().width; ++x) {
                const Cell& cell = world->getData().at(x, y);
                if (cell.material_type == MaterialType::DIRT && cell.fill_ratio > 0.5) {
                    dirtX = x;
                    found = true;

                    // Log comprehensive physics state.
                    if (step % 5 == 0 || step < 10 || step >= 18) {
                        // Calculate what newCOM would be for next frame.
                        Vector2d predictedCOM = cell.com + cell.velocity * 0.016;

                        spdlog::info("  Step {}: Dirt at ({}, {})", step, x, y);
                        spdlog::info(
                            "    vel=({:.3f}, {:.3f}) COM=({:.3f}, {:.3f}) predictedCOM=({:.3f}, "
                            "{:.3f})",
                            cell.velocity.x,
                            cell.velocity.y,
                            cell.com.x,
                            cell.com.y,
                            predictedCOM.x,
                            predictedCOM.y);
                        spdlog::info(
                            "    force=({:.3f}, {:.3f}) pressure_gradient=({:.3f}, {:.3f})",
                            cell.pending_force.x,
                            cell.pending_force.y,
                            cell.pressure_gradient.x,
                            cell.pressure_gradient.y);
                        spdlog::info(
                            "    pressure={:.3f} (hydro={:.3f}, dynamic={:.3f})",
                            cell.pressure,
                            cell.hydrostatic_component,
                            cell.dynamic_component);
                        spdlog::info(
                            "    support: any={}, vertical={}",
                            cell.has_any_support,
                            cell.has_vertical_support);
                        printWorld(step);
                    }

                    velocities.push_back(cell.velocity.x);
                    break;
                }
            }
            if (found) break;
        }

        ASSERT_TRUE(found) << "Dirt disappeared at step " << step;

        // Check if dirt reached the right wall (x=5, since wall is at x=6).
        if (dirtX >= 5) {
            spdlog::info("  Dirt reached right side at step {}", step);
            break;
        }
    }

    // Verify dirt moved to the right.
    EXPECT_GT(dirtX, startX) << "Dirt should have moved right";

    // Analyze velocity conservation.
    spdlog::info("\n=== Velocity Analysis ===");
    spdlog::info("  Initial velocity: {:.3f}", velocities.front());
    spdlog::info("  Final velocity: {:.3f}", velocities.back());

    double velocityChange = std::abs(velocities.back() - velocities.front());
    double percentChange = 100.0 * velocityChange / velocities.front();

    spdlog::info("  Velocity change: {:.3f} ({:.1f}%)", velocityChange, percentChange);

    // With air resistance disabled and no gravity, velocity should be nearly constant.
    // Allow 20% tolerance for swap energy costs.
    EXPECT_LT(percentChange, 20.0) << "Horizontal velocity should be mostly conserved (within 20%)";

    // Print all velocities for debugging.
    spdlog::info("\n=== Velocity History ===");
    for (size_t i = 0; i < velocities.size(); ++i) {
        spdlog::info("  Step {}: vel_x = {:.3f}", i, velocities[i]);
    }
}
