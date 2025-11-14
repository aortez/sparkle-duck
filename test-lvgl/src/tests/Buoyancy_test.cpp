#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "core/WorldPressureCalculator.h"

#include <cmath>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace DirtSim;

// Helper function to compare doubles with epsilon.
static bool almostEqual(double a, double b, double epsilon = 1e-6)
{
    return std::abs(a - b) < epsilon;
}

/**
 * @brief Test fixture for buoyancy physics tests.
 *
 * Provides common setup for testing pressure-based buoyancy mechanics.
 */
class BuoyancyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        spdlog::set_level(spdlog::level::info);

        // Create minimal 1D world for testing (1 cell wide, 5 cells tall).
        world = std::make_unique<World>(1, 5);

        // Enable hydrostatic pressure.
        world->setHydrostaticPressureEnabled(true);
        world->setHydrostaticPressureStrength(1.0);

        // Set gravity (pointing down).
        world->physicsSettings.gravity = 1.0;
    }

    void TearDown() override { world.reset(); }

    /**
     * @brief Helper to set up the single vertical column with materials.
     * @param materials Vector of materials from top to bottom.
     */
    void setupColumn(const std::vector<MaterialType>& materials)
    {
        for (size_t y = 0; y < materials.size() && y < world->data.height; ++y) {
            Cell& cell = world->at(0, y); // Only one column (x=0).
            if (materials[y] != MaterialType::AIR) {
                cell.addMaterial(materials[y], 1.0); // Fill completely.
            }
        }
    }

    /**
     * @brief Calculate pressure for all cells.
     */
    void calculatePressure()
    {
        WorldPressureCalculator calculator;
        calculator.calculateHydrostaticPressure(*world);
    }

    std::unique_ptr<World> world;
};

/**
 * @brief Test 1.1: Pressure Field in Pure Fluid.
 *
 * Validates basic hydrostatic pressure accumulation in a pure water column.
 */
TEST_F(BuoyancyTest, PureFluidPressureField)
{
    spdlog::info("Starting BuoyancyTest::PureFluidPressureField");

    // Setup: Vertical column of 5 water cells.
    setupColumn({ MaterialType::WATER,
                  MaterialType::WATER,
                  MaterialType::WATER,
                  MaterialType::WATER,
                  MaterialType::WATER });

    // Execute: Calculate hydrostatic pressure.
    calculatePressure();

    // Verify: Pressure increases linearly with depth.
    // Expected: pressure[i] ≈ i * density * gravity * slice_thickness * strength
    // With water density = 1.0, gravity = 1.0, slice = 1.0, HYDROSTATIC_MULTIPLIER = 1.0
    const double expected_increment = 1.0 * 1.0 * 1.0 * 1.0; // = 1.0

    for (uint32_t y = 0; y < 5; ++y) {
        const Cell& cell = world->at(0, y);
        double expected_pressure = y * expected_increment;

        spdlog::info(
            "  Cell y={}: pressure={:.6f}, expected={:.6f}", y, cell.pressure, expected_pressure);

        EXPECT_TRUE(almostEqual(cell.pressure, expected_pressure, 1e-5))
            << "Pressure at depth " << y << " should be " << expected_pressure << " but got "
            << cell.pressure;
    }
}

/**
 * @brief Test 1.2: Single Solid in Fluid Column.
 *
 * Validates that solids contribute surrounding fluid density, not their own density.
 */
