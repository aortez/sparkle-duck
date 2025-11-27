#include "core/Cell.h"
#include "core/MaterialType.h"
#include "core/World.h"
#include "core/WorldCohesionCalculator.h"

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace DirtSim;

class CohesionDebugTest : public ::testing::Test {
protected:
    void SetUp() override { spdlog::set_level(spdlog::level::info); }

    void TearDown() override { world.reset(); }

    std::unique_ptr<World> world;
};

/**
 * @brief Test cohesion in a simple 1x3 vertical column.
 */
TEST_F(CohesionDebugTest, OneByThreeColumn)
{
    spdlog::info("=== Testing 1x3 Vertical Column ===");

    // Create 1x3 world.
    world = std::make_unique<World>(1, 3);
    world->getPhysicsSettings().cohesion_strength = 150.0;

    // Fill all cells with dirt.
    for (uint32_t y = 0; y < 3; y++) {
        world->getData().at(0, y).replaceMaterial(MaterialType::DIRT, 1.0);
        world->getData().at(0, y).setCOM(0.0, 0.0); // Start centered.
    }

    // Calculate cohesion for each cell.
    WorldCohesionCalculator calc;
    for (uint32_t y = 0; y < 3; y++) {
        auto result = calc.calculateCOMCohesionForce(*world, 0, y, 1);

        spdlog::info(
            "Cell (0,{}): connections={}, force_mag={:.4f}, direction=({:.3f},{:.3f}), "
            "neighbor_center=({:.3f},{:.3f})",
            y,
            result.active_connections,
            result.force_magnitude,
            result.force_direction.x,
            result.force_direction.y,
            result.center_of_neighbors.x,
            result.center_of_neighbors.y);
    }

    // Expected:
    // Cell (0,0) - Top: 1 neighbor below → force points down
    // Cell (0,1) - Middle: 2 neighbors (above+below) → force balanced (0,0)?
    // Cell (0,2) - Bottom: 1 neighbor above → force points up
}

/**
 * @brief Test cohesion in a 3x3 grid with different patterns.
 */
TEST_F(CohesionDebugTest, ThreeByThreeGrid)
{
    spdlog::info("=== Testing 3x3 Grid - Full ===");

    // Create 3x3 world.
    world = std::make_unique<World>(3, 3);
    world->getPhysicsSettings().cohesion_strength = 150.0;

    // Fill all cells with dirt.
    for (uint32_t y = 0; y < 3; y++) {
        for (uint32_t x = 0; x < 3; x++) {
            world->getData().at(x, y).replaceMaterial(MaterialType::DIRT, 1.0);
            world->getData().at(x, y).setCOM(0.0, 0.0); // Start centered.
        }
    }

    // Calculate cohesion for center cell.
    WorldCohesionCalculator calc;
    auto center_result = calc.calculateCOMCohesionForce(*world, 1, 1, 1);

    spdlog::info(
        "Center cell (1,1): connections={}, force_mag={:.4f}, direction=({:.3f},{:.3f})",
        center_result.active_connections,
        center_result.force_magnitude,
        center_result.force_direction.x,
        center_result.force_direction.y);

    // Expected: 8 neighbors, all equal distance → force should be ~0 (balanced)

    // Calculate cohesion for corner cell.
    auto corner_result = calc.calculateCOMCohesionForce(*world, 0, 0, 1);

    spdlog::info(
        "Corner cell (0,0): connections={}, force_mag={:.4f}, direction=({:.3f},{:.3f})",
        corner_result.active_connections,
        corner_result.force_magnitude,
        corner_result.force_direction.x,
        corner_result.force_direction.y);

    // Expected: 3 neighbors (right, below, diagonal) → force points toward (1,1)
}

/**
 * @brief Test cohesion with offset COMs.
 */
TEST_F(CohesionDebugTest, OffsetCOMs)
{
    spdlog::info("=== Testing 3x3 Grid - Offset COMs ===");

    world = std::make_unique<World>(3, 3);
    world->getPhysicsSettings().cohesion_strength = 150.0;

    // Fill all cells with dirt.
    for (uint32_t y = 0; y < 3; y++) {
        for (uint32_t x = 0; x < 3; x++) {
            world->getData().at(x, y).replaceMaterial(MaterialType::DIRT, 1.0);
            world->getData().at(x, y).setCOM(0.0, 0.0);
        }
    }

    // Offset center cell's COM toward the right edge.
    world->getData().at(1, 1).setCOM(0.8, 0.0);

    WorldCohesionCalculator calc;
    auto result = calc.calculateCOMCohesionForce(*world, 1, 1, 1);

    spdlog::info(
        "Center cell with COM at (0.8, 0.0): force_mag={:.4f}, direction=({:.3f},{:.3f})",
        result.force_magnitude,
        result.force_direction.x,
        result.force_direction.y);

    // Expected: Force should pull LEFT (negative x) to recenter toward neighbor average.
    EXPECT_LT(result.force_direction.x, 0.0) << "Force should pull left to recenter offset COM";
}

/**
 * @brief Test directional correction - cohesion reduced when velocity already aligned.
 */
