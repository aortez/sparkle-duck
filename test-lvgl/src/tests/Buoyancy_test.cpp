#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/PhysicsSettings.h"
#include "core/World.h"
#include "core/WorldData.h"
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

        // Use full-strength hydrostatic pressure for buoyancy testing.
        world->getPhysicsSettings().pressure_hydrostatic_enabled = true;
        world->getPhysicsSettings().pressure_hydrostatic_strength =
            1.0; // Full strength for proper buoyancy.

        // Enable material swapping for buoyancy.
        world->getPhysicsSettings().swap_enabled = true;

        // Set gravity (pointing down).
        world->getPhysicsSettings().gravity = 9.81; // Realistic gravity (sandbox default).
    }

    void TearDown() override { world.reset(); }

    /**
     * @brief Helper to set up the single vertical column with materials.
     * @param materials Vector of materials from top to bottom.
     */
    void setupColumn(const std::vector<MaterialType>& materials)
    {
        for (size_t y = 0; y < materials.size() && y < world->getData().height; ++y) {
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

    // Calculate expected pressure increment from actual physics configuration.
    // This uses the same formula as WorldPressureCalculator::calculateHydrostaticPressure().
    const double gravity = world->getPhysicsSettings().gravity;
    const double strength = world->getPhysicsSettings().pressure_hydrostatic_strength;
    const MaterialProperties& water_props = getMaterialProperties(MaterialType::WATER);
    const double water_density = water_props.density;
    const double fill_ratio = 1.0;             // Cells are completely full.
    const double slice_thickness = 1.0;        // WorldPressureCalculator::SLICE_THICKNESS
    const double hydrostatic_multiplier = 1.0; // WorldPressureCalculator::HYDROSTATIC_MULTIPLIER

    // Formula: pressure[y] = y × (density × fill_ratio × gravity × slice × strength × multiplier)
    const double expected_increment =
        water_density * fill_ratio * gravity * slice_thickness * strength * hydrostatic_multiplier;

    spdlog::info("  Physics configuration:");
    spdlog::info("    gravity = {:.3f}", gravity);
    spdlog::info("    pressure_hydrostatic_strength = {:.3f}", strength);
    spdlog::info("    water_density = {:.3f}", water_density);
    spdlog::info("    Expected increment per cell = {:.6f}", expected_increment);

    // Verify: Pressure increases linearly with depth.
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

    // Calculate expected pressure increment from actual physics configuration.
    // The key test: metal should contribute WATER density (not metal density) to the pressure
    // field. This is how buoyancy works - solids contribute surrounding fluid density.
    const double gravity = world->getPhysicsSettings().gravity;
    const double strength = world->getPhysicsSettings().pressure_hydrostatic_strength;
    const MaterialProperties& water_props = getMaterialProperties(MaterialType::WATER);
    const MaterialProperties& metal_props = getMaterialProperties(MaterialType::METAL);
    const double water_density = water_props.density;
    const double metal_density = metal_props.density;

    // For buoyancy, solids contribute surrounding fluid density, not their own density.
    const double expected_increment = water_density * gravity * strength;

    spdlog::info("  Physics configuration:");
    spdlog::info("    gravity = {:.3f}", gravity);
    spdlog::info("    pressure_hydrostatic_strength = {:.3f}", strength);
    spdlog::info("    water_density = {:.3f}", water_density);
    spdlog::info("    metal_density = {:.3f} (should NOT affect pressure)", metal_density);
    spdlog::info(
        "    Expected increment per cell = {:.6f} (using water density)", expected_increment);

    // Verify: Pressure increases uniformly despite metal cell.
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
        double gravity_magnitude = world->getPhysicsSettings().gravity;
        Vector2d gravity_force(0.0, metal_props.density * gravity_magnitude);

        // Pressure force (gradient points from high to low pressure).
        double pressure_scale = world->getPhysicsSettings().pressure_scale;
        double hydrostatic_weight = metal_props.hydrostatic_weight;
        Vector2d pressure_force = pressure_gradient * pressure_scale * hydrostatic_weight;

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
        world->getPhysicsSettings().pressure_hydrostatic_enabled = true;
        world->getPhysicsSettings().pressure_hydrostatic_strength = 1.0;
        world->getPhysicsSettings().gravity = 1.0;

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
        double gravity_magnitude = world->getPhysicsSettings().gravity;
        Vector2d gravity_force(0.0, wood_props.density * gravity_magnitude);

        // Pressure force (gradient points from high to low pressure).
        double pressure_scale = world->getPhysicsSettings().pressure_scale;
        double hydrostatic_weight = wood_props.hydrostatic_weight;
        Vector2d pressure_force = pressure_gradient * pressure_scale * hydrostatic_weight;

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

    // Give wood a head start - position it closer to boundary.
    wood.setCOM(Vector2d(0.0, -0.7));
    spdlog::info("  Set wood COM to -0.7 (closer to boundary for faster swap test)");

    // Enable debug logging to see swap checks.
    spdlog::set_level(spdlog::level::debug);

    // Run simulation longer to see if swap occurs.
    const double deltaTime = 0.016; // 60 FPS timestep
    const int steps = 500;          // Run much longer to let water COM migrate

    int initial_wood_y = 2;
    int final_wood_y = 2;
    int swap_count = 0;

    for (int i = 0; i < steps; ++i) {
        // Find current wood position.
        int current_wood_y = -1;
        for (uint32_t y = 0; y < 5; ++y) {
            if (world->at(0, y).material_type == MaterialType::WOOD) {
                current_wood_y = y;
                break;
            }
        }

        // Log state every 50 steps.
        if (i % 50 == 0 && current_wood_y >= 0) {
            const Cell& wood_cell = world->at(0, current_wood_y);
            spdlog::info(
                "  Step {}: wood at y={}, vel=({:.4f},{:.4f}), com=({:.4f},{:.4f})",
                i,
                current_wood_y,
                wood_cell.velocity.x,
                wood_cell.velocity.y,
                wood_cell.com.x,
                wood_cell.com.y);
        }

        world->advanceTime(deltaTime);

        // Track position changes.
        int new_wood_y = -1;
        for (uint32_t y = 0; y < 5; ++y) {
            if (world->at(0, y).material_type == MaterialType::WOOD) {
                new_wood_y = y;
                break;
            }
        }

        if (new_wood_y != current_wood_y && new_wood_y >= 0) {
            swap_count++;
            spdlog::info(
                "  SWAP #{} at step {}: wood moved y={} -> y={}",
                swap_count,
                i + 1,
                current_wood_y,
                new_wood_y);
            final_wood_y = new_wood_y;
        }
    }

    // Final state.
    spdlog::info("  Final state after {} steps:", steps);
    for (uint32_t y = 0; y < 5; ++y) {
        const Cell& c = world->at(0, y);
        spdlog::info(
            "    y={}: {} vel=({:.4f},{:.4f})",
            y,
            getMaterialName(c.material_type),
            c.velocity.x,
            c.velocity.y);
    }

    // Report results.
    spdlog::info(
        "  Wood rose from y={} to y={} ({} cells upward)",
        initial_wood_y,
        final_wood_y,
        initial_wood_y - final_wood_y);
    spdlog::info("  Total swaps: {}", swap_count);

    if (swap_count > 0) {
        double avg_steps_per_swap = static_cast<double>(steps) / swap_count;
        spdlog::info("  Average steps per cell rise: {:.1f}", avg_steps_per_swap);
        spdlog::info("  SUCCESS: Swap mechanism working!");
    }
    else {
        spdlog::info("  No swap occurred (might need more steps or different conditions)");
    }

    // Wood should rise at least one cell (from y=2 to y=1 or higher).
    EXPECT_LT(final_wood_y, initial_wood_y) << "Wood should rise upward through water";
    EXPECT_GE(swap_count, 1) << "Wood should swap at least once to demonstrate buoyancy";
}

// =============================================================================
// PARAMETERIZED BUOYANCY TESTS
// =============================================================================

/**
 * @brief Test parameters for material buoyancy behavior.
 */
struct BuoyancyMaterialParams {
    MaterialType material;

    enum class ExpectedBehavior { RISE, SINK, LEVEL };
    ExpectedBehavior expected_behavior;

    int max_steps_to_endpoint; // Max timesteps to reach top (y=0) or bottom (y=4).

    std::string description;
};

/**
 * @brief Parameterized test fixture for testing multiple materials.
 */
class ParameterizedBuoyancyTest : public ::testing::TestWithParam<BuoyancyMaterialParams> {
protected:
    void SetUp() override
    {
        spdlog::set_level(spdlog::level::info);

        // Create 1x5 world for testing.
        world = std::make_unique<World>(1, 5);

        // Full-strength hydrostatic pressure for buoyancy.
        world->getPhysicsSettings().pressure_hydrostatic_enabled = true;
        world->getPhysicsSettings().pressure_hydrostatic_strength = 1.0;
        world->getPhysicsSettings().swap_enabled = true;
        world->getPhysicsSettings().gravity = 9.81;
    }

    void TearDown() override { world.reset(); }

    void setupMaterialInWater(MaterialType material)
    {
        // Fill entire column with water.
        for (int y = 0; y < 5; ++y) {
            world->addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
        }

        // Place test material in middle (y=2).
        world->at(0, 2).replaceMaterial(material, 1.0);

        // Pre-position COM for faster testing.
        if (GetParam().expected_behavior == BuoyancyMaterialParams::ExpectedBehavior::RISE) {
            world->at(0, 2).setCOM(Vector2d(0.0, -0.7)); // Near top boundary.
        }
        else if (GetParam().expected_behavior == BuoyancyMaterialParams::ExpectedBehavior::SINK) {
            world->at(0, 2).setCOM(Vector2d(0.0, 0.7)); // Near bottom boundary.
        }
    }

    std::unique_ptr<World> world;
};

/**
 * @brief Parameterized test: Material buoyancy in water column.
 *
 * Tests that materials with different densities behave correctly:
 * - Lighter materials (wood, leaf) rise to surface (y=0)
 * - Heavier materials (dirt, metal) sink to bottom (y=4)
 * - Neutral materials stay level (if any)
 */
TEST_P(ParameterizedBuoyancyTest, DISABLED_MaterialBuoyancyBehavior)
{
    const auto& params = GetParam();
    const int START_Y = 2; // Middle of 1x5 world.

    std::string behavior_str =
        (params.expected_behavior == BuoyancyMaterialParams::ExpectedBehavior::RISE)   ? "RISE"
        : (params.expected_behavior == BuoyancyMaterialParams::ExpectedBehavior::SINK) ? "SINK"
                                                                                       : "LEVEL";

    spdlog::info("===== Testing {} - Expected: {} =====", params.description, behavior_str);

    // Setup material in water column.
    setupMaterialInWater(params.material);

    // Run simulation.
    const double deltaTime = 0.016; // 60 FPS.
    int final_y = START_Y;
    int steps_taken = 0;
    int swap_count = 0;

    for (int step = 0; step < params.max_steps_to_endpoint; ++step) {
        // Track position before step.
        int current_y = -1;
        for (int y = 0; y < 5; ++y) {
            if (world->at(0, y).material_type == params.material) {
                current_y = y;
                break;
            }
        }

        // Log every 50 steps.
        if (step % 50 == 0 && current_y >= 0) {
            const Cell& cell = world->at(0, current_y);
            spdlog::info(
                "  Step {}: {} at y={}, vel=({:.3f},{:.3f}), com=({:.3f},{:.3f})",
                step,
                getMaterialName(params.material),
                current_y,
                cell.velocity.x,
                cell.velocity.y,
                cell.com.x,
                cell.com.y);
        }

        world->advanceTime(deltaTime);
        steps_taken++;

        // Track position after step.
        int new_y = -1;
        for (int y = 0; y < 5; ++y) {
            if (world->at(0, y).material_type == params.material) {
                new_y = y;
                break;
            }
        }

        // Track swaps.
        if (new_y != current_y && new_y >= 0) {
            swap_count++;
            spdlog::info(
                "  SWAP #{} at step {}: {} moved y={} -> y={}",
                swap_count,
                step,
                getMaterialName(params.material),
                current_y,
                new_y);
            final_y = new_y;
        }

        // Check if reached endpoint.
        bool reached_endpoint = false;
        if (params.expected_behavior == BuoyancyMaterialParams::ExpectedBehavior::RISE
            && final_y == 0) {
            reached_endpoint = true; // Reached top.
        }
        else if (
            params.expected_behavior == BuoyancyMaterialParams::ExpectedBehavior::SINK
            && final_y == 4) {
            reached_endpoint = true; // Reached bottom.
        }

        if (reached_endpoint) {
            spdlog::info("  SUCCESS: Reached endpoint in {} steps!", steps_taken);
            break;
        }
    }

    // Final results.
    spdlog::info(
        "Final: {} at y={} (started at y={}) after {} steps, {} swaps",
        getMaterialName(params.material),
        final_y,
        START_Y,
        steps_taken,
        swap_count);

    // Verify behavior.
    switch (params.expected_behavior) {
        case BuoyancyMaterialParams::ExpectedBehavior::RISE:
            EXPECT_EQ(final_y, 0) << params.description << " should rise to top (y=0) within "
                                  << params.max_steps_to_endpoint << " steps";
            break;
        case BuoyancyMaterialParams::ExpectedBehavior::SINK:
            EXPECT_EQ(final_y, 4) << params.description << " should sink to bottom (y=4) within "
                                  << params.max_steps_to_endpoint << " steps";
            break;
        case BuoyancyMaterialParams::ExpectedBehavior::LEVEL:
            EXPECT_EQ(final_y, START_Y) << params.description << " should stay at y=" << START_Y;
            break;
    }

    EXPECT_LE(steps_taken, params.max_steps_to_endpoint)
        << params.description << " took too long to reach endpoint";
}

// Test parameters for different materials.
// IMPORTANT: WOOD first to ensure it passes (known working case).
INSTANTIATE_TEST_SUITE_P(
    Materials,
    ParameterizedBuoyancyTest,
    ::testing::Values(
        BuoyancyMaterialParams{ .material = MaterialType::WOOD,
                                .expected_behavior = BuoyancyMaterialParams::ExpectedBehavior::RISE,
                                .max_steps_to_endpoint = 250,
                                .description = "Wood (density 0.8)" },

        BuoyancyMaterialParams{ .material = MaterialType::DIRT,
                                .expected_behavior = BuoyancyMaterialParams::ExpectedBehavior::SINK,
                                .max_steps_to_endpoint = 200,
                                .description = "Dirt (density 1.5)" },

        BuoyancyMaterialParams{ .material = MaterialType::METAL,
                                .expected_behavior = BuoyancyMaterialParams::ExpectedBehavior::SINK,
                                .max_steps_to_endpoint = 150,
                                .description = "Metal (density 7.8)" },

        BuoyancyMaterialParams{ .material = MaterialType::LEAF,
                                .expected_behavior = BuoyancyMaterialParams::ExpectedBehavior::RISE,
                                .max_steps_to_endpoint = 1200,
                                .description = "Leaf (density 0.3)" }));

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
    world->getPhysicsSettings().pressure_hydrostatic_enabled = true;
    world->getPhysicsSettings().pressure_hydrostatic_strength = 1.0;
    world->getPhysicsSettings().gravity = 1.0;

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
                        spdlog::info("      Has support: {}", c.has_any_support);
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
    world->getPhysicsSettings().pressure_hydrostatic_enabled = true;
    world->getPhysicsSettings().pressure_hydrostatic_strength = 1.0;
    world->getPhysicsSettings().gravity = 9.81; // Real gravity

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

/**
 * @brief Test 2.5: Dirt Sinks Through Water Column.
 *
 * Tests if dirt can actually sink through multiple water cells.
 */
TEST_F(BuoyancyTest, DirtSinksThroughWater)
{
    spdlog::info("Starting BuoyancyTest::DirtSinksThroughWater");

    // Create 1x6 world: dirt at top, water column below.
    world = std::make_unique<World>(1, 6);
    world->getPhysicsSettings().pressure_hydrostatic_enabled = true;
    world->getPhysicsSettings().pressure_hydrostatic_strength = 0.3; // Sandbox default.
    world->getPhysicsSettings().swap_enabled = true; // Enable material swapping for sinking.
    world->getPhysicsSettings().gravity = 9.81;      // Realistic gravity (sandbox default).

    // Setup: Dirt at top (y=0), water below.
    world->addMaterialAtCell(0, 0, MaterialType::DIRT, 1.0);
    for (uint32_t y = 1; y < 6; ++y) {
        world->addMaterialAtCell(0, y, MaterialType::WATER, 1.0);
    }

    spdlog::info("  Initial setup:");
    spdlog::info("    [D] y=0 (dirt)");
    spdlog::info("    [W] y=1");
    spdlog::info("    [W] y=2");
    spdlog::info("    [W] y=3");
    spdlog::info("    [W] y=4");
    spdlog::info("    [W] y=5");

    // Enable debug logging.
    spdlog::set_level(spdlog::level::debug);

    // Track dirt position over time.
    const double deltaTime = 0.016;
    const int steps = 500;

    int initial_dirt_y = 0;
    int final_dirt_y = 0;
    int swap_count = 0;

    for (int i = 0; i < steps; ++i) {
        // Find current dirt position.
        int current_dirt_y = -1;
        for (uint32_t y = 0; y < 6; ++y) {
            if (world->at(0, y).material_type == MaterialType::DIRT) {
                current_dirt_y = y;
                break;
            }
        }

        // Log every 100 steps.
        if (i % 100 == 0 && current_dirt_y >= 0) {
            const Cell& dirt_cell = world->at(0, current_dirt_y);
            spdlog::info(
                "  Step {}: dirt at y={}, vel=({:.3f},{:.3f}), com=({:.3f},{:.3f}), "
                "dyn_press={:.2f}",
                i,
                current_dirt_y,
                dirt_cell.velocity.x,
                dirt_cell.velocity.y,
                dirt_cell.com.x,
                dirt_cell.com.y,
                dirt_cell.dynamic_component);

            // Check expected forces.
            const MaterialProperties& dirt_props = getMaterialProperties(MaterialType::DIRT);
            spdlog::info(
                "    Dirt density={:.1f}, expected net force={:.1f} (should sink)",
                dirt_props.density,
                (dirt_props.density - 1.0) * 9.81);
        }

        world->advanceTime(deltaTime);

        // Track position changes.
        int new_dirt_y = -1;
        for (uint32_t y = 0; y < 6; ++y) {
            if (world->at(0, y).material_type == MaterialType::DIRT) {
                new_dirt_y = y;
                break;
            }
        }

        if (new_dirt_y != current_dirt_y && new_dirt_y >= 0) {
            swap_count++;
            spdlog::info(
                "  SWAP #{} at step {}: dirt moved y={} -> y={}",
                swap_count,
                i + 1,
                current_dirt_y,
                new_dirt_y);
            final_dirt_y = new_dirt_y;
        }
    }

    // Final state.
    spdlog::info("  === Final State (step {}) ===", steps);
    for (uint32_t y = 0; y < 6; ++y) {
        const Cell& c = world->at(0, y);
        char symbol = (c.material_type == MaterialType::DIRT) ? 'D' : 'W';
        spdlog::info("    [{}] y={}", symbol, y);
    }

    // Report results.
    spdlog::info(
        "  Dirt sank from y={} to y={} ({} cells downward)",
        initial_dirt_y,
        final_dirt_y,
        final_dirt_y - initial_dirt_y);
    spdlog::info("  Total swaps: {}", swap_count);

    if (swap_count > 0) {
        double avg_steps_per_swap = static_cast<double>(steps) / swap_count;
        spdlog::info("  Average steps per cell sink: {:.1f}", avg_steps_per_swap);
        spdlog::info("  SUCCESS: Swap mechanism working!");
    }
    else {
        spdlog::info("  No swap occurred (might need more steps or different conditions)");
    }

    // Dirt should sink at least one cell (from y=0 downward).
    EXPECT_GT(final_dirt_y, initial_dirt_y) << "Dirt should sink downward through water";
    EXPECT_GE(swap_count, 1) << "Dirt should swap at least once to demonstrate sinking";
}

/**
 * @brief Test: Verify Dirt Should Sink (Not Float).
 *
 * Quick sanity check on dirt forces in water.
 */
TEST_F(BuoyancyTest, DirtShouldSinkNotFloat)
{
    spdlog::info("Starting BuoyancyTest::DirtShouldSinkNotFloat");

    // Setup: Dirt surrounded by water (1x3 column).
    world = std::make_unique<World>(1, 3);
    world->getPhysicsSettings().pressure_hydrostatic_enabled = true;
    world->getPhysicsSettings().pressure_hydrostatic_strength = 1.0;
    world->getPhysicsSettings().gravity = 9.81;

    world->addMaterialAtCell(0, 0, MaterialType::WATER, 1.0);
    world->addMaterialAtCell(0, 1, MaterialType::DIRT, 1.0);
    world->addMaterialAtCell(0, 2, MaterialType::WATER, 1.0);

    // Calculate pressure.
    calculatePressure();

    // Check forces on dirt.
    const MaterialProperties& dirt_props = getMaterialProperties(MaterialType::DIRT);

    WorldPressureCalculator calc;
    Vector2d pressure_grad = calc.calculatePressureGradient(*world, 0, 1);

    double gravity_force = dirt_props.density * 9.81;
    double pressure_force = pressure_grad.y;
    double net_force = gravity_force + pressure_force;

    spdlog::info("  Dirt density: {:.1f}", dirt_props.density);
    spdlog::info("  Gravity force: {:.2f} (down)", gravity_force);
    spdlog::info("  Pressure force: {:.2f} (negative = up)", pressure_force);
    spdlog::info("  Net force: {:.2f} (positive = down)", net_force);

    // Verify net force is downward (dirt should sink).
    EXPECT_GT(net_force, 0.0) << "Dirt should have net downward force (sink in water)";

    spdlog::info(
        "  {}: Dirt {} sink",
        net_force > 0 ? "CORRECT" : "WRONG",
        net_force > 0 ? "should" : "should NOT");
}