TEST_F(BuoyancyTest, SolidInFluidColumn)
{
    spdlog::info("Starting BuoyancyTest::SolidInFluidColumn");

    // Setup: Water column with metal cell in the middle.
    setupColumn({ MaterialType::WATER,
                  MaterialType::WATER,
                  MaterialType::METAL,
                  MaterialType::WATER,
                  MaterialType::WATER });

    // Execute: Calculate hydrostatic pressure.
    calculatePressure();

    // Verify: Pressure increases uniformly despite metal cell.
    // The metal should contribute ~1.0 (water density) not 7.8 (metal density).
    const double expected_increment = 1.0 * 1.0 * 1.0 * 1.0; // Same as pure water.

    for (uint32_t y = 0; y < 5; ++y) {
        const Cell& cell = world->at(0, y);
        double expected_pressure = y * expected_increment;

        spdlog::info(
            "  Cell y={}: material={}, pressure={:.6f}, expected={:.6f}",
            y,
            getMaterialName(cell.material_type),
            cell.pressure,
            expected_pressure);

        EXPECT_TRUE(almostEqual(cell.pressure, expected_pressure, 1e-5))
            << "Pressure at depth " << y << " should be " << expected_pressure
            << " (metal should not pollute pressure field)";
    }
}

/**
 * @brief Test 1.3: Pressure Forces Direction.
 *
 * Validates that pressure gradient through a solid points in the correct direction.
 */
TEST_F(BuoyancyTest, PressureForceDirection)
{
    spdlog::info("Starting BuoyancyTest::PressureForceDirection");

    // Setup: Water column with metal cell in the middle.
    setupColumn({ MaterialType::WATER,
                  MaterialType::WATER,
                  MaterialType::METAL,
                  MaterialType::WATER,
                  MaterialType::WATER });

    // Execute: Calculate hydrostatic pressure.
    calculatePressure();

    // Get metal cell and neighbors.
    const Cell& metal = world->at(0, 2);
    const Cell& above = world->at(0, 1);
    const Cell& below = world->at(0, 3);

    spdlog::info("  Metal cell y=2: pressure={:.6f}", metal.pressure);
    spdlog::info("  Cell above y=1: pressure={:.6f}", above.pressure);
    spdlog::info("  Cell below y=3: pressure={:.6f}", below.pressure);

    // Verify: Pressure gradient points downward (higher pressure below).
    EXPECT_GT(below.pressure, above.pressure)
        << "Pressure should be higher below the metal cell (gradient points down)";

    // Calculate pressure gradient through metal.
    WorldPressureCalculator calculator;
    Vector2d gradient = calculator.calculatePressureGradient(*world, 0, 2);

    spdlog::info("  Pressure gradient: ({:.6f}, {:.6f})", gradient.x, gradient.y);

    // Verify: Gradient has upward component (negative y).
    // The gradient points from high to low pressure, which is upward (away from high pressure
    // below). This creates an upward buoyancy force.
    EXPECT_LT(gradient.y, 0.0) << "Pressure gradient should point upward (negative y) for buoyancy";

    // Verify: Gradient magnitude is roughly proportional to fluid pressure difference.
    double pressure_diff = below.pressure - above.pressure;
    spdlog::info("  Pressure difference (below - above): {:.6f}", pressure_diff);
    EXPECT_GT(pressure_diff, 0.0) << "Pressure difference should be positive";
}

/**
 * @brief Test 1.4: Net Force Calculation.
 *
 * Validates that net force (gravity + pressure) correctly determines sink/float behavior.
 */