TEST_F(CohesionDebugTest, DirectionalCorrection)
{
    spdlog::info("=== Testing Directional Correction ===");

    world = std::make_unique<World>(3, 1);
    world->getPhysicsSettings().cohesion_strength = 150.0;

    // Setup: Left-Center-Right dirt cells.
    world->getData().at(0, 0).replaceMaterial(MaterialType::DIRT, 1.0);
    world->getData().at(1, 0).replaceMaterial(MaterialType::DIRT, 1.0);
    world->getData().at(2, 0).replaceMaterial(MaterialType::DIRT, 1.0);

    // All COMs centered initially.
    world->getData().at(0, 0).setCOM(0.0, 0.0);
    world->getData().at(1, 0).setCOM(0.0, 0.0);
    world->getData().at(2, 0).setCOM(0.0, 0.0);

    // Calculate cohesion without velocity (baseline).
    WorldCohesionCalculator calc;
    auto baseline = calc.calculateCOMCohesionForce(*world, 1, 0, 1);
    spdlog::info(
        "Baseline (velocity=0): force_mag={:.4f}, direction=({:.4f},{:.4f})",
        baseline.force_magnitude,
        baseline.force_direction.x,
        baseline.force_direction.y);

    // Now give center cell velocity toward the right.
    world->getData().at(1, 0).velocity = Vector2d(1.0, 0.0);

    // Recalculate (same result - calculator doesn't see velocity).
    auto with_velocity = calc.calculateCOMCohesionForce(*world, 1, 0, 1);
    spdlog::info(
        "With rightward velocity: force_mag={:.4f}, direction=({:.4f},{:.4f})",
        with_velocity.force_magnitude,
        with_velocity.force_direction.x,
        with_velocity.force_direction.y);

    spdlog::info(
        "Note: Directional correction is applied in World.applyCohesionForces() based on "
        "velocity alignment. The calculator itself doesn't modify forces based on velocity.");
}

/**
 * @brief Test alignment gating - clustering only applied when it helps centering.
 */
TEST_F(CohesionDebugTest, AlignmentGating)
{
    spdlog::info("=== Testing Alignment Gating ===");

    // Create 1x3 horizontal row.
    world = std::make_unique<World>(3, 1);
    world->getPhysicsSettings().cohesion_strength = 3.0; // Lower strength for testing.
    spdlog::set_level(spdlog::level::trace);             // Enable trace logging.

    // All dirt cells.
    world->getData().at(0, 0).replaceMaterial(MaterialType::DIRT, 1.0);
    world->getData().at(1, 0).replaceMaterial(MaterialType::DIRT, 1.0);
    world->getData().at(2, 0).replaceMaterial(MaterialType::DIRT, 1.0);

    // Test Case A: COM offset TOWARD neighbors (should skip clustering).
    spdlog::info("--- Case A: COM offset toward neighbors (clustering opposes centering) ---");

    world->getData().at(0, 0).setCOM(0.0, 0.0); // Left neighbor centered.
    world->getData().at(1, 0).setCOM(0.6, 0.0); // Center cell COM shifted RIGHT.
    world->getData().at(2, 0).setCOM(
        0.8, 0.0); // Right neighbor COM also shifted RIGHT (creates pull RIGHT).

    WorldCohesionCalculator calc;
    auto case_a = calc.calculateCOMCohesionForce(*world, 1, 0, 1);

    spdlog::info(
        "Center cell, COM at (+0.6, 0): force_mag={:.4f}, direction=({:.3f},{:.3f})",
        case_a.force_magnitude,
        case_a.force_direction.x,
        case_a.force_direction.y);

    spdlog::info("Expected: Mostly centering (LEFT), clustering skipped because it pulls RIGHT");

    // Test Case B: COM offset AWAY from neighbors (should apply clustering).
    spdlog::info("--- Case B: COM offset away from neighbors (clustering helps centering) ---");

    // Only RIGHT neighbor exists (left cell is AIR).
    world->getData().at(0, 0).replaceMaterial(MaterialType::AIR, 0.0); // No left neighbor.
    world->getData().at(1, 0).setCOM(
        -0.6, 0.0); // Center cell COM shifted LEFT (away from right neighbor).
    world->getData().at(2, 0).setCOM(0.0, 0.0); // Right neighbor centered.

    auto case_b = calc.calculateCOMCohesionForce(*world, 1, 0, 1);

    spdlog::info(
        "Center cell, COM at (-0.6, 0): force_mag={:.4f}, direction=({:.3f},{:.3f})",
        case_b.force_magnitude,
        case_b.force_direction.x,
        case_b.force_direction.y);

    spdlog::info("Expected: Centering (RIGHT) + clustering boost (both pull RIGHT)");

    // Case B should have stronger force due to clustering boost.
    EXPECT_GT(case_b.force_magnitude, case_a.force_magnitude)
        << "Aligned clustering should boost force";

    double boost_ratio = case_b.force_magnitude / case_a.force_magnitude;
    spdlog::info("Force boost when aligned: {:.2f}×", boost_ratio);
}