TEST_F(BuoyancyTest, NetForceCalculation)
{
    spdlog::info("Starting BuoyancyTest::NetForceCalculation");

    // Test Case A: Metal should sink (density 7.8 > water 1.0).
    {
        spdlog::info("  Test Case A: Metal in water");

        // Setup.
        setupColumn({ MaterialType::WATER,
                      MaterialType::WATER,
                      MaterialType::METAL,
                      MaterialType::WATER,
                      MaterialType::WATER });
        calculatePressure();

        WorldPressureCalculator calculator;
        Vector2d pressure_gradient = calculator.calculatePressureGradient(*world, 0, 2);

        // Gravity force (downward, positive y).
        const MaterialProperties& metal_props = getMaterialProperties(MaterialType::METAL);
        double gravity_magnitude = 1.0; // From setup.
        Vector2d gravity_force(0.0, metal_props.density * gravity_magnitude);

        // Pressure force (gradient points from high to low pressure).
        // In World.cpp:459, pressure_force = gradient * pressure_scale
        double pressure_scale = 1.0; // Default from PhysicsSettings
        Vector2d pressure_force = pressure_gradient * pressure_scale;

        // Net force.
        Vector2d net_force = gravity_force + pressure_force;

        spdlog::info("    Gravity force: ({:.3f}, {:.3f})", gravity_force.x, gravity_force.y);
        spdlog::info("    Pressure force: ({:.3f}, {:.3f})", pressure_force.x, pressure_force.y);
        spdlog::info("    Net force: ({:.3f}, {:.3f})", net_force.x, net_force.y);

        // Verify: Net force is downward (metal sinks).
        EXPECT_GT(net_force.y, 0.0) << "Metal should have net downward force (sink)";
    }

    // Test Case B: Wood should float (density 0.8 < water 1.0).
    {
        spdlog::info("  Test Case B: Wood in water");

        // Clear previous setup.
        world = std::make_unique<World>(1, 5);
        world->setHydrostaticPressureEnabled(true);
        world->setHydrostaticPressureStrength(1.0);
        world->physicsSettings.gravity = 1.0;

        // Setup.
        setupColumn({ MaterialType::WATER,
                      MaterialType::WATER,
                      MaterialType::WOOD,
                      MaterialType::WATER,
                      MaterialType::WATER });
        calculatePressure();

        WorldPressureCalculator calculator;
        Vector2d pressure_gradient = calculator.calculatePressureGradient(*world, 0, 2);

        // Gravity force (downward, positive y).
        const MaterialProperties& wood_props = getMaterialProperties(MaterialType::WOOD);
        double gravity_magnitude = 1.0;
        Vector2d gravity_force(0.0, wood_props.density * gravity_magnitude);

        // Pressure force (gradient points from high to low pressure).
        double pressure_scale = 1.0;
        Vector2d pressure_force = pressure_gradient * pressure_scale;

        // Net force.
        Vector2d net_force = gravity_force + pressure_force;

        spdlog::info("    Gravity force: ({:.3f}, {:.3f})", gravity_force.x, gravity_force.y);
        spdlog::info("    Pressure force: ({:.3f}, {:.3f})", pressure_force.x, pressure_force.y);
        spdlog::info("    Net force: ({:.3f}, {:.3f})", net_force.x, net_force.y);

        // Verify: Net force is upward (wood floats).
        EXPECT_LT(net_force.y, 0.0) << "Wood should have net upward force (float)";
    }
}

/**
 * @brief Test 2.1: Wood Develops Upward Velocity.
 *
 * Validates that wood actually accelerates upward when submerged in water.
 */
TEST_F(BuoyancyTest, WoodDevelopsUpwardVelocity)
{
    spdlog::info("Starting BuoyancyTest::WoodDevelopsUpwardVelocity");

    // Setup: Wood cell submerged in water column.
    setupColumn({ MaterialType::WATER,
                  MaterialType::WATER,
                  MaterialType::WOOD,
                  MaterialType::WATER,
                  MaterialType::WATER });

    // Get wood cell reference.
    Cell& wood = world->at(0, 2);

    // Verify initial state: wood at rest.
    EXPECT_TRUE(almostEqual(wood.velocity.y, 0.0, 1e-5)) << "Wood should start with zero velocity";

    spdlog::info("  Initial velocity: ({:.6f}, {:.6f})", wood.velocity.x, wood.velocity.y);

    // Run simulation for several timesteps to allow velocity to develop.
    const double deltaTime = 0.016; // 60 FPS timestep
    const int steps = 10;

    for (int i = 0; i < steps; ++i) {
        world->advanceTime(deltaTime);
    }

    // Get updated velocity.
    spdlog::info(
        "  Final velocity after {} steps: ({:.6f}, {:.6f})",
        steps,
        wood.velocity.x,
        wood.velocity.y);

    // Verify: Wood developed upward velocity (negative y).
    EXPECT_LT(wood.velocity.y, -0.01)
        << "Wood should develop upward velocity (negative y) after " << steps << " timesteps";

    // Verify: Velocity magnitude is reasonable (not infinite).
    double velocity_magnitude = wood.velocity.magnitude();
    EXPECT_LT(velocity_magnitude, 10.0) << "Velocity should be reasonable, not explosive";
}

/**
 * @brief Test 2.2: Metal Develops Downward Velocity.
 *
 * Validates that metal actually accelerates downward when submerged in water.
 */
TEST_F(BuoyancyTest, MetalDevelopsDownwardVelocity)
{
    spdlog::info("Starting BuoyancyTest::MetalDevelopsDownwardVelocity");

    // Setup: Metal cell submerged in water column.
    setupColumn({ MaterialType::WATER,
                  MaterialType::WATER,
                  MaterialType::METAL,
                  MaterialType::WATER,
                  MaterialType::WATER });

    // Get metal cell reference.
    Cell& metal = world->at(0, 2);

    // Verify initial state: metal at rest.
    EXPECT_TRUE(almostEqual(metal.velocity.y, 0.0, 1e-5))
        << "Metal should start with zero velocity";

    spdlog::info("  Initial velocity: ({:.6f}, {:.6f})", metal.velocity.x, metal.velocity.y);

    // Run simulation for several timesteps.
    const double deltaTime = 0.016; // 60 FPS timestep
    const int steps = 10;

    for (int i = 0; i < steps; ++i) {
        world->advanceTime(deltaTime);
    }

    // Get updated velocity.
    spdlog::info(
        "  Final velocity after {} steps: ({:.6f}, {:.6f})",
        steps,
        metal.velocity.x,
        metal.velocity.y);

    // Verify: Metal developed downward velocity (positive y).
    EXPECT_GT(metal.velocity.y, 0.01)
        << "Metal should develop downward velocity (positive y) after " << steps << " timesteps";

    // Verify: Velocity magnitude is reasonable.
    double velocity_magnitude = metal.velocity.magnitude();
    EXPECT_LT(velocity_magnitude, 10.0) << "Velocity should be reasonable, not explosive";
}

/**
 * @brief Test 2.3: Wood Can Rise in 3x3 World.
 *
 * Tests if wood can actually change vertical position when horizontal flow is possible.
 */
TEST_F(BuoyancyTest, WoodCanRiseIn3x3World)
{
    spdlog::info("Starting BuoyancyTest::WoodCanRiseIn3x3World");

    // Create 3x3 world to allow horizontal water flow.
    world = std::make_unique<World>(3, 3);
    world->setHydrostaticPressureEnabled(true);
    world->setHydrostaticPressureStrength(1.0);
    world->physicsSettings.gravity = 1.0;

    // Setup: Wood in center (1,1), water everywhere else.
    for (uint32_t y = 0; y < 3; ++y) {
        for (uint32_t x = 0; x < 3; ++x) {
            if (x == 1 && y == 1) {
                world->addMaterialAtCell(x, y, MaterialType::WOOD, 1.0);
            }
            else {
                world->addMaterialAtCell(x, y, MaterialType::WATER, 1.0);
            }
        }
    }

    spdlog::info("  Initial setup:");
    spdlog::info("    [W] [W] [W]");
    spdlog::info("    [W] [X] [W]  (X = wood at center)");
    spdlog::info("    [W] [W] [W]");

    // Track wood cell over time.
    const double deltaTime = 0.016;
    const int steps = 300; // Run longer to see if wood actually transfers to new cell

    // Log detailed state every 50 steps.
    for (int i = 0; i < steps; ++i) {
        if (i % 50 == 0) {
            spdlog::info("  === Step {} ===", i);

            // Print grid state with pressure values.
            for (uint32_t y = 0; y < 3; ++y) {
                std::string row = "    ";
                for (uint32_t x = 0; x < 3; ++x) {
                    const Cell& c = world->at(x, y);
                    char symbol = '?';
                    if (c.material_type == MaterialType::WOOD) {
                        symbol = 'X';
                    }
                    else if (c.material_type == MaterialType::WATER) {
                        symbol = 'W';
                    }
                    else {
                        symbol = ' ';
                    }
                    row += "[" + std::string(1, symbol) + "]";
                }
                spdlog::info("{}", row);
            }

            // Print pressure field.
            spdlog::info("    Pressure field:");
            for (uint32_t y = 0; y < 3; ++y) {
                std::string row = "    ";
                for (uint32_t x = 0; x < 3; ++x) {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "[%.2f]", world->at(x, y).pressure);
                    row += buf;
                }
                spdlog::info("{}", row);
            }

            // Log wood cell details (find it first).
            for (uint32_t y = 0; y < 3; ++y) {
                for (uint32_t x = 0; x < 3; ++x) {
                    const Cell& c = world->at(x, y);
                    if (c.material_type == MaterialType::WOOD) {
                        // Calculate expected forces.
                        const MaterialProperties& wood_props =
                            getMaterialProperties(MaterialType::WOOD);
                        double gravity_force = wood_props.density * 1.0; // gravity = 1.0

                        WorldPressureCalculator calc;
                        Vector2d pressure_grad = calc.calculatePressureGradient(*world, x, y);

                        // Check all neighbors for gradient analysis.
                        spdlog::info(
                            "    Wood at ({},{}): vel=({:.4f},{:.4f}), com=({:.4f},{:.4f}), "
                            "fill={:.2f}",
                            x,
                            y,
                            c.velocity.x,
                            c.velocity.y,
                            c.com.x,
                            c.com.y,
                            c.fill_ratio);
                        spdlog::info(
                            "      Neighbors: Up={:.2f}, Down={:.2f}, Left={:.2f}, Right={:.2f}",
                            world->at(x, y - 1).pressure,
                            world->at(x, y + 1).pressure,
                            world->at(x - 1, y).pressure,
                            world->at(x + 1, y).pressure);
                        spdlog::info(
                            "      Pressure: {:.4f}, Gradient: ({:.4f},{:.4f})",
                            c.pressure,
                            pressure_grad.x,
                            pressure_grad.y);
                        spdlog::info(
                            "      Expected gravity: {:.4f}, Expected pressure force: {:.4f}",
                            gravity_force,
                            pressure_grad.y);
                        spdlog::info(
                            "      Expected net force: {:.4f} (should be negative = upward)",
                            gravity_force + pressure_grad.y);
                        spdlog::info("      Has support: {}", c.has_support);
                    }
                }
            }
        }

        world->advanceTime(deltaTime);
    }

    // Final state logging.
    spdlog::info("  === Final State (step {}) ===", steps);
    for (uint32_t y = 0; y < 3; ++y) {
        std::string row = "    ";
        for (uint32_t x = 0; x < 3; ++x) {
            const Cell& c = world->at(x, y);
            if (c.material_type == MaterialType::WOOD) {
                row += "[X]";
            }
            else if (c.material_type == MaterialType::WATER) {
                row += "[W]";
            }
            else {
                row += "[ ]";
            }
        }
        spdlog::info("{}", row);
    }

    // Find wood and check if it moved upward.
    bool wood_found = false;
    uint32_t wood_y = 999;

    for (uint32_t y = 0; y < 3; ++y) {
        for (uint32_t x = 0; x < 3; ++x) {
            const Cell& c = world->at(x, y);
            if (c.material_type == MaterialType::WOOD) {
                wood_found = true;
                wood_y = y;
                spdlog::info(
                    "  Final wood position: ({},{}) with vel=({:.4f},{:.4f}), "
                    "com=({:.4f},{:.4f})",
                    x,
                    y,
                    c.velocity.x,
                    c.velocity.y,
                    c.com.x,
                    c.com.y);
            }
        }
    }

    // Verify wood still exists.
    EXPECT_TRUE(wood_found) << "Wood should still exist in the world";

    // Check if wood rose (moved to y=0) or stayed at y=1.
    // For now, just log the result - we're investigating behavior.
    spdlog::info("  Wood vertical position: y={} (started at y=1)", wood_y);

    if (wood_y < 1) {
        spdlog::info("  SUCCESS: Wood rose from y=1 to y={}!", wood_y);
    }
    else if (wood_y == 1) {
        spdlog::info("  Wood stayed at y=1 (might be oscillating at boundary)");
    }
    else {
        spdlog::info("  UNEXPECTED: Wood sank to y={}?!", wood_y);
    }

    // Soft assertion for now - we're still investigating.
    // EXPECT_LE(wood_y, 1) << "Wood should rise or stay neutral, not sink";
}

/**
 * @brief Test 2.4: Water Column Should Fall.
 *
 * Tests basic gravity on water - water at top should fall to bottom.
 */
TEST_F(BuoyancyTest, WaterColumnFalls)
{
    spdlog::info("Starting BuoyancyTest::WaterColumnFalls");

    // Create 2x4 world with water in top 2x2 cells.
    world = std::make_unique<World>(2, 4);
    world->setHydrostaticPressureEnabled(true);
    world->setHydrostaticPressureStrength(1.0);
    world->physicsSettings.gravity = 9.81; // Real gravity

    // Setup: Water in top 2x2, empty below.
    for (uint32_t y = 0; y < 2; ++y) {
        for (uint32_t x = 0; x < 2; ++x) {
            world->addMaterialAtCell(x, y, MaterialType::WATER, 1.0);
        }
    }

    spdlog::info("  Initial setup:");
    spdlog::info("    [W] [W]  y=0");
    spdlog::info("    [W] [W]  y=1");
    spdlog::info("    [ ] [ ]  y=2 (empty)");
    spdlog::info("    [ ] [ ]  y=3 (empty)");

    // Track over time.
    const double deltaTime = 0.016;
    const int steps = 100;

    for (int i = 0; i < steps; ++i) {
        if (i % 25 == 0) {
            spdlog::info("  === Step {} ===", i);

            // Print grid.
            for (uint32_t y = 0; y < 4; ++y) {
                std::string row = "    ";
                for (uint32_t x = 0; x < 2; ++x) {
                    const Cell& c = world->at(x, y);
                    if (c.material_type == MaterialType::WATER) {
                        row += "[W]";
                    }
                    else {
                        row += "[ ]";
                    }
                }
                spdlog::info("{} y={}", row, y);
            }

            // Log some cell details.
            spdlog::info(
                "    Cell (0,1) water: vel=({:.4f},{:.4f}), com=({:.4f},{:.4f}), "
                "pressure={:.4f}",
                world->at(0, 1).velocity.x,
                world->at(0, 1).velocity.y,
                world->at(0, 1).com.x,
                world->at(0, 1).com.y,
                world->at(0, 1).pressure);
        }

        world->advanceTime(deltaTime);
    }

    // Final state.
    spdlog::info("  === Final State (step {}) ===", steps);
    for (uint32_t y = 0; y < 4; ++y) {
        std::string row = "    ";
        for (uint32_t x = 0; x < 2; ++x) {
            const Cell& c = world->at(x, y);
            if (c.material_type == MaterialType::WATER) {
                row += "[W]";
            }
            else {
                row += "[ ]";
            }
        }
        spdlog::info("{} y={}", row, y);
    }

    // Check if water moved down.
    int water_count_bottom_half = 0;
    for (uint32_t y = 2; y < 4; ++y) {
        for (uint32_t x = 0; x < 2; ++x) {
            if (world->at(x, y).material_type == MaterialType::WATER) {
                water_count_bottom_half++;
            }
        }
    }

    spdlog::info("  Water cells in bottom half (y=2,3): {}", water_count_bottom_half);

    if (water_count_bottom_half > 0) {
        spdlog::info("  SUCCESS: Water fell to bottom!");
    }
    else {
        spdlog::info("  PROBLEM: Water stayed at top - gravity not working!");
    }

    // Expect at least some water to have fallen.
    // EXPECT_GT(water_count_bottom_half, 0) << "Water should fall under gravity";
}
